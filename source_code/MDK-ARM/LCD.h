#ifndef __LCD_H
#define __LCD_H

// 1. 替换为F4系列HAL库头文件（原F1头文件删除）
#include "stm32f4xx_hal.h"
#include "stdlib.h"
#include "string.h"

// 2. 重新定义引脚宏（严格对应新接口：PA3=RST, PA8=CS, PA9=DC）
// -------------------------- 引脚宏定义（GPIOA口）--------------------------
#define SPI1_SCK_PIN    GPIO_PIN_5    // SPI1_SCK（硬件引脚：PA5）
#define SPI1_MOSI_PIN   GPIO_PIN_7    // SPI1_MOSI（硬件引脚：PA7）
#define SPI1_MISO_PIN   GPIO_PIN_6    // SPI1_MISO（硬件引脚：PA6）
#define SPI_CS_PIN      GPIO_PIN_8    // 片选（GPIO控制：PA8，原PA4）
#define SPI_DC_PIN      GPIO_PIN_9    // 命令/数据（GPIO控制：PA9，原PA3）
#define SPI_RST_PIN     GPIO_PIN_4    // 复位（GPIO控制：PA3，原PA2）
// 注：新接口未提供LED背光引脚，删除LED相关定义
#define LCD_LED_PIN     GPIO_PIN_1    // 移除：无LED引脚
#define LCD_LED_PORT		GPIOA

// 3. 控制引脚操作宏（跟随引脚修改自动适配）
// -------------------------- 控制引脚操作宏 --------------------------
#define SPI_CS_CLR()    HAL_GPIO_WritePin(GPIOA, SPI_CS_PIN, GPIO_PIN_RESET)
#define SPI_CS_SET()    HAL_GPIO_WritePin(GPIOA, SPI_CS_PIN, GPIO_PIN_SET)
#define SPI_DC_CLR()    HAL_GPIO_WritePin(GPIOA, SPI_DC_PIN, GPIO_PIN_RESET)
#define SPI_DC_SET()    HAL_GPIO_WritePin(GPIOA, SPI_DC_PIN, GPIO_PIN_SET)
#define SPI_RST_CLR()   HAL_GPIO_WritePin(GPIOA, SPI_RST_PIN, GPIO_PIN_RESET)
#define SPI_RST_SET()   HAL_GPIO_WritePin(GPIOA, SPI_RST_PIN, GPIO_PIN_SET)
// 移除LED控制宏（无对应引脚）
// #define LCD_LED_ON()    HAL_GPIO_WritePin(GPIOA, LCD_LED_PIN, GPIO_PIN_SET)
// #define LCD_LED_OFF()   HAL_GPIO_WritePin(GPIOA, LCD_LED_PIN, GPIO_PIN_RESET)

// 4. 颜色定义（保持不变）
// -------------------------- 颜色定义 --------------------------
#define WHITE           0xFFFF
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define YELLOW          0xFFE0
#define GRAY            0X8430
#define BRED            0xF81F
#define MAGENTA         0xF81F
#define DARKBLUE        0x0010
#define LIGHTGREEN      0x87E0
#define GBLUE           0x07FF
#define BRRED           0xFC18

// 5. LCD参数结构体（保持不变）
// -------------------------- LCD参数结构体 --------------------------
typedef struct {
    uint16_t width;    // 宽度
    uint16_t height;   // 高度
    uint8_t dir;       // 方向：0=竖屏，1=横屏
    uint16_t wramcmd;  // 写GRAM命令
    uint16_t setxcmd;  // 设置X坐标命令
    uint16_t setycmd;  // 设置Y坐标命令
    uint16_t id;       // 芯片ID
} _lcd_dev;


void LCD_Select(void);
void LCD_Unselect(void);
void LCD_DC_Command(void);
void LCD_DC_Data(void);

// 原有代码保留
extern volatile uint8_t g_spi1_tx_done;
extern volatile uint8_t g_spi1_tx_err;

