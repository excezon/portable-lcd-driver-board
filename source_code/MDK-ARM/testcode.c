#include "testcode.h"
#include "tf.h"
#include "fatfs.h"

// 颜色数组定义（u16 → uint16_t）
uint16_t ColorTab[5] = {RED, GREEN, BLUE, YELLOW, BRED};
uint16_t ColornTab[8] = {RED, MAGENTA, GREEN, DARKBLUE, BLUE, BLACK, LIGHTGREEN};

//TF卡数据
uint8_t sd_test_buf[512] = {0};  // SD卡读写测试缓冲区（512字节，扇区大小）
uint8_t sd_card_type = SD_TYPE_ERR;  // 存储SD卡类型
char lcd_print_buf[40] = {0};        // LCD字符串格式化缓冲区

// FatFS全局对象
FATFS fs;     // 文件系统对象
FIL file;     // 文件对象
FRESULT res;  // FatFS操作结果
UINT  bw= 0;  // 写入字节数
UINT  br= 0;  // 读取字节数
char write_buf[] = "Hello TF Card! FatFS Test - 2025";  // 写入内容
char read_buf[512] = {0};                               // 读取缓冲区
char lcd_buf[64] = {0};                                 // LCD显示缓冲区


// 主固定栏绘制（u8 → uint8_t）
void DrawTestPage(uint8_t *str)
{
    // 绘制上固定栏
    LCD_Clear(WHITE);
    LCD_Fill(0, 0, lcddev.width, 20, BLUE);
    POINT_COLOR = GREEN;
    LCD_DrawLine(0, 20, lcddev.width, 20);
    POINT_COLOR = BLUE;
    LCD_DrawLine(0, 21, lcddev.width, 21);
    POINT_COLOR = BRRED;
    LCD_DrawLine(0, 22, lcddev.width, 22);
    // 绘制下固定栏
    LCD_Fill(0, lcddev.height - 20, lcddev.width, lcddev.height, BLUE);
    POINT_COLOR = GREEN;
    LCD_DrawLine(0, lcddev.height - 20, lcddev.width, lcddev.height - 20);
    POINT_COLOR = BLUE;
    LCD_DrawLine(0, lcddev.height - 21, lcddev.width, lcddev.height - 21);
    POINT_COLOR = BRRED;
    LCD_DrawLine(0, lcddev.height - 22, lcddev.width, lcddev.height - 22);
    // 显示标题和厂家信息
    POINT_COLOR = WHITE;
    Gui_StrCenter(0, 2, str, 16, 1); // 居中显示标题
    Gui_StrCenter(0, lcddev.height - 18, "深圳市鸿讯电子有限公司", 16, 1); // 居中显示厂家
}

// 中文显示测试
void Chinese_Font_test(void)
{
    DrawTestPage("中文显示测试");
    POINT_COLOR = RED;    // 画笔颜色
    BACK_COLOR = BLACK;   // 背景色

    Show_Str(10, 30, "16X16:鸿讯电子", 16, 1);
    Show_Str(10, 50, "16X16:TFT液晶显示屏", 16, 1);
    Show_Str(10, 70, "24X24:中文显示", 24, 1);
    Show_Str(10, 100, "32X32:测试例程", 32, 1);
    HAL_Delay(1500);
}

// 测试主页（u8 → uint8_t）
void main_test(uint8_t *id)
{
    DrawTestPage("显示屏测试程序");
    POINT_COLOR = RED;
    Gui_StrCenter(0, 100, "显示屏", 32, 1);
    POINT_COLOR = GREEN;
    Gui_StrCenter(0, 135, "测试程序", 32, 1);
    POINT_COLOR = BLUE;
    Gui_StrCenter(0, 170, id, 24, 1);
    POINT_COLOR = BLACK;
    Gui_StrCenter(0, 196, "240RGB*320", 24, 1);
    POINT_COLOR = BLACK;
    Gui_StrCenter(0, 235, "TEL:13528896127", 24, 1);
    HAL_Delay(1800);
}

// 图形测试（u8 → uint8_t）
void FillRec_Test(void)
{
    uint8_t i = 0;
    DrawTestPage("图形测试");
    LCD_Fill(0, 24, lcddev.width, lcddev.height - 24, WHITE);

    // 绘制空心矩形
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_DrawRectangle(lcddev.width/2 -25 + (i*15), lcddev.height/2 -25 + (i*15), 
                         lcddev.width/2 -25 + (i*15) +60, lcddev.height/2 -25 + (i*15) +60);
        HAL_Delay(100);
    }

    // 绘制空心圆
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_Draw_Circle(50 + (i*35), 60, 30);
        HAL_Delay(100);
    }
    HAL_Delay(1500);

    // 绘制实心矩形
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_Fill(lcddev.width/2 -25 + (i*15), lcddev.height/2 -25 + (i*15), 
                 lcddev.width/2 -25 + (i*15) +60, lcddev.height/2 -25 + (i*15) +60, POINT_COLOR);
        HAL_Delay(100);
    }

    // 绘制实心圆
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        gui_fill_circle(50 + (i*35), 60, 30, POINT_COLOR);
        HAL_Delay(100);
    }
    HAL_Delay(1500);
}

