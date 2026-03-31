/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor include file               (C)ChaN, 2019
/ 适配STM32F401RCT6（裸机/HAL库环境）
/----------------------------------------------------------------------------*/
#ifndef DEF_TJPGDEC
#define DEF_TJPGDEC
/*---------------------------------------------------------------------------*/
/* 【STM32F401适配】系统配置 - 按需修改 */

// 1. 输出像素格式（二选一，F401内存有限，默认RGB565）
#define TJPGD_USING_FORMAT_RGB565    // 1: RGB565 (2字节/像素)，节省RAM
//#define TJPGD_USING_FORMAT_RGB888  // 0: RGB888 (3字节/像素)，色彩更准

// 2. 启用缩放功能（F401算力足够，支持1/1~1/8缩放）
#define TJPGD_USING_SCALE

// 3. 启用快速裁剪表（占用1K Flash，加速像素值限制，建议启用）
#define TJPGD_USING_TBLCLIP

// 4. 输入缓冲区大小（F401 RAM 80K，设512字节平衡效率和内存）
#define TJPGD_INPUT_BUFFER_SIZE    512

/*---------------------------------------------------------------------------*/
/* 自动推导配置（无需修改） */
#ifdef TJPGD_USING_FORMAT_RGB888
#define JD_FORMAT        0   /* RGB888 (3 BYTE/pix) */
#elif defined TJPGD_USING_FORMAT_RGB565
#define JD_FORMAT        1   /* RGB565 (1 WORD/pix) */
#endif

#ifdef TJPGD_USING_SCALE
#define JD_USE_SCALE     1   /* 启用缩放 */
#else
#define JD_USE_SCALE     0
#endif

#ifdef TJPGD_USING_TBLCLIP
#define JD_TBLCLIP       1   /* 启用快速裁剪表 */
#else
#define JD_TBLCLIP       0
#endif

#define JD_SZBUF         TJPGD_INPUT_BUFFER_SIZE  /* 输入缓冲区大小 */

/*---------------------------------------------------------------------------*/
/* 【STM32F401适配】编译器兼容（ARM GCC/MDK） */
#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"  // STM32 HAL/标准库自带

// 补充inline定义（兼容ARM编译器）
#ifndef inline
#ifdef __GNUC__
#define inline __inline__
#else
#define inline __inline
#endif
#endif

/* 错误码（保持原逻辑） */
typedef enum {
    JDR_OK = 0,    /* 0: 成功 */
    JDR_INTR,      /* 1: 输出函数中断 */
    JDR_INP,       /* 2: 输入流错误 */
    JDR_MEM1,      /* 3: 内存池不足 */
    JDR_MEM2,      /* 4: 输入缓冲区不足 */
    JDR_PAR,       /* 5: 参数错误 */
    JDR_FMT1,      /* 6: 数据格式错误 */
    JDR_FMT2,      /* 7: 格式支持但不兼容 */
    JDR_FMT3       /* 8: 不支持的JPEG标准 */
} JRESULT;

/* 矩形结构体（指定输出区域） */
typedef struct {
    uint16_t left, right, top, bottom;
} JRECT;

/* 解码器上下文（修复拼写错误：momory→memory） */
typedef struct JDEC JDEC;
struct JDEC {
    uint16_t dctr;                /* 输入缓冲区剩余字节数 */
    uint8_t* dptr;                /* 当前读取指针 */
    uint8_t* inbuf;                /* 输入缓冲区 */
    uint8_t dmsk;                 /* 位掩码 */
    uint8_t scale;                /* 缩放比例（0-3） */
    uint8_t msx, msy;             /* MCU块大小（宽/高） */
    uint8_t qtid[3];              /* 量化表ID */
    int16_t dcv[3];               /* 上一个DC元素 */
    uint16_t nrst;                /* 重启间隔 */
    uint16_t width, height;       /* 图像尺寸 */
    uint8_t* huffbits[2][2];       /* 哈夫曼比特表 */
    uint16_t* huffcode[2][2];      /* 哈夫曼码字表 */
    uint8_t* huffdata[2][2];       /* 哈夫曼数据表 */
    int32_t* qttbl[4];            /* 反量化表 */
    void* workbuf;                /* IDCT工作缓冲区 */
    uint8_t* mcubuf;              /* MCU缓冲区 */
    void* pool;                   /* 内存池指针 */
    uint16_t sz_pool;             /* 内存池大小（修复拼写错误） */
    uint16_t (*infunc)(JDEC*, uint8_t*, uint16_t); /* 输入回调 */
    void* device;                 /* 设备句柄（可绑定FATFS/LCD） */
};

/* 核心API声明 */
JRESULT jd_prepare(JDEC*, uint16_t(*)(JDEC*, uint8_t*, uint16_t), void*, uint16_t, void*);
JRESULT jd_decomp(JDEC*, uint16_t(*)(JDEC*, void*, JRECT*), uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* DEF_TJPGDEC */  // 修复宏定义一致性