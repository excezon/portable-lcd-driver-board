/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor R0.01c                     (C)ChaN, 2019
/ 适配STM32F401RCT6（裸机/HAL库环境）
/----------------------------------------------------------------------------*/
#include "tjpgd.h"

/*-----------------------------------------------*/
/* 之字形转光栅序表 */
/*-----------------------------------------------*/
#define ZIG(n) Zig[n]  // 修复原代码缺少空格的语法错误
static const uint8_t Zig[64] = {
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/*-------------------------------------------------*/
/* Arai算法输入比例因子（16位定点数） */
/*-------------------------------------------------*/
#define IPSF(n) Ipsf[n]  // 修复语法错误
static const uint16_t Ipsf[64] = {
    (uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192),
    (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
    (uint16_t)(1.38704*8192), (uint16_t)(1.92388*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.63099*8192),
    (uint16_t)(1.38704*8192), (uint16_t)(1.08979*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.38268*8192),
    (uint16_t)(1.30656*8192), (uint16_t)(1.81226*8192), (uint16_t)(1.70711*8192), (uint16_t)(1.53636*8192),
    (uint16_t)(1.30656*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.36048*8192),
    (uint16_t)(1.17588*8192), (uint16_t)(1.63099*8192), (uint16_t)(1.53636*8192), (uint16_t)(1.38268*8192),
    (uint16_t)(1.17588*8192), (uint16_t)(0.92388*8192), (uint16_t)(0.63638*8192), (uint16_t)(0.32442*8192),
    (uint16_t)(1.00000*8192), (uint16_t)(1.38704*8192), (uint16_t)(1.30656*8192), (uint16_t)(1.17588*8192),
    (uint16_t)(1.00000*8192), (uint16_t)(0.78570*8192), (uint16_t)(0.54120*8192), (uint16_t)(0.27590*8192),
    (uint16_t)(0.78570*8192), (uint16_t)(1.08979*8192), (uint16_t)(1.02656*8192), (uint16_t)(0.92388*8192),
    (uint16_t)(0.78570*8192), (uint16_t)(0.61732*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.21677*8192),
    (uint16_t)(0.54120*8192), (uint16_t)(0.75066*8192), (uint16_t)(0.70711*8192), (uint16_t)(0.63638*8192),
    (uint16_t)(0.54120*8192), (uint16_t)(0.42522*8192), (uint16_t)(0.29290*8192), (uint16_t)(0.14932*8192),
    (uint16_t)(0.27590*8192), (uint16_t)(0.38268*8192), (uint16_t)(0.36048*8192), (uint16_t)(0.32442*8192),
    (uint16_t)(0.27590*8192), (uint16_t)(0.21678*8192), (uint16_t)(0.14932*8192), (uint16_t)(0.07612*8192)
};

/*---------------------------------------------*/
/* 快速裁剪表（JD_TBLCLIP=1时启用） */
/*---------------------------------------------*/
#if JD_TBLCLIP
#define BYTECLIP(v) Clip8[(uint16_t)(v) & 0x3FF]
static const uint8_t Clip8[1024] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
#else
/* 无快速裁剪表时的inline函数（替换原rt_inline） */
inline uint8_t BYTECLIP(int16_t val) {
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return (uint8_t)val;
}
#endif

/*-----------------------------------------------------------------------*/
/* 从内存池分配内存（4字节对齐，适配ARM架构） */
/*-----------------------------------------------------------------------*/
static void* alloc_pool(JDEC* jd, uint16_t nd) {
    char* rp = 0;
    nd = (nd + 3) & ~3;  // 4字节对齐
    if (jd->sz_pool >= nd) {
        jd->sz_pool -= nd;
        rp = (char*)jd->pool;
        jd->pool = (void*)(rp + nd);
    }
    return (void*)rp;
}

/*-----------------------------------------------------------------------*/
/* 创建反量化表 */
/*-----------------------------------------------------------------------*/
static int create_qt_tbl(JDEC* jd, const uint8_t* data, uint16_t ndata) {
    uint16_t i;
    uint8_t d, z;
    int32_t* pb;
    while (ndata) {
        if (ndata < 65) return JDR_FMT1;
        ndata -= 65;
        d = *data++;
        if (d & 0xF0) return JDR_FMT1;
        i = d & 3;
        pb = alloc_pool(jd, 64 * sizeof(int32_t));
        if (!pb) return JDR_MEM1;
        jd->qttbl[i] = pb;
        for (i = 0; i < 64; i++) {
            z = ZIG(i);
            pb[z] = (int32_t)((uint32_t)*data++ * IPSF(z));
        }
    }
    return JDR_OK;
}

/*-----------------------------------------------------------------------*/
/* 创建哈夫曼表 */
/*-----------------------------------------------------------------------*/
static int create_huffman_tbl(JDEC* jd, const uint8_t* data, uint16_t ndata) {
    uint16_t i, j, b, np, cls, num;
    uint8_t d, *pb, *pd;
    uint16_t hc, *ph;
    while (ndata) {
        if (ndata < 17) return JDR_FMT1;
        ndata -= 17;
        d = *data++;
        if (d & 0xEE) return JDR_FMT1;
        cls = d >> 4; num = d & 0x0F;
        pb = alloc_pool(jd, 16);
        if (!pb) return JDR_MEM1;
        jd->huffbits[num][cls] = pb;
        for (np = i = 0; i < 16; i++) {
            np += (pb[i] = *data++);
        }
        ph = alloc_pool(jd, (uint16_t)(np * sizeof(uint16_t)));
        if (!ph) return JDR_MEM1;
        jd->huffcode[num][cls] = ph;
        hc = 0;
        for (j = i = 0; i < 16; i++) {
            b = pb[i];
            while (b--) ph[j++] = hc++;
            hc <<= 1;
        }
        if (ndata < np) return JDR_FMT1;
        ndata -= np;
        pd = alloc_pool(jd, np);
        if (!pd) return JDR_MEM1;
        jd->huffdata[num][cls] = pd;
        for (i = 0; i < np; i++) {
            d = *data++;
            if (!cls && d > 11) return JDR_FMT1;
            *pd++ = d;
        }
    }
    return JDR_OK;
}

/*-----------------------------------------------------------------------*/
/* 从输入流提取N位 */
/*-----------------------------------------------------------------------*/
static int bitext(JDEC* jd, int nbit) {
    uint8_t msk, s, *dp;
    uint16_t dc, v, f;
    msk = jd->dmsk; dc = jd->dctr; dp = jd->dptr;
    s = *dp; v = f = 0;
    do {
        if (!msk) {
            if (!dc) {
                dp = jd->inbuf;
                dc = jd->infunc(jd, dp, JD_SZBUF);
                if (!dc) return 0 - (int16_t)JDR_INP;
            } else {
                dp++;
            }
            dc--;
            if (f) {
                f = 0;
                if (*dp != 0) return 0 - (int16_t)JDR_FMT1;
                *dp = s = 0xFF;
            } else {
                s = *dp;
                if (s == 0xFF) {
                    f = 1; continue;
                }
            }
            msk = 0x80;
        }
        v <<= 1;
        if (s & msk) v++;
        msk >>= 1;
        nbit--;
    } while (nbit);
    jd->dmsk = msk; jd->dctr = dc; jd->dptr = dp;
    return (int)v;
}

/*-----------------------------------------------------------------------*/
/* 哈夫曼解码 */
/*-----------------------------------------------------------------------*/
static int16_t huffext(JDEC* jd, const uint8_t* hbits, const uint16_t* hcode, const uint8_t* hdata) {
    uint8_t msk, s, *dp;
    uint16_t dc, v, f, bl, nd;
    msk = jd->dmsk; dc = jd->dctr; dp = jd->dptr;
    s = *dp; v = f = 0;
    bl = 16;
    do {
        if (!msk) {
            if (!dc) {
                dp = jd->inbuf;
                dc = jd->infunc(jd, dp, JD_SZBUF);
                if (!dc) return 0 - (int16_t)JDR_INP;
            } else {
                dp++;
            }
            dc--;
            if (f) {
                f = 0;
                if (*dp != 0) return 0 - (int16_t)JDR_FMT1;
                *dp = s = 0xFF;
            } else {
                s = *dp;
                if (s == 0xFF) {
                    f = 1; continue;
                }
            }
            msk = 0x80;
        }
        v <<= 1;
        if (s & msk) v++;
        msk >>= 1;
        for (nd = *hbits++; nd; nd--) {
            if (v == *hcode++) {
                jd->dmsk = msk; jd->dctr = dc; jd->dptr = dp;
                return *hdata;
            }
            hdata++;
        }
        bl--;
    } while (bl);
    return 0 - (int16_t)JDR_FMT1;
}

/*-----------------------------------------------------------------------*/
/* IDCT变换（Arai算法） */
/*-----------------------------------------------------------------------*/
static void block_idct(int32_t* src, uint8_t* dst) {
    const int32_t M13 = (int32_t)(1.41421*4096), M2 = (int32_t)(1.08239*4096), M4 = (int32_t)(2.61313*4096), M5 = (int32_t)(1.84776*4096);
    int32_t v0, v1, v2, v3, v4, v5, v6, v7;
    int32_t t10, t11, t12, t13;
    uint16_t i;
    /* 列处理 */
    for (i = 0; i < 8; i++) {
        v0 = src[8 * 0];
        v1 = src[8 * 2];
        v2 = src[8 * 4];
        v3 = src[8 * 6];
        t10 = v0 + v2;
        t12 = v0 - v2;
        t11 = (v1 - v3) * M13 >> 12;
        v3 += v1;
        t11 -= v3;
        v0 = t10 + v3;
        v3 = t10 - v3;
        v1 = t11 + t12;
        v2 = t12 - t11;
        v4 = src[8 * 7];
        v5 = src[8 * 1];
        v6 = src[8 * 5];
        v7 = src[8 * 3];
        t10 = v5 - v4;
        t11 = v5 + v4;
        t12 = v6 - v7;
        v7 += v6;
        v5 = (t11 - v7) * M13 >> 12;
        v7 += t11;
        t13 = (t10 + t12) * M5 >> 12;
        v4 = t13 - (t10 * M2 >> 12);
        v6 = t13 - (t12 * M4 >> 12) - v7;
        v5 -= v6;
        v4 -= v5;
        src[8 * 0] = v0 + v7;
        src[8 * 7] = v0 - v7;
        src[8 * 1] = v1 + v6;
        src[8 * 6] = v1 - v6;
        src[8 * 2] = v2 + v5;
        src[8 * 5] = v2 - v5;
        src[8 * 3] = v3 + v4;
        src[8 * 4] = v3 - v4;
        src++;
    }
    /* 行处理 */
    src -= 8;
    for (i = 0; i < 8; i++) {
        v0 = src[0] + (128L << 8);
        v1 = src[2];
        v2 = src[4];
        v3 = src[6];
        t10 = v0 + v2;
        t12 = v0 - v2;
        t11 = (v1 - v3) * M13 >> 12;
        v3 += v1;
        t11 -= v3;
        v0 = t10 + v3;
        v3 = t10 - v3;
        v1 = t11 + t12;
        v2 = t12 - t11;
        v4 = src[7];
        v5 = src[1];
        v6 = src[5];
        v7 = src[3];
        t10 = v5 - v4;
        t11 = v5 + v4;
        t12 = v6 - v7;
        v7 += v6;
        v5 = (t11 - v7) * M13 >> 12;
        v7 += t11;
        t13 = (t10 + t12) * M5 >> 12;
        v4 = t13 - (t10 * M2 >> 12);
        v6 = t13 - (t12 * M4 >> 12) - v7;
        v5 -= v6;
        v4 -= v5;
        dst[0] = BYTECLIP((v0 + v7) >> 8);
        dst[7] = BYTECLIP((v0 - v7) >> 8);
        dst[1] = BYTECLIP((v1 + v6) >> 8);
        dst[6] = BYTECLIP((v1 - v6) >> 8);
        dst[2] = BYTECLIP((v2 + v5) >> 8);
        dst[5] = BYTECLIP((v2 - v5) >> 8);
        dst[3] = BYTECLIP((v3 + v4) >> 8);
        dst[4] = BYTECLIP((v3 - v4) >> 8);
        dst += 8;
        src += 8;
    }
}

/*-----------------------------------------------------------------------*/
/* 加载MCU块 */
/*-----------------------------------------------------------------------*/
static JRESULT mcu_load(JDEC* jd) {
    int32_t *tmp = (int32_t*)jd->workbuf;
    int b, d, e;
    uint16_t blk, nby, nbc, i, z, id, cmp;
    uint8_t *bp;
    const uint8_t *hb, *hd;
    const uint16_t *hc;
    const int32_t *dqf;
    nby = jd->msx * jd->msy;
    nbc = 2;
    bp = jd->mcubuf;
    for (blk = 0; blk < nby + nbc; blk++) {
        cmp = (blk < nby) ? 0 : blk - nby + 1;
        id = cmp ? 1 : 0;
        /* 解码DC分量 */
        hb = jd->huffbits[id][0];
        hc = jd->huffcode[id][0];
        hd = jd->huffdata[id][0];
        b = huffext(jd, hb, hc, hd);
        if (b < 0) return 0 - b;
        d = jd->dcv[cmp];
        if (b) {
            e = bitext(jd, b);
            if (e < 0) return 0 - e;
            b = 1 << (b - 1);
            if (!(e & b)) e -= (b << 1) - 1;
            d += e;
            jd->dcv[cmp] = (int16_t)d;
        }
        dqf = jd->qttbl[jd->qtid[cmp]];
        tmp[0] = d * dqf[0] >> 8;
        /* 解码AC分量 */
        for (i = 1; i < 64; tmp[i++] = 0);
        hb = jd->huffbits[id][1];
        hc = jd->huffcode[id][1];
        hd = jd->huffdata[id][1];
        i = 1;
        do {
            b = huffext(jd, hb, hc, hd);
            if (b == 0) break;
            if (b < 0) return 0 - b;
            z = (uint16_t)b >> 4;
            if (z) {
                i += z;
                if (i >= 64) return JDR_FMT1;
            }
            if (b &= 0x0F) {
                d = bitext(jd, b);
                if (d < 0) return 0 - d;
                b = 1 << (b - 1);
                if (!(d & b)) d -= (b << 1) - 1;
                z = ZIG(i);
                tmp[z] = d * dqf[z] >> 8;
            }
        } while (++i < 64);
        /* IDCT变换 */
        if (JD_USE_SCALE && jd->scale == 3) {
            *bp = (uint8_t)((*tmp / 256) + 128);
        } else {
            block_idct(tmp, bp);
        }
        bp += 64;
    }
    return JDR_OK;
}

/*-----------------------------------------------------------------------*/
/* 输出MCU（YCbCr转RGB） */
/*-----------------------------------------------------------------------*/
static JRESULT mcu_output(JDEC* jd, uint16_t (*outfunc)(JDEC*, void*, JRECT*), uint16_t x, uint16_t y) {
    const int16_t CVACC = (sizeof(int16_t) > 2) ? 1024 : 128;
    uint16_t ix, iy, mx, my, rx, ry;
    int16_t yy, cb, cr;
    uint8_t *py, *pc, *rgb24;
    JRECT rect;
    mx = jd->msx * 8; my = jd->msy * 8;
    rx = (x + mx <= jd->width) ? mx : jd->width - x;
    ry = (y + my <= jd->height) ? my : jd->height - y;
    if (JD_USE_SCALE) {
        rx >>= jd->scale; ry >>= jd->scale;
        if (!rx || !ry) return JDR_OK;
        x >>= jd->scale; y >>= jd->scale;
    }
    rect.left = x; rect.right = x + rx - 1;
    rect.top = y; rect.bottom = y + ry - 1;
    /* YCbCr转RGB */
    if (!JD_USE_SCALE || jd->scale != 3) {
        rgb24 = (uint8_t*)jd->workbuf;
        for (iy = 0; iy < my; iy++) {
            pc = jd->mcubuf;
            py = pc + iy * 8;
            if (my == 16) {
                pc += 64 * 4 + (iy >> 1) * 8;
                if (iy >= 8) py += 64;
            } else {
                pc += mx * 8 + iy * 8;
            }
            for (ix = 0; ix < mx; ix++) {
                cb = pc[0] - 128;
                cr = pc[64] - 128;
                if (mx == 16) {
                    if (ix == 8) py += 64 - 8;
                    pc += ix & 1;
                } else {
                    pc++;
                }
                yy = *py++;
                *rgb24++ = BYTECLIP(yy + ((int16_t)(1.402 * CVACC) * cr) / CVACC);
                *rgb24++ = BYTECLIP(yy - ((int16_t)(0.344 * CVACC) * cb + (int16_t)(0.714 * CVACC) * cr) / CVACC);
                *rgb24++ = BYTECLIP(yy + ((int16_t)(1.772 * CVACC) * cb) / CVACC);
            }
        }
        /* 缩放处理 */
        if (JD_USE_SCALE && jd->scale) {
            uint16_t x, y, r, g, b, s, w, a;
            uint8_t *op;
            s = jd->scale * 2;
            w = 1 << jd->scale;
            a = (mx - w) * 3;
            op = (uint8_t*)jd->workbuf;
            for (iy = 0; iy < my; iy += w) {
                for (ix = 0; ix < mx; ix += w) {
                    rgb24 = (uint8_t*)jd->workbuf + (iy * mx + ix) * 3;
                    r = g = b = 0;
                    for (y = 0; y < w; y++) {
                        for (x = 0; x < w; x++) {
                            r += *rgb24++;
                            g += *rgb24++;
                            b += *rgb24++;
                        }
                        rgb24 += a;
                    }
                    *op++ = (uint8_t)(r >> s);
                    *op++ = (uint8_t)(g >> s);
                    *op++ = (uint8_t)(b >> s);
                }
            }
        }
    } else {
        rgb24 = (uint8_t*)jd->workbuf;
        pc = jd->mcubuf + mx * my;
        cb = pc[0] - 128;
        cr = pc[64] - 128;
        for (iy = 0; iy < my; iy += 8) {
            py = jd->mcubuf;
            if (iy == 8) py += 64 * 2;
            for (ix = 0; ix < mx; ix += 8) {
                yy = *py;
                py += 64;
                *rgb24++ = BYTECLIP(yy + ((int16_t)(1.402 * CVACC) * cr / CVACC));
                *rgb24++ = BYTECLIP(yy - ((int16_t)(0.344 * CVACC) * cb + (int16_t)(0.714 * CVACC) * cr) / CVACC);
                *rgb24++ = BYTECLIP(yy + ((int16_t)(1.772 * CVACC) * cb / CVACC));
            }
        }
    }
    /* 裁剪像素 */
    mx >>= jd->scale;
    if (rx < mx) {
        uint8_t *s, *d;
        uint16_t x, y;
        s = d = (uint8_t*)jd->workbuf;
        for (y = 0; y < ry; y++) {
            for (x = 0; x < rx; x++) {
                *d++ = *s++;
                *d++ = *s++;
                *d++ = *s++;
            }
            s += (mx - rx) * 3;
        }
    }
    /* 转RGB565 */
    if (JD_FORMAT == 1) {
        uint8_t *s = (uint8_t*)jd->workbuf;
        uint16_t w, *d = (uint16_t*)s;
        uint16_t n = rx * ry;
        do {
            w = (*s++ & 0xF8) << 8;
            w |= (*s++ & 0xFC) << 3;
            w |= *s++ >> 3;
            *d++ = w;
        } while (--n);
    }
    /* 调用输出回调 */
    return outfunc(jd, jd->workbuf, &rect) ? JDR_OK : JDR_INTR;
}

/*-----------------------------------------------------------------------*/
/* 处理重启间隔 */
/*-----------------------------------------------------------------------*/
static JRESULT restart(JDEC* jd, uint16_t rstn) {
    uint16_t i, dc;
    uint16_t d;
    uint8_t *dp;
    dp = jd->dptr; dc = jd->dctr;
    d = 0;
    for (i = 0; i < 2; i++) {
        if (!dc) {
            dp = jd->inbuf;
            dc = jd->infunc(jd, dp, JD_SZBUF);
            if (!dc) return JDR_INP;
        } else {
            dp++;
        }
        dc--;
        d = (d << 8) | *dp;
    }
    jd->dptr = dp; jd->dctr = dc; jd->dmsk = 0;
    if ((d & 0xFFD8) != 0xFFD0 || (d & 7) != (rstn & 7)) {
        return JDR_FMT1;
    }
    jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;
    return JDR_OK;
}

/*-----------------------------------------------------------------------*/
/* 初始化解码器 */
/*-----------------------------------------------------------------------*/
#define LDB_WORD(ptr) (uint16_t)(((uint16_t)*((uint8_t*)(ptr))<<8)|(uint16_t)*(uint8_t*)((ptr)+1))  // 修复语法错误
JRESULT jd_prepare(JDEC* jd, uint16_t (*infunc)(JDEC*, uint8_t*, uint16_t), void* pool, uint16_t sz_pool, void* dev) {
    uint8_t *seg, b;
    uint16_t marker;
    uint32_t ofs;
    uint16_t n, i, j, len;
    JRESULT rc;
    if (!pool) return JDR_PAR;
    jd->pool = pool;
    jd->sz_pool = sz_pool;
    jd->infunc = infunc;
    jd->device = dev;
    jd->nrst = 0;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            jd->huffbits[i][j] = 0;
            jd->huffcode[i][j] = 0;
            jd->huffdata[i][j] = 0;
        }
    }
    for (i = 0; i < 4; jd->qttbl[i++] = 0);
    jd->inbuf = seg = alloc_pool(jd, JD_SZBUF);
    if (!seg) return JDR_MEM1;
    if (jd->infunc(jd, seg, 2) != 2) return JDR_INP;
    if (LDB_WORD(seg) != 0xFFD8) return JDR_FMT1;
    ofs = 2;
    for (;;) {
        if (jd->infunc(jd, seg, 4) != 4) return JDR_INP;
        marker = LDB_WORD(seg);
        len = LDB_WORD(seg + 2);
        if (len <= 2 || (marker >> 8) != 0xFF) return JDR_FMT1;
        len -= 2;
        ofs += 4 + len;
        switch (marker & 0xFF) {
            case 0xC0:  /* SOF0 */
                if (len > JD_SZBUF) return JDR_MEM2;
                if (jd->infunc(jd, seg, len) != len) return JDR_INP;
                jd->width = LDB_WORD(seg+3);
                jd->height = LDB_WORD(seg+1);
                if (seg[5] != 3) return JDR_FMT3;
                for (i = 0; i < 3; i++) {
                    b = seg[7 + 3 * i];
                    if (!i) {
                        if (b != 0x11 && b != 0x22 && b != 0x21) {
                            return JDR_FMT3;
                        }
                        jd->msx = b >> 4; jd->msy = b & 15;
                    } else {
                        if (b != 0x11) return JDR_FMT3;
                    }
                    b = seg[8 + 3 * i];
                    if (b > 3) return JDR_FMT3;
                    jd->qtid[i] = b;
                }
                break;
            case 0xDD:  /* DRI */
                if (len > JD_SZBUF) return JDR_MEM2;
                if (jd->infunc(jd, seg, len) != len) return JDR_INP;
                jd->nrst = LDB_WORD(seg);
                break;
            case 0xC4:  /* DHT */
                if (len > JD_SZBUF) return JDR_MEM2;
                if (jd->infunc(jd, seg, len) != len) return JDR_INP;
                rc = create_huffman_tbl(jd, seg, len);
                if (rc) return rc;
                break;
            case 0xDB:  /* DQT */
                if (len > JD_SZBUF) return JDR_MEM2;
                if (jd->infunc(jd, seg, len) != len) return JDR_INP;
                rc = create_qt_tbl(jd, seg, len);
                if (rc) return rc;
                break;
            case 0xDA:  /* SOS */
                if (len > JD_SZBUF) return JDR_MEM2;
                if (jd->infunc(jd, seg, len) != len) return JDR_INP;
                if (!jd->width || !jd->height) return JDR_FMT1;
                if (seg[0] != 3) return JDR_FMT3;
                for (i = 0; i < 3; i++) {
                    b = seg[2 + 2 * i];
                    if (b != 0x00 && b != 0x11) return JDR_FMT3;
                    b = i ? 1 : 0;
                    if (!jd->huffbits[b][0] || !jd->huffbits[b][1]) {
                        return JDR_FMT1;
                    }
                    if (!jd->qttbl[jd->qtid[i]]) {
                        return JDR_FMT1;
                    }
                }
                n = jd->msy * jd->msx;
                if (!n) return JDR_FMT1;
                len = n * 64 * 2 + 64;
                if (len < 256) len = 256;
                jd->workbuf = alloc_pool(jd, len);
                if (!jd->workbuf) return JDR_MEM1;
                jd->mcubuf = (uint8_t*)alloc_pool(jd, (uint16_t)((n + 2) * 64));
                if (!jd->mcubuf) return JDR_MEM1;
                jd->dptr = seg; jd->dctr = 0; jd->dmsk = 0;
                if (ofs %= JD_SZBUF) {
                    jd->dctr = jd->infunc(jd, seg + ofs, (uint16_t)(JD_SZBUF - ofs));
                    jd->dptr = seg + ofs - 1;
                }
                return JDR_OK;
            case 0xC1: case 0xC2: case 0xC3: case 0xC5: case 0xC6: case 0xC7:
            case 0xC9: case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF: case 0xD9:
                return JDR_FMT3;
            default:
                if (jd->infunc(jd, 0, len) != len) {
                    return JDR_INP;
                }
        }
    }
}

/*-----------------------------------------------------------------------*/
/* 执行解码 */
/*-----------------------------------------------------------------------*/
JRESULT jd_decomp(JDEC* jd, uint16_t (*outfunc)(JDEC*, void*, JRECT*), uint8_t scale) {
    uint16_t x, y, mx, my;
    uint16_t rst, rsc;
    JRESULT rc;
    if (scale > (JD_USE_SCALE ? 3 : 0)) return JDR_PAR;
    jd->scale = scale;
    mx = jd->msx * 8; my = jd->msy * 8;
    jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0;
    rst = rsc = 0;
    rc = JDR_OK;
    for (y = 0; y < jd->height; y += my) {
        for (x = 0; x < jd->width; x += mx) {
            if (jd->nrst && rst++ == jd->nrst) {
                rc = restart(jd, rsc++);
                if (rc != JDR_OK) return rc;
                rst = 1;
            }
            rc = mcu_load(jd);
            if (rc != JDR_OK) return rc;
            rc = mcu_output(jd, outfunc, x, y);
            if (rc != JDR_OK) return rc;
        }
    }
    return rc;
}