// 英文显示测试
void English_Font_test(void)
{
    DrawTestPage("英文显示测试");
    POINT_COLOR = RED;
    BACK_COLOR = WHITE;
    LCD_ShowString(10, 30, 200, 12, 12, "6X12:abcdefghijklmnopqrstuvwxyz");
    LCD_ShowString(10, 45, 200, 12, 12, "6X12:ABCDEFGHIJKLMNOP0123456789");
    POINT_COLOR = GREEN;
    LCD_ShowString(10, 60, 200, 16, 16, "8X16:abcdefghijklmnopqrstuvwxyz");
    LCD_ShowString(10, 80, 200, 16, 16, "8X16:ABCDEFGHIJKLMNOP0123456789");
    POINT_COLOR = BLUE;
    LCD_ShowString(10, 100, 200, 24, 24, "12X24:abcdefghijklmnopqrstuvwxyz");
    LCD_ShowString(10, 128, 200, 24, 24, "12X24:ABCDEFGHIJKLMNOP0123456789");
    HAL_Delay(1500);
}

// 图片显示测试
void Pic_test(void)
{
    DrawTestPage("图片显示测试");
    POINT_COLOR = BLUE;
    //Gui_Drawbmp16(60, 100, gImage_1);
    //Gui_Drawbmp16_50(130, 100, gImage_2);
    HAL_Delay(1500);
}

// 旋转显示测试（u8 → uint8_t）
void Rotate_Test(void)
{
    uint8_t num = 0;
    uint8_t *Direction[4] = {"Rotation:0", "Rotation:180", "Rotation:270", "Rotation:90"};

    for(num = 0; num < 4; num++)
    {
        LCD_Display_Dir(num);
        DrawTestPage("旋转测试");
        POINT_COLOR = BLUE;
        Show_Str(20, 30, Direction[num], 16, 1);
        //Gui_Drawbmp16(30, 50, gImage_1);
        HAL_Delay(1000);
        HAL_Delay(1000);
    }
    LCD_Display_Dir(USE_LCM_DIR); // 恢复默认显示方向
}

// 彩条显示（u16 → uint16_t，u8 → uint8_t）
void DispBand(void)
{
    uint32_t i, j, k;  // 宽/高可能超过8位，改用uint32_t更安全
    uint16_t color[8] = {BLUE, GREEN, RED, GBLUE, BRED, YELLOW, BLACK, WHITE};
    LCD_Set_Window(0, 0, lcddev.width, lcddev.height);
    LCD_WriteRAM_Prepare(); // 开始写入GRAM

    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < lcddev.height/8; j++)
        {
            for(k = 0; k < lcddev.width; k++)
            {
                LCD_WriteRAM(color[i]);
            }
        }
    }
    for(j = 0; j < lcddev.height%8; j++)
    {
        for(k = 0; k < lcddev.width; k++)
        {
            LCD_WriteRAM(color[7]);
        }
    }
    HAL_Delay(1000);
}

// 纯色显示测试
void Color_Test(void)
{
    DispBand();
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    Show_Str(20, 30, "WHITE", 16, 1);
    HAL_Delay(1000);

    POINT_COLOR = WHITE;
    LCD_Clear(RED);
    Show_Str(20, 30, "RED ", 16, 1);
    HAL_Delay(1000);

    LCD_Clear(GREEN);
    Show_Str(20, 30, "GREEN ", 16, 1);
    HAL_Delay(1000);

    LCD_Clear(BLUE);
    Show_Str(20, 30, "BLUE ", 16, 1);
    HAL_Delay(1500);
}

