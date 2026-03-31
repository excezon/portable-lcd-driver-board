#include "show.h"
#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "main.h"
#include "lcd.h"
#include "spi.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_tim.h"

// =================== 宏定义 ===================
#ifndef LCD_MAX_W
#define LCD_MAX_W  240
#endif
#ifndef LCD_MAX_H
#define LCD_MAX_H  320
#endif
#ifndef SHOW_PATH_MAX
#define SHOW_PATH_MAX  256
#endif
#ifndef BIN_HEADER_SECTOR_SIZE
#define BIN_HEADER_SECTOR_SIZE  512U
#endif

#define SPI_TIMEOUT  1000U
#define DMA_TIMEOUT  500U

// 双缓冲配置
#define DMA_BUFFER_SIZE 8192

// 基准分辨率和基准ARR值
#define BASE_WIDTH  180
#define BASE_HEIGHT 320
#define BASE_ARR    29999  // 基准10fps时的ARR值

// =================== 统计信息缓冲区 ===================
static char s1[32], s2[32], s3[32], s4[32];

// =================== 乒乓缓冲区核心变量 ===================
static uint8_t g_pingpong_buf[2][DMA_BUFFER_SIZE];
static volatile uint8_t  g_buf_state[2] = {0}; // 0=空闲,1=已读满,2=发送中
static volatile uint8_t  g_next_read_idx = 0;
static volatile uint8_t  g_next_tx_idx   = 0;
static volatile uint32_t g_bytes_left    = 0;
static volatile uint32_t g_tx_bytes_total= 0;
static FIL *g_fp = NULL;
static volatile uint8_t g_trans_err = 0;
static volatile uint8_t g_dma_busy  = 0;

// VEO(V565) 端序交换开关：1=交换payload里每像素2字节
static volatile uint8_t g_swap_bytes = 0;

// ====== DMA TX timing (benchmark) ======
volatile uint32_t g_tx_chunk_t0 = 0;
volatile uint32_t g_tx_time_acc = 0;

// =================== GPIO宏定义 ===================
#undef SPI_CS_CLR
#undef SPI_CS_SET
#undef SPI_DC_CLR
#undef SPI_DC_SET
#undef SPI_RST_CLR
#undef SPI_RST_SET
#define SPI_CS_CLR()  (GPIOA->ODR &= ~(1 << 8))
#define SPI_CS_SET()  (GPIOA->ODR |= (1 << 8))
#define SPI_DC_CLR()  (GPIOA->ODR &= ~(1 << 9))
#define SPI_DC_SET()  (GPIOA->ODR |= (1 << 9))
#define SPI_RST_CLR() (GPIOA->ODR &= ~(1 << 4))
#define SPI_RST_SET() (GPIOA->ODR |= (1 << 4))

// =================== TIM2 切帧(根据当前帧分辨率动态调整) ===================
extern TIM_HandleTypeDef htim2;
volatile uint8_t  g_frame_tick  = 0;
volatile uint32_t g_tim2_cb_cnt = 0;

// 计算基于分辨率的ARR值
static uint32_t calculate_arr_value(uint16_t width, uint16_t height)
{
    // 计算当前分辨率的像素总数
    uint32_t current_pixels = (uint32_t)width * height;
    
    // 计算基准分辨率的像素总数
    uint32_t base_pixels = (uint32_t)BASE_WIDTH * BASE_HEIGHT;
    
    // 计算像素比例（基准分辨率是当前分辨率的多少倍）
    float pixel_ratio = (float)base_pixels / (float)current_pixels;
    
    // 基于比例计算ARR值：分辨率越小，ARR值越小（帧率越高）
    // 例如：如果当前分辨率是基准的一半（像素数少一半），则ARR值应为基准的一半（帧率高一倍）
    float arr_ratio = pixel_ratio;
    
    if (arr_ratio <= 0.0f) {
        arr_ratio = 1.0f; // 防止除零
    }
    
    uint32_t arr_value = (uint32_t)((float)BASE_ARR / arr_ratio);
    
    // 限制ARR值在合理范围内（1到65535之间）
    if (arr_value > BASE_ARR) {
        arr_value = BASE_ARR; // 最大不超过基准值
    } else if (arr_value < 1) {
        arr_value = 1;
    }
    
    return arr_value;
}

// !!! 关键：必须置位 g_frame_tick，否则 while(!g_frame_tick) 永远出不来
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        g_tim2_cb_cnt++;
        g_frame_tick = 1;
    }
}

// TIM2: 运行时强制为 1MHz tick + 动态ARR值（根据当前帧分辨率）
static inline void tim2_stop(void)
{
    HAL_TIM_Base_Stop_IT(&htim2);
    __HAL_TIM_DISABLE(&htim2);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);
}

static inline void tim2_start_with_resolution(uint16_t width, uint16_t height)
{
    g_frame_tick = 0;

    tim2_stop();

    // 根据当前帧分辨率计算ARR值
    uint32_t calculated_arr = calculate_arr_value(width, height);

    // 强制配置：TIMCLK=84MHz => PSC=83 => 1MHz(1us/tick)
    __HAL_TIM_DISABLE(&htim2);
    __HAL_TIM_SET_PRESCALER(&htim2, 83);
    __HAL_TIM_SET_AUTORELOAD(&htim2, calculated_arr);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    // 关键：强制更新(UG)，把 PSC/ARR 立即装载到影子寄存器
    TIM2->EGR = TIM_EGR_UG;

    // 清 UIF / 清 UIE pending
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);

    HAL_TIM_Base_Start_IT(&htim2);
}

