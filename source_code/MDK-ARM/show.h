#ifndef __SHOW_H
#define __SHOW_H

#include "stm32f4xx_hal.h"
#include "ff.h"
#include <stdint.h>
#include "fatfs.h"  // 识别 SDFatFS 结构体
#include "lcd.h"    // 识别 _lcd_dev 结构体（lcddev 依赖）

// -------------------------- 配置区（与 show.c 统一）--------------------------
#ifndef LCD_MAX_W
#define LCD_MAX_W   240        // LCD 最大宽度
#endif
#ifndef LCD_MAX_H
#define LCD_MAX_H   320        // LCD 最大高度
#endif
#ifndef SHOW_PATH_MAX
#define SHOW_PATH_MAX  128     // 文件路径最大长度
#endif
#ifndef LCD_STREAM_BUF_BYTES
#define LCD_STREAM_BUF_BYTES  8192  // LCD 流传输缓冲区大小（show.c 依赖）
#endif
#ifndef BIN_HEADER_SECTOR_SIZE
#define BIN_HEADER_SECTOR_SIZE  512U  // BIN 文件头部扇区大小（show.c 依赖）
#endif


extern volatile uint8_t g_dma_busy;
void SPI1_DMA_Init(void);
void DMA2_Stream5_TransferComplete_Callback(void);
void Show_MaxFPS_Test_SimTF_Fast(uint32_t frame_limit);

void LCD_MaxFPS_Bench(uint32_t run_ms);


// -------------------------- 外部依赖声明（与 main.c 关联）--------------------------
/**
 * @brief  main.c 中的 FatFS 错误码解析函数
 * @param  res: FatFS 操作结果码（FRESULT 类型）
 * @retval 错误码对应的描述字符串
 */
const char* fresult_str(FRESULT res);

/**
 * @brief  main.c 中的 FatFS 文件系统对象（强制挂载时使用）
 */
extern FATFS SDFatFS;

/**
 * @brief  LCD 设备结构体（存储 LCD 宽度、高度、控制命令等信息，lcd.h 中定义）
 */
extern _lcd_dev lcddev;

// -------------------------- SPI/LCD 底层函数声明（show.c 依赖，需在 lcd.h 或 spi.h 中实现）--------------------------
/**
 * @brief  设置 LCD 显示窗口（指定起始坐标和宽高）
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  width: 显示宽度
 * @param  height: 显示高度
 * @retval 无
 */
void LCD_Set_Window(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/**
 * @brief  LCD 字符串显示函数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  width: 显示区域宽度
 * @param  height: 显示区域高度
 * @param  size: 字体大小（如 12/16/24）
 * @param  p: 要显示的字符串（uint8_t 类型指针）
 * @retval 无
 */
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t *p);

/**
 * @brief  核心功能函数声明
 */
void Show_Image_From_Bin(const char *file_path);
void Show_Image_From_Bin_Benchmark(const char *file_path);
void Play_Bin_Folder_Loop(const char *folder_path);
void List_Folder_Files(const char *folder_path);
void Test_Dir_Open(const char *folder_path);
void ShowStat(uint32_t t_frame0, uint32_t t_read_sum, uint32_t t_tx_sum, uint32_t total_bytes);
void Show_MaxFPS_Test_SimTF(uint32_t frame_limit);
#endif /* __SHOW_H */

// 修复警告：文件末尾添加实际空行（此处留空，无任何字符）