// 显示开关测试（u8 → uint8_t）
void TF_test(void){
    // 局部变量扩展：新增验证相关变量
    uint16_t verify_err_idx = 0;  // 验证错误的字节位置
    uint8_t verify_flag = 1;      // 验证标志：1=成功，0=失败
    const uint32_t test_sector = 100;  // 测试扇区（选100，避开前10个引导扇区）

	// 1. 显示测试标题
    LCD_ShowString(10, 10, 240, 16, 16, "    SD Card Test Program");  // 大标题（16号字体）
    LCD_ShowString(10, 35, 240, 12, 12, "--------------------------------------");

    // 2. 检测SD卡是否插入（CD引脚）
    LCD_ShowString(10, 55, 240, 12, 12, "Step1: Check Card Insert...");
    if(!SD_CD_CHECK())
    {
        // 无卡：显示错误并死循环
        LCD_ShowString(10, 70, 240, 12, 12, "Error: No SD Card Insert!");
        while(1)
        {
            HAL_Delay(500);  // 循环等待，可加LED闪烁提示
        }
    }
    LCD_ShowString(10, 70, 240, 12, 12, "Card Insert: OK");

    // 3. SD卡初始化（内部已包含LCD打印初始化过程）
    LCD_ShowString(10, 85, 240, 12, 12, "Step2: SD Card Init...");
    sd_card_type = SD_Init();
    if(sd_card_type == SD_TYPE_ERR)
    {
        // 初始化失败：显示错误并死循环
        LCD_ShowString(10, 100, 240, 12, 12, "Error: SD Init Failed!");
        while(1)
        {
            HAL_Delay(500);
        }
    }
    // 显示SD卡类型
    LCD_ShowString(10, 100, 240, 12, 12, "SD Type: ");
    switch(sd_card_type)
    {
        case SD_TYPE_MMC:    sprintf(lcd_print_buf, "MMC Card"); break;
        case SD_TYPE_V1:     sprintf(lcd_print_buf, "SD V1 Card"); break;
        case SD_TYPE_V2:     sprintf(lcd_print_buf, "SD V2 Card"); break;
        case SD_TYPE_V2HC:   sprintf(lcd_print_buf, "SD V2 HC Card"); break;
        default:             sprintf(lcd_print_buf, "Unknown"); break;
    }
    LCD_ShowString(60, 100, 240, 12, 12, lcd_print_buf);

    // 4. SD卡写扇区测试（优化：选扇区100，避开引导区）
    LCD_ShowString(10, 115, 240, 12, 12, "Step3: Write Sector Test...");
  
    // 4.1 填充可追溯的测试数据（0~255循环+特征值，便于定位错误）
    for(uint16_t i = 0; i < 512; i++)
    {
        sd_test_buf[i] = (i % 256) ^ 0x55;  // 异或0x55，避免全0/全1的无效测试
    }

    // 4.2 写入测试扇区（HC卡直接传扇区号，非HC卡驱动已自动×512）
    if(SD_WriteSector(test_sector, sd_test_buf) != 0)
    {
        // 写入失败
        LCD_ShowString(10, 130, 240, 12, 12, "Write Sector: FAIL");
        while(1)
        {
            HAL_Delay(500);
        }
    }
    LCD_ShowString(10, 130, 240, 12, 12, "Write Sector: OK");

    // 5. 清空缓冲区，准备读取验证（避免残留数据干扰）
    memset(sd_test_buf, 0x00, 512);
    LCD_ShowString(10, 145, 240, 12, 12, "Step4: Read Sector Test...");

    // 6. 读取同一测试扇区（修正原代码读取扇区不一致的错误）
    if(SD_ReadSector(test_sector, sd_test_buf) != 0)
    {
        // 读取失败
        LCD_ShowString(10, 160, 240, 12, 12, "Read Sector: FAIL");
        while(1)
        {
            HAL_Delay(500);
        }
    }
    LCD_ShowString(10, 160, 240, 12, 12, "Read Sector: OK");

    // ========== 新增：Step5 数据验证逻辑 ==========
    LCD_ShowString(10, 175, 240, 12, 12, "Step5: Data Verify...");
    // 6.1 逐字节对比读取数据与原始数据
    verify_flag = 1;
    verify_err_idx = 0;
    for(uint16_t i = 0; i < 512; i++)
    {
        // 计算原始数据（与写入时一致）
        uint8_t orig_data = (i % 256) ^ 0x55;
        if(sd_test_buf[i] != orig_data)
        {
            verify_flag = 0;          // 验证失败
            verify_err_idx = i;       // 记录错误字节位置
            break;                    // 找到第一个错误即退出，减少耗时
        }
    }
    // 6.2 显示验证结果
    if(verify_flag == 1)
    {
        LCD_ShowString(10, 190, 240, 12, 12, "Data Verify: OK");
    }
    else
    {
        LCD_ShowString(10, 190, 240, 12, 12, "Data Verify: FAIL");
        // 显示错误位置和数据（便于定位问题）
        sprintf(lcd_print_buf, "Err Pos:%d (0x%04X)", verify_err_idx, verify_err_idx);
        LCD_ShowString(10, 205, 240, 12, 12, lcd_print_buf);
        sprintf(lcd_print_buf, "Expect:0x%02X Got:0x%02X", 
                (verify_err_idx % 256) ^ 0x55, sd_test_buf[verify_err_idx]);
        LCD_ShowString(10, 220, 240, 12, 12, lcd_print_buf);
        // 验证失败，死循环提示
        while(1)
        {
            HAL_Delay(500);
        }
    }

    // 7. （可选）全屏显示WinHex格式数据（保留原有功能）
    //SD_ShowSectorData_WinHex(sd_test_buf);  

    // 8. 测试完成提示
    LCD_ShowString(10, 202, 240, 12, 12, "All Test Pass!");
}