// =================== DMA初始化函数 ===================
static void SPI1_DMA_Init(void)
{
    // 启用DMA2时钟
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    
    // --- 配置 DMA2 Stream5 ---
    LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
    while (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_5));

    // 设置通道和传输方向
    LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_5, LL_DMA_CHANNEL_3); // SPI1_TX
    LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_5, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_5, (uint32_t)&SPI1->DR); // 关键：设置外设地址
    LL_DMA_SetMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_5, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_5, LL_DMA_MDATAALIGN_BYTE);
    
    // 设置高优先级
    DMA2_Stream5->CR &= ~DMA_SxCR_PL;  // 清除优先级位
    DMA2_Stream5->CR |= LL_DMA_PRIORITY_VERYHIGH << DMA_SxCR_PL_Pos;  // 设置高优先级

    // 启用传输完成中断
    LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_5);
    LL_DMA_EnableIT_TE(DMA2, LL_DMA_STREAM_5);
}

// =================== SPI单字节发送（指令用） ===================
static uint8_t SPI1_Transmit_Byte_Timeout(uint8_t data)
{
    uint32_t timeout = SPI_TIMEOUT;
    while(!LL_SPI_IsActiveFlag_TXE(SPI1) && timeout--) __NOP();
    if (timeout == 0) return 1;

    LL_SPI_TransmitData8(SPI1, data);

    timeout = SPI_TIMEOUT;
    while(LL_SPI_IsActiveFlag_BSY(SPI1) && timeout--) __NOP();
    if (timeout == 0) return 1;

    (void)LL_SPI_ReceiveData8(SPI1);
    return 0;
}

// ==================== 异步DMA发送（不阻塞CPU） ====================
static uint8_t SPI1_Transmit_Buffer_DMA_Async(const uint8_t *pbuf, uint16_t len)
{
    if (len == 0 || pbuf == NULL || g_dma_busy) return 1;

    // 等待上一次传输完成
    while (g_dma_busy) {}

    g_dma_busy = 1;

    // 初始化DMA（仅第一次调用）
    static volatile uint8_t g_dma_initialized = 0;
    if (!g_dma_initialized) {
        SPI1_DMA_Init();
        g_dma_initialized = 1;
    }

    // --- 仅更新必要的寄存器 ---
    LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
    while (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_5));

    // 更新内存地址和数据长度
    LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_5, (uint32_t)pbuf);
    LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_5, len);

    // 清除相关标志
    LL_DMA_ClearFlag_TC5(DMA2);
    LL_DMA_ClearFlag_HT5(DMA2);
    LL_DMA_ClearFlag_TE5(DMA2);
    LL_DMA_ClearFlag_DME5(DMA2);
    LL_DMA_ClearFlag_FE5(DMA2);

    // 启动 DMA
    LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_5);

    // 使能 SPI 的 TX DMA 请求
    LL_SPI_EnableDMAReq_TX(SPI1);

    return 0;
}

// ==================== DMA回调（非阻塞切换缓冲区） ====================
void DMA2_Stream5_TransferComplete_Callback(void)
{
    LL_SPI_DisableDMAReq_TX(SPI1); // 关闭SPI的DMA请求
    
    uint32_t now = HAL_GetTick();
    g_tx_time_acc += (now - g_tx_chunk_t0);

    // 标记当前发送缓冲区为空闲
    g_buf_state[g_next_tx_idx] = 0;
    // 切换下一个发送索引
    g_next_tx_idx = (g_next_tx_idx + 1) % 2;

    g_dma_busy = 0;
}

// ==================== 非阻塞读取缓冲区 ====================
static uint32_t pingpong_read_buf_nonblock(void)
{
    if (g_fp == NULL || g_bytes_left == 0) return 0;
    if (g_buf_state[g_next_read_idx] != 0) return 0;

    UINT want = (g_bytes_left > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : (UINT)g_bytes_left;
    want = (want + 1) & ~1U;

    UINT br = 0;
    FRESULT res = f_read(g_fp, g_pingpong_buf[g_next_read_idx], want, &br);
    if (res != FR_OK || br == 0) {
        g_trans_err = 1;
        return 0;
    }

    g_bytes_left -= br;
    g_buf_state[g_next_read_idx] = 1;
    g_next_read_idx = (g_next_read_idx + 1) % 2;
    return br;
}

// =================== 硬Abort：不等DMA结束，强切帧 ===================
static void spi1_abort_dma_hard(void)
{
    LL_SPI_DisableDMAReq_TX(SPI1); // 关闭SPI的DMA请求

    LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_5);
    while (LL_DMA_IsEnabledStream(DMA2, LL_DMA_STREAM_5)) { }

    // 结束本次GRAM写
    SPI_CS_SET();
    SPI_DC_CLR();

    // 清状态，避免卡住
    g_dma_busy = 0;
    g_buf_state[0] = 0;
    g_buf_state[1] = 0;
}

