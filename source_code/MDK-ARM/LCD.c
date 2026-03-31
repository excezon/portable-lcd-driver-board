#include "lcd.h"
#include "font.h"
#include "spi.h"
#include <string.h>

/* -------------------------- 全局变量定义 -------------------------- */
uint16_t POINT_COLOR = 0x0000;
uint16_t BACK_COLOR  = 0xFFFF;
_lcd_dev lcddev;

/* -------------------------- 优化延时（适配LL库）-------------------------- */
void opt_delay(uint8_t i)
{
    while(i--);
}

/* -------------------------- 微秒延时（适配84MHz主频LL库）-------------------------- */
void delay_us(uint32_t us)
{
    uint32_t ticks = (SystemCoreClock / 1000000) * us;
    uint32_t start = SysTick->VAL;
    while((start - SysTick->VAL + (1 << 24)) % (1 << 24) < ticks);
}

/* ===================== 事务与大块发送 ===================== */
void LCD_Select(void)   { SPI_CS_CLR(); }
void LCD_Unselect(void) { SPI_CS_SET(); }
void LCD_DC_Command(void){ SPI_DC_CLR(); }
void LCD_DC_Data(void)   { SPI_DC_SET(); }

/* 单字节命令发送（不自动抬CS，便于组合事务） */
static inline void LCD_TxCmd8(uint8_t cmd)
{
    LCD_DC_Command();
    while(!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, cmd);
    while(!LL_SPI_IsActiveFlag_RXNE(SPI1));
    (void)LL_SPI_ReceiveData8(SPI1);
}

/* 单字节数据发送（不自动抬CS） */
static inline void LCD_TxData8(uint8_t dat)
{
    LCD_DC_Data();
    while(!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, dat);
    while(!LL_SPI_IsActiveFlag_RXNE(SPI1));
    (void)LL_SPI_ReceiveData8(SPI1);
}

/* 发送一段数据流（字节流） */
static inline void LCD_TxDataBytes(const uint8_t *buf, uint16_t len)
{
    LCD_DC_Data();
    for(uint16_t i = 0; i < len; i++)
    {
        while(!LL_SPI_IsActiveFlag_TXE(SPI1));
        LL_SPI_TransmitData8(SPI1, buf[i]);
        while(!LL_SPI_IsActiveFlag_RXNE(SPI1));
        (void)LL_SPI_ReceiveData8(SPI1);
    }
}

/* 开始一次GRAM写入事务：窗口已设置后调用 */
static inline void LCD_BeginWriteRAM(void)
{
    LCD_Select();
    LCD_TxCmd8((uint8_t)lcddev.wramcmd);
    LCD_DC_Data();
}

/* 结束GRAM写入事务 */
static inline void LCD_EndWriteRAM(void)
{
    LCD_Unselect();
}

void LCD_PushColors_u16(const uint16_t *colors, uint32_t count)
{
    enum { PIXELS_PER_CHUNK = 256 };
    static uint8_t txbuf[PIXELS_PER_CHUNK * 2];

    while (count)
    {
        uint32_t n = (count > PIXELS_PER_CHUNK) ? PIXELS_PER_CHUNK : count;

        for (uint32_t i = 0; i < n; i++)
        {
            uint16_t c = colors[i];
            txbuf[2*i]   = (uint8_t)(c >> 8);
            txbuf[2*i+1] = (uint8_t)(c & 0xFF);
        }

        LCD_TxDataBytes(txbuf, (uint16_t)(n * 2));
        colors += n;
        count  -= n;
    }
}

void LCD_PushColorRepeat(uint16_t color, uint32_t count)
{
    enum { PIXELS_PER_CHUNK = 512 };
    static uint8_t txbuf[PIXELS_PER_CHUNK * 2];

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    for (uint32_t i = 0; i < PIXELS_PER_CHUNK; i++)
    {
        txbuf[2*i]   = hi;
        txbuf[2*i+1] = lo;
    }

    while (count)
    {
        uint32_t n = (count > PIXELS_PER_CHUNK) ? PIXELS_PER_CHUNK : count;
        LCD_TxDataBytes(txbuf, (uint16_t)(n * 2));
        count -= n;
    }
}

/* -------------------------- 写LCD寄存器（硬件SPI传输）-------------------------- */
void LCD_WR_REG(uint16_t data)
{
    uint8_t cmd = (uint8_t)data;
    LCD_Select();
    LCD_TxCmd8(cmd);
    LCD_Unselect();
}

/* -------------------------- 写8位数据（硬件SPI传输）-------------------------- */
void LCD_WR_DATA(uint16_t data)
{
    uint8_t dat = (uint8_t)data;
    LCD_Select();
    LCD_TxData8(dat);
    LCD_Unselect();
}

/* -------------------------- 写寄存器+数据（硬件SPI）-------------------------- */
void LCD_WriteReg(uint16_t reg, uint16_t val)
{
    LCD_WR_REG(reg);
    LCD_WriteRAM(val);
}

/* -------------------------- 准备写GRAM -------------------------- */
void LCD_WriteRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.wramcmd);
}

