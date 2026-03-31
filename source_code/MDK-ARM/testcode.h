#ifndef __TEST_CODE_H
#define __TEST_CODE_H

#include "stm32f4xx_hal.h"
#include "lcd.h"
#include "pic.h"

// 函数声明（仅保留显示类测试）
void DrawTestPage(uint8_t *str);   // 主固定栏绘制
void Chinese_Font_test(void); // 中文显示测试
void main_test(uint8_t *id);       // 测试主页显示
void FillRec_Test(void);      // 图形绘制测试
void English_Font_test(void); // 英文显示测试
void Pic_test(void);          // 图片显示测试
void Rotate_Test(void);       // 旋转显示测试
void Color_Test(void);        // 纯色显示测试
void Switch_test(void);       // 显示开关测试
void Read_Test(void);         // 颜色读取测试
void TF_test(void);           //TF卡初始化读写测试
void Fatfs_test(void);        //Fatfs系统读写测试

#endif