// =================== 统计显示（保留） ===================
void ShowStat(uint32_t t_frame0, uint32_t t_read_total, uint32_t t_tx_total, uint32_t total_bytes)
{
    uint32_t t_all = HAL_GetTick() - t_frame0;
    snprintf(s1, sizeof(s1), "Read(ms): %lu", (unsigned long)t_read_total);
    snprintf(s2, sizeof(s2), "Tx(ms): %lu",   (unsigned long)t_tx_total);
    snprintf(s3, sizeof(s3), "All(ms): %lu",  (unsigned long)t_all);

    uint32_t kbps = 0;
    if (t_all > 0) {
        kbps = (uint32_t)(((uint64_t)total_bytes * 1000ULL) / (1024ULL * (uint64_t)t_all));
    }
    snprintf(s4, sizeof(s4), "Thru: %lu KB/s", (unsigned long)kbps);

    LCD_ShowString(0,  0, 240, 16, 16, (uint8_t*)s1);
    LCD_ShowString(0, 16, 240, 16, 16, (uint8_t*)s2);
    LCD_ShowString(0, 32, 240, 16, 16, (uint8_t*)s3);
    LCD_ShowString(0, 48, 240, 16, 16, (uint8_t*)s4);
}

// =================== 工具函数 ===================
static int ends_with_bin(const char *name)
{
    if (!name) return 0;
    size_t n = strlen(name);
    if (n < 4) return 0;
    const char *p = name + (n - 4);
    return ((p[0] == '.') && ((p[1] == 'b') || (p[1] == 'B')) &&
            ((p[2] == 'i') || (p[2] == 'I')) && ((p[3] == 'n') || (p[3] == 'N')));
}

static uint16_t rd_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int parse_header_512(const uint8_t header512[BIN_HEADER_SECTOR_SIZE],
                            uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint16_t *play_ms)
{
    if (!header512 || !x || !y || !w || !h || !play_ms) return -1;
    *x = rd_le16(&header512[0]);
    *y = rd_le16(&header512[2]);
    *w = rd_le16(&header512[4]);
    *h = rd_le16(&header512[6]);
    *play_ms = rd_le16(&header512[8]);
    return 0;
}

// ================================================================
// ===================== V565（veo）解析与播放 =======================
// 头512： "V565" + version + endian + fps_q8_8 + frame_count + disp/box/rot...
// 每帧： [16字节帧头] + [data_len字节RGB565] + pad到512边界
// ================================================================
#define V565_MAGIC0 'V'
#define V565_MAGIC1 '5'
#define V565_MAGIC2 '6'
#define V565_MAGIC3 '5'

typedef struct {
    uint8_t  magic[4];
    uint16_t version;
    uint8_t  endian_flag;   // 0=little,1=big（payload RGB565 byte order）
    uint8_t  reserved0;
    uint16_t fps_q8_8;      // fps*256
    uint16_t frame_count;
    uint16_t disp_w, disp_h;
    uint16_t box_w, box_h;
    uint16_t rot;
} V565Header;

typedef struct {
    uint16_t cw, ch;
    uint16_t dx, dy;
    uint16_t dur_ms;
    uint16_t reserved;
    uint32_t data_len;
} V565FrameHdr;

static int parse_v565_header_512(const uint8_t header512[BIN_HEADER_SECTOR_SIZE], V565Header *out)
{
    if (!header512 || !out) return -1;
    if (header512[0] != V565_MAGIC0 || header512[1] != V565_MAGIC1 ||
        header512[2] != V565_MAGIC2 || header512[3] != V565_MAGIC3) {
        return -2;
    }

    out->magic[0] = header512[0];
    out->magic[1] = header512[1];
    out->magic[2] = header512[2];
    out->magic[3] = header512[3];

    out->version     = rd_le16(&header512[4]);
    out->endian_flag = header512[6];
    out->reserved0   = header512[7];
    out->fps_q8_8    = rd_le16(&header512[8]);
    out->frame_count = rd_le16(&header512[10]);
    out->disp_w      = rd_le16(&header512[12]);
    out->disp_h      = rd_le16(&header512[14]);
    out->box_w       = rd_le16(&header512[16]);
    out->box_h       = rd_le16(&header512[18]);
    out->rot         = rd_le16(&header512[20]);

    if (out->version != 1) return -3;
    return 0;
}

static int read_v565_frame_hdr(FIL *fp, V565FrameHdr *fh)
{
    uint8_t b[16];
    UINT br = 0;
    if (!fp || !fh) return -1;
    if (f_read(fp, b, sizeof(b), &br) != FR_OK || br != sizeof(b)) return -2;

    fh->cw       = rd_le16(&b[0]);
    fh->ch       = rd_le16(&b[2]);
    fh->dx       = rd_le16(&b[4]);
    fh->dy       = rd_le16(&b[6]);
    fh->dur_ms   = rd_le16(&b[8]);
    fh->reserved = rd_le16(&b[10]);
    fh->data_len = rd_le32(&b[12]);
    return 0;
}