/* -------------------------- 写16位GRAM数据（RGB565）-------------------------- */
void LCD_WriteRAM(uint16_t color)
{
    uint8_t dat[2];
    dat[0] = (color >> 8) & 0xFF;
    dat[1] = color & 0xFF;
    SPI_CS_CLR();
    SPI_DC_SET();
    while(!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, dat[0]);
    while(!LL_SPI_IsActiveFlag_RXNE(SPI1));
    (void)LL_SPI_ReceiveData8(SPI1);
    while(!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData8(SPI1, dat[1]);
    while(!LL_SPI_IsActiveFlag_RXNE(SPI1));
    (void)LL_SPI_ReceiveData8(SPI1);
    SPI_CS_SET();
}

void LCD_DisplayOn(void)  { LCD_WR_REG(0X29); }
void LCD_DisplayOff(void) { LCD_WR_REG(0X28); }

void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(x >> 8);
    LCD_WR_DATA(x & 0XFF);
    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(y >> 8);
    LCD_WR_DATA(y & 0XFF);
}

void LCD_DrawPoint(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    LCD_WriteRAM_Prepare();
    LCD_WriteRAM(POINT_COLOR);
}

void LCD_Fast_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetCursor(x, y);
    LCD_WriteReg(lcddev.wramcmd, color);
}

void LCD_Display_Dir(uint8_t dir)
{
    if(dir == 0 || dir == 1)
    {
        lcddev.dir = 0;
        lcddev.width = 240;
        lcddev.height = 320;
        lcddev.wramcmd = 0X2C;
        lcddev.setxcmd = 0X2A;
        lcddev.setycmd = 0X2B;

        LCD_WR_REG(0x36);
        if(dir == 0) LCD_WR_DATA((0 << 3) | (0 << 7) | (0 << 6) | (0 << 5));
        else         LCD_WR_DATA((0 << 3) | (1 << 7) | (1 << 6) | (0 << 5));
    }
    else if(dir == 2 || dir == 3)
    {
        lcddev.dir = 1;
        lcddev.width = 320;
        lcddev.height = 240;
        lcddev.wramcmd = 0X2C;
        lcddev.setxcmd = 0X2A;
        lcddev.setycmd = 0X2B;

        LCD_WR_REG(0x36);
        if(dir == 2) LCD_WR_DATA((0 << 3) | (1 << 7) | (0 << 6) | (1 << 5));
        else         LCD_WR_DATA((0 << 3) | (0 << 7) | (1 << 6) | (1 << 5));
    }

    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(0);
    LCD_WR_DATA(0);
    LCD_WR_DATA((lcddev.width - 1) >> 8);
    LCD_WR_DATA((lcddev.width - 1) & 0XFF);
    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(0);
    LCD_WR_DATA(0);
    LCD_WR_DATA((lcddev.height - 1) >> 8);
    LCD_WR_DATA((lcddev.height - 1) & 0XFF);
}

void LCD_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth = sx + width - 1;
    uint16_t theight = sy + height - 1;

    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(sx >> 8);
    LCD_WR_DATA(sx & 0XFF);
    LCD_WR_DATA(twidth >> 8);
    LCD_WR_DATA(twidth & 0XFF);
    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(sy >> 8);
    LCD_WR_DATA(sy & 0XFF);
    LCD_WR_DATA(theight >> 8);
    LCD_WR_DATA(theight & 0XFF);
}

// 第一个代码版本 - 稳定但刷新慢
void LCD_Init(void)
{			
    SPI_RST_SET();
    delay_us(1000); // 1ms
    SPI_RST_CLR();
    delay_us(10000); // 10ms
    SPI_RST_SET();
    delay_us(120000); // 120ms
		
    LCD_WR_REG(0x11);
    delay_us(120000); // 120ms

    LCD_WR_REG(0x36);
    LCD_WR_DATA(0x00);
    LCD_WR_REG(0x3a);
    LCD_WR_DATA(0x05);

    LCD_WR_REG(0xb2);
    LCD_WR_DATA(0x0c);
    LCD_WR_DATA(0x0c);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x33);
    LCD_WR_DATA(0x33);
    LCD_WR_REG(0xb7);
    LCD_WR_DATA(0x35);

    LCD_WR_REG(0xbb);
    LCD_WR_DATA(0x28);
    LCD_WR_REG(0xc0);
    LCD_WR_DATA(0x2c);
    LCD_WR_REG(0xc2);
    LCD_WR_DATA(0x01);
    LCD_WR_REG(0xc3);
    LCD_WR_DATA(0x0b);
    LCD_WR_REG(0xc4);
    LCD_WR_DATA(0x20);
    LCD_WR_REG(0xc6);
    LCD_WR_DATA(0x0f);
    LCD_WR_REG(0xd0);
    LCD_WR_DATA(0xa4);
    LCD_WR_DATA(0xa1);

    LCD_WR_REG(0xe0);
    LCD_WR_DATA(0xd0);
    LCD_WR_DATA(0x01);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x0f);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x2a);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x55);
    LCD_WR_DATA(0x44);
    LCD_WR_DATA(0x3a);
    LCD_WR_DATA(0x0b);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x20);
    LCD_WR_REG(0xe1);
    LCD_WR_DATA(0xd0);
    LCD_WR_DATA(0x02);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x0a);
    LCD_WR_DATA(0x0b);
    LCD_WR_DATA(0x18);
    LCD_WR_DATA(0x34);
    LCD_WR_DATA(0x43);
    LCD_WR_DATA(0x4a);
    LCD_WR_DATA(0x2b);
    LCD_WR_DATA(0x1b);
    LCD_WR_DATA(0x1c);
    LCD_WR_DATA(0x22);
    LCD_WR_DATA(0x1f);

    LCD_WR_REG(0x29);
    LCD_Set_Window(0,0,240,320);   // 你的屏一般 240x320
    LCD_WR_REG(0x2C);              // Memory Write
    // 连续写一些像素数据，比如写 1000 个红色点（RGB565=0xF800）
    for(int i=0;i<1000;i++){
        LCD_WR_DATA(0xF8); // 高字节
        LCD_WR_DATA(0x00); // 低字节
    }
    LCD_Display_Dir(USE_LCM_DIR);
    lcddev.id = 0X7789;
    LCD_Clear(WHITE);
}