// 6. SPI DMA回调函数声明（保持不变）
void LCD_SPI1_OnTxCplt(void);
void LCD_SPI1_OnError(void);

int  LCD_SPI1_TxDMA_Start(uint8_t *buf, uint16_t len);
int  LCD_SPI1_TxDMA_Wait(uint32_t timeout_ms);
uint8_t LCD_SPI1_TxDMA_IsDone(void);

// 7. 全局变量声明（保持不变）
// -------------------------- 全局变量声明 --------------------------
extern _lcd_dev lcddev;
extern uint16_t POINT_COLOR;  // 画笔颜色
extern uint16_t BACK_COLOR;   // 背景色
extern SPI_HandleTypeDef hspi1;  // SPI1句柄

// 8. 显示方向配置（保持不变，按需修改）
#define USE_LCM_DIR     0   // 0=0度竖屏，1=180度竖屏，2=270度横屏，3=90度横屏

// 9. 函数声明（保持不变）
// -------------------------- 函数声明 --------------------------
void LCD_Init(void);                  // LCD初始化（硬件SPI版）
void LCD_DisplayOn(void);             // 开启显示
void LCD_DisplayOff(void);            // 关闭显示
void LCD_SetCursor(uint16_t x, uint16_t y);  // 设置光标
void LCD_DrawPoint(uint16_t x, uint16_t y);  // 画点
void LCD_Fast_DrawPoint(uint16_t x, uint16_t y, uint16_t color);  // 快速画点
void LCD_Display_Dir(uint8_t dir);    // 设置显示方向
void LCD_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);  // 设置显示窗口
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y);  // 读取点颜色
void LCD_Clear(uint16_t color);       // 清屏
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);  // 填充区域
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);  // 填充颜色数组
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);  // 画线
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);  // 画矩形
void LCD_Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r);  // 画圆
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode);  // 显示ASCII字符
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size);  // 显示数字（高位不补0）
void LCD_ShowxNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode);  // 显示数字（可补0）
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t *p);  // 显示字符串
void GUI_DrawFont16(uint16_t x, uint16_t y, uint8_t *s, uint8_t mode);  // 显示16x16汉字
void GUI_DrawFont24(uint16_t x, uint16_t y, uint8_t *s, uint8_t mode);  // 显示24x24汉字
void GUI_DrawFont32(uint16_t x, uint16_t y, uint8_t *s, uint8_t mode);  // 显示32x32汉字
void Show_Str(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint8_t mode);  // 显示混合字符串（英文+汉字）
void Gui_Drawbmp16(uint16_t x, uint16_t y, const unsigned char *p);  // 显示40x40 RGB565图片
void Gui_Drawbmp16_50(uint16_t x, uint16_t y, const unsigned char *p);  // 显示50x50 RGB565图片
void Gui_StrCenter(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint8_t mode);  // 居中显示字符串
void gui_draw_hline(uint16_t x0, uint16_t y0, uint16_t len, uint16_t color);  // 画水平线
void gui_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);  // 画实心圆
void lcd_draw_bline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t size, uint16_t color);  // 画粗线

// 内部函数声明（无需外部调用）
void LCD_WR_REG(uint16_t data);       // 写寄存器（硬件SPI）
void LCD_WR_DATA(uint16_t data);      // 写8位数据（硬件SPI）
void LCD_WriteReg(uint16_t reg, uint16_t val);  // 写寄存器+数据
void LCD_WriteRAM_Prepare(void);      // 准备写GRAM
void LCD_WriteRAM(uint16_t color);    // 写16位GRAM数据（硬件SPI）
void opt_delay(uint8_t i);            // 优化延时
void delay_us(uint32_t us);           // 微秒延时
void Error_Handler(void);             // 错误处理

void LCD_PushColors_u16(const uint16_t *colors, uint32_t count);
void LCD_PushColorRepeat(uint16_t color, uint32_t count);
void LCD_SPI1_Transmit_DMA_Blocking(uint8_t *buf, uint16_t len);

#endif