static int skip_to_next_sector(FIL *fp, uint32_t already_in_frame)
{
    uint32_t pad = (BIN_HEADER_SECTOR_SIZE - (already_in_frame % BIN_HEADER_SECTOR_SIZE)) % BIN_HEADER_SECTOR_SIZE;
    if (pad == 0) return 0;
    FSIZE_t cur = f_tell(fp);
    return (f_lseek(fp, cur + pad) == FR_OK) ? 0 : -1;
}

//设置LCD窗口 + 写GRAM（小封装）
static int lcd_set_window_and_wram(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint8_t x_data[4];
    uint8_t y_data[4];

    x_data[0] = (sx >> 8); x_data[1] = (sx & 0xFF);
    x_data[2] = (ex >> 8); x_data[3] = (ex & 0xFF);

    y_data[0] = (sy >> 8); y_data[1] = (sy & 0xFF);
    y_data[2] = (ey >> 8); y_data[3] = (ey & 0xFF);

    SPI_CS_CLR();

    // set X
    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setxcmd) != 0) { SPI_CS_SET(); return -1; }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(x_data, 4) != 0) { SPI_CS_SET(); return -2; }
    while (g_dma_busy);

    // set Y
    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setycmd) != 0) { SPI_CS_SET(); return -3; }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(y_data, 4) != 0) { SPI_CS_SET(); return -4; }
    while (g_dma_busy);

    // write GRAM
    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.wramcmd) != 0) { SPI_CS_SET(); return -5; }
    SPI_DC_SET();

    return 0;
}

// 快速填充屏幕函数
static void lcd_fill_screen_fast(const uint8_t *color_data, uint16_t color_size)
{
    uint32_t total_bytes = (uint32_t)lcddev.width * (uint32_t)lcddev.height * 2U;
    
    // 填充双缓冲区
    uint32_t bytes_per_buffer = DMA_BUFFER_SIZE;
    
    // 填充两个缓冲区
    for(int buf_idx = 0; buf_idx < 2; buf_idx++) {
        uint8_t *buffer = g_pingpong_buf[buf_idx];
        uint32_t copied = 0;
        
        while(copied < bytes_per_buffer) {
            uint16_t copy_size = (bytes_per_buffer - copied < color_size) ? 
                                (bytes_per_buffer - copied) : color_size;
            memcpy(buffer + copied, color_data, copy_size);
            copied += copy_size;
        }
    }
    
    // 启动DMA传输
    uint32_t remaining_bytes = total_bytes;
    uint8_t current_buf_idx = 0;
    
    while(remaining_bytes > 0) {
        uint32_t chunk_size = (remaining_bytes > DMA_BUFFER_SIZE) ? 
                             DMA_BUFFER_SIZE : remaining_bytes;
        
        SPI1_Transmit_Buffer_DMA_Async(g_pingpong_buf[current_buf_idx], chunk_size);
        while(g_dma_busy); // 等待完成
        
        remaining_bytes -= chunk_size;
        current_buf_idx = 1 - current_buf_idx; // 切换缓冲区
    }
}

/**
 * Show_Veo_From_Bin（基于当前帧分辨率的动态帧率）
 * - 每帧：设置窗口+WRAM -> 根据当前帧分辨率启动 TIM2 -> 边读边发DMA
 * - TIM2到点：不等DMA结束，硬Abort，文件指针跳下一帧
 */