// -------------------------- 清屏 --------------------------
void LCD_Clear(uint16_t color)
{
    /* 设置全屏窗口 */
    LCD_Set_Window(0, 0, lcddev.width, lcddev.height);
    /* 开始写GRAM事务：CS保持低，DC=1连续发 */
    LCD_BeginWriteRAM();
    uint32_t total = (uint32_t)lcddev.width * (uint32_t)lcddev.height;
    LCD_PushColorRepeat(color, total);
    LCD_EndWriteRAM();
}

// -------------------------- 填充指定区域 --------------------------
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    if (ex < sx || ey < sy) return;

    uint16_t w = ex - sx + 1;
    uint16_t h = ey - sy + 1;

    LCD_Set_Window(sx, sy, w, h);

    LCD_BeginWriteRAM();
    LCD_PushColorRepeat(color, (uint32_t)w * (uint32_t)h);
    LCD_EndWriteRAM();

    /* 可选：恢复全屏窗口（你原本很多地方会恢复）
       LCD_Set_Window(0, 0, lcddev.width, lcddev.height); */
}

// -------------------------- 填充颜色数组 --------------------------
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    if (ex < sx || ey < sy) return;

    uint16_t w = ex - sx + 1;
    uint16_t h = ey - sy + 1;

    LCD_Set_Window(sx, sy, w, h);

    LCD_BeginWriteRAM();
    LCD_PushColors_u16(color, (uint32_t)w * (uint32_t)h);
    LCD_EndWriteRAM();
}

// -------------------------- 显示单个ASCII字符 --------------------------
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    num -= ' ';  // 偏移量（字库从空格开始）

    for(t = 0; t < csize; t++)
    {
        if(size == 12) temp = asc2_1206[num][t];
        else if(size == 16) temp = asc2_1608[num][t];
        else if(size == 24) temp = asc2_2412[num][t];
        else return;

        for(t1 = 0; t1 < 8; t1++)
        {
            if(temp & 0x80) LCD_Fast_DrawPoint(x, y, POINT_COLOR);
            else if(mode == 0) LCD_Fast_DrawPoint(x, y, BACK_COLOR);

            temp <<= 1;
            y++;
            if(y >= lcddev.height) return;
            if((y - y0) == size)
            {
                y = y0;
                x++;
                if(x >= lcddev.width) return;
                break;
            }
        }
    }
}

// -------------------------- 幂函数（数字显示用）--------------------------
uint32_t LCD_Pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while(n--) result *= m;
    return result;
}

// -------------------------- 显示数字（高位不补0）--------------------------
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for(t = 0; t < len; t++)
    {
        temp = (num / LCD_Pow(10, len - t - 1)) % 10;
        if(enshow == 0 && t < (len - 1))
        {
            if(temp == 0)
            {
                LCD_ShowChar(x + (size / 2) * t, y, ' ', size, 0);
                continue;
            }
            else enshow = 1;
        }
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, 0);
    }
}

// -------------------------- 显示数字（可补0）--------------------------
void LCD_ShowxNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for(t = 0; t < len; t++)
    {
        temp = (num / LCD_Pow(10, len - t - 1)) % 10;
        if(enshow == 0 && t < (len - 1))
        {
            if(temp == 0)
            {
                if(mode & 0X80) LCD_ShowChar(x + (size / 2) * t, y, '0', size, mode & 0X01);
                else LCD_ShowChar(x + (size / 2) * t, y, ' ', size, mode & 0X01);
                continue;
            }
            else enshow = 1;
        }
        LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, mode & 0X01);
    }
}

// -------------------------- 显示ASCII字符串 --------------------------
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t *p)
{
    uint8_t x0 = x;
    width += x;
    height += y;

    while((*p <= '~') && (*p >= ' '))
    {
        if(x >= width) { x = x0; y += size; }
        if(y >= height) break;
        LCD_ShowChar(x, y, *p, size, 0);
        x += size / 2;
        p++;
    }
}