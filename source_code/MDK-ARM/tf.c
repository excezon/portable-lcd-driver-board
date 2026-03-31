#include "tf.h"
#include "sdio.h"
#include <stdio.h>
#include <string.h>

/************************** 内部参数（修改类型名） **************************/
TF_CardTypeDef SD_Type = SD_TYPE_ERR;
static uint8_t s_inited_ok = 0;

// 超时参数（与原逻辑一致）
#ifndef SD_RW_TIMEOUT_MS
#define SD_RW_TIMEOUT_MS  5000
#endif

// 测试参数（与main.c一致）
#define TEST_BLOCK_ADDR 100
#define TEST_BLOCK_NUM  1
#define SD_TIMEOUT_MS   5000

/************************** 【警告修复】删除未引用的SD_WaitReady函数 **************************/

/************************** TF卡初始化 **************************/
TF_CardTypeDef SD_Init(void)
{
    char lcd_buf[32];
    uint16_t height = 20;

    if (s_inited_ok && SD_Type != SD_TYPE_ERR)
        return SD_Type;

    LCD_ShowString(10, height, 200, 12, 12, (uint8_t *)"SDIO Init:       ");
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
        LCD_ShowString(90, height, 200, 12, 12, (uint8_t *)"BUS FAIL");
        SD_Type = SD_TYPE_ERR;
        s_inited_ok = 0;
        return SD_Type;
    }
    LCD_ShowString(90, height, 200, 12, 12, (uint8_t *)"OK");

    // 卡类型识别（仅修改类型名）
    HAL_SD_CardInfoTypeDef card_info;
    HAL_SD_GetCardInfo(&hsd, &card_info);
    SD_Type = (card_info.CardType == CARD_SDHC_SDXC) ? SD_TYPE_V2HC : SD_TYPE_V2;

    LCD_ShowString(10, height + 12, 200, 12, 12, (uint8_t *)"Init Result:     ");
    sprintf(lcd_buf, "SUCCESS (TYPE:%d)", SD_Type);
    LCD_ShowString(90, height + 12, 220, 12, 12, (uint8_t *)lcd_buf);
    s_inited_ok = 1;

    return SD_Type;
}

/************************** 写入单扇区 **************************/
uint8_t SD_WriteSector(uint32_t sector, uint8_t *buf)
{
    if (buf == NULL) return 1;

    HAL_StatusTypeDef sd_status = HAL_SD_WriteBlocks(&hsd, buf, sector, 1, SD_TIMEOUT_MS);
    if (sd_status != HAL_OK)
        return 1;

    uint32_t timeout = SD_TIMEOUT_MS;
    while((HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) && (timeout-- > 0))
    {
        HAL_Delay(1);
    }
    if (timeout == 0)
        return 1;

    return 0;
}

/************************** 读取单扇区 **************************/
uint8_t SD_ReadSector(uint32_t sector, uint8_t *buf)
{
    if (buf == NULL) return 1;

    HAL_StatusTypeDef sd_status = HAL_SD_ReadBlocks(&hsd, buf, sector, 1, SD_TIMEOUT_MS);
    if (sd_status != HAL_OK)
        return 1;

    uint32_t timeout = SD_TIMEOUT_MS;
    while((HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) && (timeout-- > 0))
    {
        HAL_Delay(1);
    }
    if (timeout == 0)
        return 1;

    return 0;
}

/************************** 读取多扇区 **************************/
uint8_t SD_ReadMultiSector(uint32_t sector, uint8_t *buf, uint32_t count)
{
    if (buf == NULL || count == 0) return 1;

    HAL_StatusTypeDef sd_status = HAL_SD_ReadBlocks(&hsd, buf, sector, count, SD_TIMEOUT_MS);
    if (sd_status != HAL_OK)
        return 1;

    uint32_t timeout = SD_TIMEOUT_MS;
    while((HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) && (timeout-- > 0))
    {
        HAL_Delay(1);
    }
    if (timeout == 0)
        return 1;

    return 0;
}

/************************** TF卡测试函数 **************************/
void SD_Test(void)
{
    uint16_t lcd_show_height = 10;
    char info_buf[64] = {0};

    LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"TF Card Test Start...");
    lcd_show_height += 24;

    LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"SDIO Init Success");
    lcd_show_height += 24;

    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd);
    if(card_state != HAL_SD_CARD_TRANSFER)
    {
        LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"Error: TF Card Not Found!");
        while(1);
    }
    LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"TF Card Detected");
    lcd_show_height += 24;

    HAL_SD_CardInfoTypeDef card_info;
    HAL_SD_GetCardInfo(&hsd, &card_info);
    sprintf(info_buf, "Card Size: %.1f GB", (float)(card_info.BlockSize * (uint64_t)card_info.BlockNbr) / 1024 / 1024 / 1024);
    LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)info_buf);
    lcd_show_height += 24;

    // 显示卡类型
    if(card_info.CardType == CARD_SDHC_SDXC)
    {
        LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"Card Type: SDHC/SDXC");
    }
    else if(card_info.CardType == CARD_SDSC)
    {
        LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"Card Type: Standard SD");
    }
    else
    {
        LCD_ShowString(10, lcd_show_height, 200, 12, 12, (uint8_t *)"Card Type: Unknown");
    }
}

/************************** 兼容旧接口（无功能） **************************/
uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    (void)cmd;
    (void)arg;
    (void)crc;
    return 0xFF;
}
// 【警告修复】删除注释，留实际空行