void Show_Veo_From_Bin(const char *file_path)
{
    FIL fp;
    FRESULT res;
    UINT br = 0;

    uint8_t header512[BIN_HEADER_SECTOR_SIZE] = {0};
    V565Header vh;

    // 初始化
    memset((void*)g_buf_state, 0, sizeof(g_buf_state));
    g_next_read_idx   = 0;
    g_next_tx_idx     = 0;
    g_bytes_left      = 0;
    g_tx_bytes_total  = 0;
    g_trans_err       = 0;
    g_dma_busy        = 0;
    g_fp              = NULL;
    g_swap_bytes      = 0;

    tim2_stop();

    if (file_path == NULL || strlen(file_path) == 0) return;

    res = f_open(&fp, file_path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) { f_close(&fp); return; }
    g_fp = &fp;

    res = f_read(&fp, header512, BIN_HEADER_SECTOR_SIZE, &br);
    if (res != FR_OK || br != BIN_HEADER_SECTOR_SIZE) {
        f_close(&fp); g_fp = NULL; return;
    }

    if (parse_v565_header_512(header512, &vh) != 0) {
        f_close(&fp); g_fp = NULL; return;
    }

    // 端序：bin little -> swap；bin big -> 不swap（LCD通常高字节先发）
    g_swap_bytes = (vh.endian_flag == 0) ? 1 : 0;

    // 添加快速填充黑色屏幕
    uint8_t black_color[4] = {0x00, 0x00, 0x00, 0x00}; // BLACK = 0x0000
    if (lcd_set_window_and_wram(0, 0, lcddev.width - 1, lcddev.height - 1) == 0) {
        lcd_fill_screen_fast(black_color, 4);
        SPI_CS_SET();
        SPI_DC_CLR();
    }

    for (uint16_t fi = 0; fi < vh.frame_count; fi++) {

        V565FrameHdr fh;
        if (read_v565_frame_hdr(&fp, &fh) != 0) break;

        FSIZE_t payload_start = f_tell(&fp);

        // 空帧：只等100ms切下一帧
        if (fh.cw == 0 || fh.ch == 0 || fh.data_len == 0) {
            (void)f_lseek(&fp, payload_start + fh.data_len);
            (void)skip_to_next_sector(&fp, 16U + fh.data_len);

            // 根据当前帧分辨率计算帧率（即使空帧也要用当前帧的分辨率）
            tim2_start_with_resolution(fh.cw, fh.ch);
            while (!g_frame_tick) { }
            tim2_stop();
            continue;
        }

        // 窗口
        uint16_t sx = fh.dx;
        uint16_t sy = fh.dy;
        uint16_t ex = (uint16_t)(fh.dx + fh.cw - 1);
        uint16_t ey = (uint16_t)(fh.dy + fh.ch - 1);

        // 完全屏外：跳过payload + pad，再等100ms
        if (sx >= lcddev.width || sy >= lcddev.height) {
            (void)f_lseek(&fp, payload_start + fh.data_len);
            (void)skip_to_next_sector(&fp, 16U + fh.data_len);

            // 根据当前帧分辨率计算帧率
            tim2_start_with_resolution(fh.cw, fh.ch);
            while (!g_frame_tick) { }
            tim2_stop();
            continue;
        }

        if (ex >= lcddev.width)  ex = lcddev.width  - 1;
        if (ey >= lcddev.height) ey = lcddev.height - 1;

        uint16_t real_w = (uint16_t)(ex - sx + 1);
        uint16_t real_h = (uint16_t)(ey - sy + 1);
        uint32_t real_need = (uint32_t)real_w * (uint32_t)real_h * 2U;

        uint32_t send_total = fh.data_len;
        if (real_need < send_total) send_total = real_need;
        send_total &= ~1U;

        if (lcd_set_window_and_wram(sx, sy, ex, ey) != 0) {
            g_trans_err = 1;
            break;
        }

        // 开始传就计时：根据当前帧分辨率动态调整
        tim2_start_with_resolution(fh.cw, fh.ch);

        // 初始化本帧乒乓
        memset((void*)g_buf_state, 0, sizeof(g_buf_state));
        g_next_read_idx   = 0;
        g_next_tx_idx     = 0;
        g_tx_bytes_total  = 0;
        g_bytes_left      = send_total;
        g_dma_busy        = 0;
        g_trans_err       = 0;
        g_fp              = &fp;

        // 预读（到点就停）
        while (!g_frame_tick &&
               (g_buf_state[0] == 0 || g_buf_state[1] == 0) &&
               g_bytes_left > 0 && !g_trans_err) {
            pingpong_read_buf_nonblock();
        }

        // 边读边发：到点强切（不等DMA）
        while (!g_frame_tick && !g_trans_err) {

            if (!g_dma_busy && g_buf_state[g_next_tx_idx] == 1) {

                uint32_t remain = (send_total > g_tx_bytes_total) ? (send_total - g_tx_bytes_total) : 0;
                uint16_t send_len = (remain > DMA_BUFFER_SIZE) ? (uint16_t)DMA_BUFFER_SIZE : (uint16_t)remain;
                send_len &= ~1U;

                if (send_len != 0) {
                    if (SPI1_Transmit_Buffer_DMA_Async(g_pingpong_buf[g_next_tx_idx], send_len) == 0) {
                        g_buf_state[g_next_tx_idx] = 2;
                        g_tx_bytes_total += send_len;
                    }
                }
            }

            pingpong_read_buf_nonblock();
        }

        // 到点：强切（不等DMA）
        if (g_dma_busy) {
            spi1_abort_dma_hard();
        } else {
            SPI_CS_SET();
            SPI_DC_CLR();
        }

        tim2_stop();

        if (g_trans_err) break;

        // 文件指针必须跳过完整payload并对齐
        (void)f_lseek(&fp, payload_start + fh.data_len);
        (void)skip_to_next_sector(&fp, 16U + fh.data_len);
    }

    tim2_stop();
    SPI_CS_SET();
    SPI_DC_CLR();
    f_close(&fp);
    g_fp = NULL;
    g_swap_bytes = 0;
}

