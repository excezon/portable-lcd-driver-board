#ifndef __TF_H
#define __TF_H

#include "stm32f4xx_hal.h"
#include "lcd.h"

// 【核心修复】修改类型名，避免与SDIO_TypeDef冲突
typedef uint8_t TF_CardTypeDef;

// 卡类型宏定义（与原逻辑一致）
#define SD_TYPE_ERR 0
#define SD_TYPE_V2 1
#define SD_TYPE_V2HC 2
#define SD_TYPE_SDXC 3

/************************** 核心接口（完全不变） **************************/
TF_CardTypeDef SD_Init(void);
uint8_t SD_ReadSector(uint32_t sector, uint8_t *buf);
uint8_t SD_WriteSector(uint32_t sector, uint8_t *buf);
uint8_t SD_ReadMultiSector(uint32_t sector, uint8_t *buf, uint32_t count);
void SD_Test(void);
uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc);

#endif
// 【警告修复】删除注释，留空行（必须是实际空行，不是注释）