void Fatfs_test(void){
	LCD_ShowString(10, 10, 240, 12, 12, "=== TF FatFs Verify ===");

	char lcd_buf[64];

// 0) SD init
LCD_ShowString(10, 25, 240, 12, 12, "0) SD_Init:");
uint8_t t = SD_Init();
sprintf(lcd_buf, "TYPE=%d", t);
LCD_ShowString(120, 25, 240, 12, 12, lcd_buf);
if(t == SD_TYPE_ERR) while(1);

// 1) mount
LCD_ShowString(10, 40, 240, 12, 12, "1) f_mount:");
FRESULT fr = f_mount(&fs, "0:", 1);
sprintf(lcd_buf, "FR=%d", fr);
LCD_ShowString(120, 40, 240, 12, 12, lcd_buf);
if(fr != FR_OK) while(1);

// 2) getfree + show total/free KB
LCD_ShowString(10, 55, 240, 12, 12, "2) f_getfree:");
DWORD fre_clust = 0;
FATFS *pfs = 0;
fr = f_getfree("0:", &fre_clust, &pfs);
sprintf(lcd_buf, "FR=%d", fr);
LCD_ShowString(120, 55, 240, 12, 12, lcd_buf);
if(fr != FR_OK) while(1);

// total/free sectors -> KB
DWORD tot_sect = (pfs->n_fatent - 2) * pfs->csize;
DWORD fre_sect = fre_clust * pfs->csize;
sprintf(lcd_buf, "TotalKB:%lu", tot_sect / 2);
LCD_ShowString(10, 70, 240, 12, 12, lcd_buf);
sprintf(lcd_buf, "FreeKB :%lu", fre_sect / 2);
LCD_ShowString(10, 82, 240, 12, 12, lcd_buf);

// 3) write file
LCD_ShowString(10, 97, 240, 12, 12, "3) write 0:/t.txt:");
const char *msg = "Hello TF Card! FatFS OK.";
UINT bw = 0;
fr = f_open(&file, "0:/t.txt", FA_CREATE_ALWAYS | FA_WRITE);
if(fr == FR_OK) {
    fr = f_write(&file, msg, strlen(msg), &bw);
    if(fr == FR_OK) fr = f_sync(&file);
    f_close(&file);
}
sprintf(lcd_buf, "FR=%d BW=%u", fr, (unsigned)bw);
LCD_ShowString(10, 112, 240, 12, 12, lcd_buf);
if(fr != FR_OK || bw != strlen(msg)) while(1);

// 4) stat file (exist?)
LCD_ShowString(10, 127, 240, 12, 12, "4) f_stat t.txt:");
FILINFO fno;
fr = f_stat("0:/t.txt", &fno);
sprintf(lcd_buf, "FR=%d SZ=%lu", fr, (unsigned long)fno.fsize);
LCD_ShowString(10, 142, 240, 12, 12, lcd_buf);
if(fr != FR_OK) while(1);

// 5) read back + compare
LCD_ShowString(10, 157, 240, 12, 12, "5) read & cmp:");
char rb[64] = {0};
UINT br = 0;
fr = f_open(&file, "0:/t.txt", FA_READ);
if(fr == FR_OK) {
    fr = f_read(&file, rb, sizeof(rb)-1, &br);
    f_close(&file);
}
int ok = (fr == FR_OK) && (br == strlen(msg)) && (memcmp(rb, msg, strlen(msg)) == 0);
sprintf(lcd_buf, "FR=%d BR=%u %s", fr, (unsigned)br, ok ? "PASS" : "FAIL");
LCD_ShowString(10, 172, 240, 12, 12, lcd_buf);
LCD_ShowString(10, 187, 240, 12, 12, "RB:");
LCD_ShowString(40, 187, 240, 12, 12, rb);   // rb 是读回buffer

// 6) list root (optional: show first entry)
LCD_ShowString(10, 202, 240, 12, 12, "6) dir 0:/:");
DIR dir;
fr = f_opendir(&dir, "0:/");
if(fr == FR_OK) {
    fr = f_readdir(&dir, &fno);
    f_closedir(&dir);
}
sprintf(lcd_buf, "FR=%d %s", fr, (fr==FR_OK && fno.fname[0]) ? fno.fname : "(empty)");
LCD_ShowString(80, 202, 240, 12, 12, lcd_buf);

}