// ===================================================================
// ====================== 旧格式 Show_Image_From_Bin =================
// ===================================================================
void Show_Image_From_Bin(const char *file_path)
{
    FIL fp;
    FRESULT res;
    UINT br = 0;
    uint8_t header512[BIN_HEADER_SECTOR_SIZE] = {0};
    uint16_t start_x = 0, start_y = 0, img_width = 0, img_height = 0, play_ms = 0;
    uint8_t x_data[4] = {0}, y_data[4] = {0};
    uint32_t total_bytes = 0;

    g_swap_bytes = 0;

    memset((void*)g_buf_state, 0, sizeof(g_buf_state));
    g_next_read_idx = 0;
    g_next_tx_idx = 0;
    g_bytes_left = 0;
    g_tx_bytes_total = 0;
    g_trans_err = 0;
    g_dma_busy = 0;
    g_fp = NULL;

    if (file_path == NULL || strlen(file_path) == 0) return;

    res = f_open(&fp, file_path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) { f_close(&fp); return; }
    g_fp = &fp;

    res = f_read(&fp, header512, BIN_HEADER_SECTOR_SIZE, &br);
    if (res != FR_OK || br != BIN_HEADER_SECTOR_SIZE) { f_close(&fp); g_fp = NULL; return; }

    if (parse_header_512(header512, &start_x, &start_y, &img_width, &img_height, &play_ms) != 0) {
        f_close(&fp); g_fp = NULL; return;
    }

    // 强制全屏
    start_x = 0;
    start_y = 0;
    img_width  = lcddev.width;
    img_height = lcddev.height;

    x_data[0] = (start_x >> 8);
    x_data[1] = (start_x & 0xFF);
    x_data[2] = ((start_x + img_width - 1) >> 8);
    x_data[3] = ((start_x + img_width - 1) & 0xFF);

    y_data[0] = (start_y >> 8);
    y_data[1] = (start_y & 0xFF);
    y_data[2] = ((start_y + img_height - 1) >> 8);
    y_data[3] = ((start_y + img_height - 1) & 0xFF);

    SPI_CS_CLR();

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setxcmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(x_data, 4) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    while(g_dma_busy);

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setycmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(y_data, 4) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    while(g_dma_busy);

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.wramcmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();

    total_bytes = (uint32_t)img_width * img_height * 2U;
    g_bytes_left = total_bytes;

    while (g_buf_state[0] == 0 || g_buf_state[1] == 0) {
        pingpong_read_buf_nonblock();
    }

    while ((g_bytes_left > 0 || g_buf_state[0] == 1 || g_buf_state[1] == 1) && !g_trans_err) {
        if (!g_dma_busy && g_buf_state[g_next_tx_idx] == 1) {
            uint16_t send_len = (g_tx_bytes_total + DMA_BUFFER_SIZE) > total_bytes ?
                               (uint16_t)(total_bytes - g_tx_bytes_total) : (uint16_t)DMA_BUFFER_SIZE;
            send_len = (send_len + 1) & ~1U;

            if (SPI1_Transmit_Buffer_DMA_Async(g_pingpong_buf[g_next_tx_idx], send_len) == 0) {
                g_buf_state[g_next_tx_idx] = 2;
                g_tx_bytes_total += send_len;
            }
        }
        pingpong_read_buf_nonblock();
    }

    while(g_dma_busy && !g_trans_err);

    SPI_CS_SET();
    SPI_DC_CLR();
    f_close(&fp);
    g_fp = NULL;

    if (play_ms > 0) {
        HAL_Delay(play_ms);
    }
}

// ===================================================================
// ====================== Benchmark（可选） ===========================
// ===================================================================
void Show_Image_From_Bin_Benchmark(const char *file_path)
{
    FIL fp;
    FRESULT res;
    UINT br = 0;
    uint8_t header512[BIN_HEADER_SECTOR_SIZE] = {0};
    uint16_t start_x = 0, start_y = 0;
    uint16_t img_width = 0, img_height = 0;
    uint16_t play_ms = 0;

    uint8_t x_data[4] = {0};
    uint8_t y_data[4] = {0};
    uint32_t total_bytes = 0;

    uint32_t t_frame0 = HAL_GetTick();
    uint32_t t_read_total = 0;
    uint32_t t0;

    g_swap_bytes = 0;

    memset((void*)g_buf_state, 0, sizeof(g_buf_state));
    g_next_read_idx = 0;
    g_next_tx_idx = 0;
    g_bytes_left = 0;
    g_tx_bytes_total = 0;
    g_trans_err = 0;
    g_dma_busy = 0;
    g_fp = NULL;

    g_tx_time_acc = 0;
    g_tx_chunk_t0 = 0;

    if (file_path == NULL || strlen(file_path) == 0) return;

    res = f_open(&fp, file_path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) { f_close(&fp); return; }
    g_fp = &fp;

    res = f_read(&fp, header512, BIN_HEADER_SECTOR_SIZE, &br);
    if (res != FR_OK || br != BIN_HEADER_SECTOR_SIZE) { f_close(&fp); g_fp = NULL; return; }

    if (parse_header_512(header512, &start_x, &start_y, &img_width, &img_height, &play_ms) != 0) {
        f_close(&fp); g_fp = NULL; return;
    }

    start_x = 0;
    start_y = 0;
    img_width  = lcddev.width;
    img_height = lcddev.height;

    x_data[0] = (start_x >> 8);
    x_data[1] = (start_x & 0xFF);
    x_data[2] = ((start_x + img_width - 1) >> 8);
    x_data[3] = ((start_x + img_width - 1) & 0xFF);

    y_data[0] = (start_y >> 8);
    y_data[1] = (start_y & 0xFF);
    y_data[2] = ((start_y + img_height - 1) >> 8);
    y_data[3] = ((start_y + img_height - 1) & 0xFF);

    SPI_CS_CLR();

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setxcmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(x_data, 4) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    while (g_dma_busy);

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.setycmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();
    if (SPI1_Transmit_Buffer_DMA_Async(y_data, 4) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    while (g_dma_busy);

    SPI_DC_CLR();
    if (SPI1_Transmit_Byte_Timeout((uint8_t)lcddev.wramcmd) != 0) {
        SPI_CS_SET(); f_close(&fp); g_fp = NULL; return;
    }
    SPI_DC_SET();

    total_bytes  = (uint32_t)img_width * img_height * 2U;
    g_bytes_left = total_bytes;

    while (g_buf_state[0] == 0 || g_buf_state[1] == 0) {
        t0 = HAL_GetTick();
        pingpong_read_buf_nonblock();
        t_read_total += (HAL_GetTick() - t0);
    }

    while ((g_bytes_left > 0 || g_buf_state[0] == 1 || g_buf_state[1] == 1) && !g_trans_err) {

        if (!g_dma_busy && g_buf_state[g_next_tx_idx] == 1) {

            uint16_t send_len = (g_tx_bytes_total + DMA_BUFFER_SIZE) > total_bytes ?
                                (uint16_t)(total_bytes - g_tx_bytes_total) :
                                (uint16_t)DMA_BUFFER_SIZE;

            if (SPI1_Transmit_Buffer_DMA_Async(g_pingpong_buf[g_next_tx_idx], send_len) != 0) {
                g_trans_err = 1;
                break;
            }
            g_tx_bytes_total += send_len;
        }

        t0 = HAL_GetTick();
        pingpong_read_buf_nonblock();
        t_read_total += (HAL_GetTick() - t0);
    }

    while (g_dma_busy && !g_trans_err);

    SPI_CS_SET();
    SPI_DC_CLR();
    f_close(&fp);
    g_fp = NULL;

    ShowStat(t_frame0, t_read_total, g_tx_time_acc, total_bytes);

    if (play_ms > 0) {
        HAL_Delay(play_ms);
    }
}

// ===================================================================
// ====================== 目录播放/列目录（保持你原来） =================
// ===================================================================
void Play_Bin_Folder_Loop(const char *folder_path)
{
    if (folder_path == NULL) {
        LCD_ShowString(10, 100, 220, 16, 16, (uint8_t*)"Err: Folder NULL");
        return;
    }

    for (;;) {
        DIR dir;
        FILINFO fno;
        FRESULT res = f_opendir(&dir, folder_path);

        if (res != FR_OK) {
            char buf[64];
            snprintf(buf, sizeof(buf), "OpenDir Err: %d", res);
            LCD_ShowString(10, 100, 220, 16, 16, (uint8_t*)buf);
            HAL_Delay(1000);
            continue;
        }

        while (1) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK) {
                char buf[64];
                snprintf(buf, sizeof(buf), "ReadDir Err: %d", res);
                LCD_ShowString(10, 120, 220, 16, 16, (uint8_t*)buf);
                break;
            }

            if (fno.fname[0] == 0) break;
            if (fno.fattrib & AM_DIR) continue;
            if (!ends_with_bin(fno.fname)) continue;

            char fullpath[SHOW_PATH_MAX];
            size_t flen = strlen(folder_path);
            if (flen > 0 && (folder_path[flen - 1] == '/' || folder_path[flen - 1] == '\\')) {
                snprintf(fullpath, sizeof(fullpath), "%s%s", folder_path, fno.fname);
            } else {
                snprintf(fullpath, sizeof(fullpath), "%s/%s", folder_path, fno.fname);
            }

            LCD_Clear(WHITE);
            LCD_ShowString(10, 20, 220, 16, 16, (uint8_t*)"Playing:");
            LCD_ShowString(10, 40, 220, 16, 16, (uint8_t*)fullpath);

            // 自动识别 V565 / 旧格式
            {
                FIL t;
                uint8_t h[BIN_HEADER_SECTOR_SIZE];
                UINT r = 0;
                if (f_open(&t, fullpath, FA_READ | FA_OPEN_EXISTING) == FR_OK) {
                    if (f_read(&t, h, BIN_HEADER_SECTOR_SIZE, &r) == FR_OK && r == BIN_HEADER_SECTOR_SIZE &&
                        h[0] == V565_MAGIC0 && h[1] == V565_MAGIC1 && h[2] == V565_MAGIC2 && h[3] == V565_MAGIC3) {
                        f_close(&t);
                        Show_Veo_From_Bin(fullpath);
                    } else {
                        f_close(&t);
                        Show_Image_From_Bin(fullpath);
                    }
                } else {
                    Show_Image_From_Bin(fullpath);
                }
            }
        }

        f_closedir(&dir);
    }
}

void List_Folder_Files(const char *folder_path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint16_t lcd_y = 20;
    char show_buf[64];

    if (folder_path == NULL) {
        LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)"Err: 路径为空");
        return;
    }

    res = f_opendir(&dir, folder_path);
    if (res != FR_OK) {
        snprintf(show_buf, sizeof(show_buf), "打开目录失败: %d", res);
        LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)show_buf);
        return;
    }

    snprintf(show_buf, sizeof(show_buf), "目录: %s", folder_path);
    LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)show_buf);
    lcd_y += 20;

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            snprintf(show_buf, sizeof(show_buf), "读目录失败: %d", res);
            LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)show_buf);
            break;
        }

        if (fno.fname[0] == 0) {
            LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)"---------------- 遍历结束");
            break;
        }

        if (fno.fattrib & AM_DIR) {
            snprintf(show_buf, sizeof(show_buf), "[DIR]  %s", fno.fname);
        } else {
            snprintf(show_buf, sizeof(show_buf), "[FILE] %s", fno.fname);
        }
        LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)show_buf);
        lcd_y += 20;

        char full_path[SHOW_PATH_MAX];
        size_t path_len = strlen(folder_path);
        if (path_len > 0 && (folder_path[path_len-1] == '/' || folder_path[path_len-1] == '\\')) {
            snprintf(full_path, sizeof(full_path), "%s%s", folder_path, fno.fname);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, fno.fname);
        }
        LCD_ShowString(20, lcd_y, 220, 12, 12, (uint8_t*)full_path);
        lcd_y += 15;

        if (lcd_y > 300) {
            LCD_ShowString(10, lcd_y, 220, 16, 16, (uint8_t*)"... 未显示完（屏幕空间不足）");
            break;
        }
    }

    f_closedir(&dir);
}

void Test_Dir_Open(const char *folder_path)
{
    DIR dir;
    FRESULT res;
    uint16_t y = 100;
    char buf[64];

    LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)"=== Dir Test Start ===");
    y += 25;
    snprintf(buf, sizeof(buf), "Test Path: %s", folder_path);
    LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
    y += 25;

    res = f_mount(&SDFatFS, folder_path, 1);
    if (res != FR_OK) {
        snprintf(buf, sizeof(buf), "Mount Err: %d", res);
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
        if (res == FR_DISK_ERR) {
            LCD_ShowString(10, y+20, 220, 16, 16, (uint8_t*)"原因：TF卡硬件/格式错误");
        } else if (res == FR_NOT_ENABLED) {
            LCD_ShowString(10, y+20, 220, 16, 16, (uint8_t*)"原因：卷未挂载");
        }
        return;
    }
    LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)"Mount: OK");
    y += 25;

    DWORD free_clusters;
    FATFS *fs;
    res = f_getfree(folder_path, &free_clusters, &fs);
    if (res != FR_OK) {
        snprintf(buf, sizeof(buf), "GetFree Err: %d", res);
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
        return;
    }
    uint32_t free_space = (free_clusters * fs->csize * 512) / 1024 / 1024;
    snprintf(buf, sizeof(buf), "Free: %lu MB", (unsigned long)free_space);
    LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
    y += 25;

    res = f_opendir(&dir, folder_path);
    if (res != FR_OK) {
        snprintf(buf, sizeof(buf), "OpenDir Err: %d", res);
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
        if (res == FR_DISK_ERR) {
            LCD_ShowString(10, y+20, 220, 16, 16, (uint8_t*)"原因：TF卡格式/目录损坏");
            LCD_ShowString(10, y+40, 220, 16, 16, (uint8_t*)"解决方案：FAT32格式化");
        } else if (res == FR_NO_PATH) {
            LCD_ShowString(10, y+20, 220, 16, 16, (uint8_t*)"原因：路径不存在");
        }
        return;
    }

    LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)"OpenDir: OK");
    y += 25;

    FILINFO fno;
    res = f_readdir(&dir, &fno);
    if (res != FR_OK) {
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)"ReadDir Err");
    } else if (fno.fname[0] == 0) {
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)"Dir Empty: 无文件");
    } else {
        snprintf(buf, sizeof(buf), "Found: %s", fno.fname);
        LCD_ShowString(10, y, 220, 16, 16, (uint8_t*)buf);
    }

    f_closedir(&dir);
}

void Show_MaxFPS_Test_SimTF_Fast(uint32_t frame_limit)
{
    static uint8_t color_buffer[4]; // 两个像素的颜色数据
    static uint8_t color_buffer_alt[4]; // 另外两个像素的颜色数据
    static uint8_t inited = 0;

    if (!inited)
    {
        // 定义两个 RGB565 颜色：A 和 B
        uint16_t colorA = 0xF800; // Red
        uint16_t colorB = 0x07E0; // Green
        
        // 填充颜色缓冲区（两个像素）
        color_buffer[0] = (colorA >> 8);
        color_buffer[1] = (colorA & 0xFF);
        color_buffer[2] = (colorA >> 8);
        color_buffer[3] = (colorA & 0xFF);
        
        color_buffer_alt[0] = (colorB >> 8);
        color_buffer_alt[1] = (colorB & 0xFF);
        color_buffer_alt[2] = (colorB >> 8);
        color_buffer_alt[3] = (colorB & 0xFF);
        
        inited = 1;
    }

    uint32_t frame = 0;
    uint32_t total_bytes = (uint32_t)lcddev.width * (uint32_t)lcddev.height * 2U;

    while (1)
    {
        // 选择是填充颜色 A 还是颜色 B
        const uint8_t *color_to_use = (frame & 1) ? color_buffer_alt : color_buffer;

        // 设置全屏窗口 + WRAM
        if (lcd_set_window_and_wram(0, 0, lcddev.width - 1, lcddev.height - 1) != 0)
        {
            SPI_CS_SET();
            SPI_DC_CLR();
            return;
        }

        // 快速填充屏幕
        lcd_fill_screen_fast(color_to_use, 4);

        // 结束GRAM写
        SPI_CS_SET();
        SPI_DC_CLR();

        frame++;
        if (frame_limit && frame >= frame_limit) break;
    }
}
