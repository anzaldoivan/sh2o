/* C references of the fixed-point kernels.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Written as the SH-2 MAC unit works, a 64-bit sum in two 32-bit words (hi = MACH, lo = MACL) with an explicit
 * carry, so the SH-2 build needs no libgcc helper. The SH-2 has no barrel
 * shifter: no variable shift and no arithmetic right shift by more than 1 here (each a libgcc call), only constant
 * logical shifts. Included by sh2o/k.h. */
#ifndef SH2O_K_SW_INL_H
#define SH2O_K_SW_INL_H

#include <stdint.h>

/* hi:lo += p, p a signed 32-bit product. Forced inline: GCC 13.2 leaves it out of line at -Os (a call). */
SH2OI_FORCE_INLINE void sh2oi_k_mac_c(uint32_t *hi,uint32_t *lo,int32_t p) {
    uint32_t u = (uint32_t)p;
    uint32_t c;
    *lo += u;
    c = *lo < u;                    /* carry out of the low word */
    *hi += (uint32_t)(p >> 31) + c; /* the product's sign word, plus the carry */
}

/* Contract tag k-dot3b-1.
 * sh2o_k_dot3b: ((int64)tr << 12) + m[0]*v[0] + m[1]*v[1] + m[2]*v[2]; contract in sh2o/k.h */
SH2OI_FORCE_INLINE int64_t sh2o_k_dot3b_c(const int16_t m[3],const int16_t v[3],int32_t tr) {
    uint32_t lo = (uint32_t)tr << 12;                /* MACL: tr << 12 */
    uint32_t sx = 0u - ((uint32_t)tr >> 31);         /* all ones when tr < 0 */
    uint32_t hi = ((uint32_t)tr >> 20) | (sx << 12); /* MACH: tr >> 20, sign-extended (no libgcc shift) */
    sh2oi_k_mac_c(&hi,&lo,(int32_t)m[0] * v[0]);
    sh2oi_k_mac_c(&hi,&lo,(int32_t)m[1] * v[1]);
    sh2oi_k_mac_c(&hi,&lo,(int32_t)m[2] * v[2]);
    return (int64_t)(((uint64_t)hi << 32) | lo);
}

/* Contract tag k-mvb3-1.
 * sh2o_k_mvb3: out[i] = ((int64)t[i] << 12) + row i . v, the rows from five words (sh2oi_mat's halves); contract in
 * sh2o/k.h */
SH2OI_FORCE_INLINE void sh2o_k_mvb3_c(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) {
    unsigned i;
    int16_t m[9];
    for(i = 0; i < 4; i++) {
        m[2 * i] = (int16_t)(uint16_t)w[i];             /* low half */
        m[2 * i + 1] = (int16_t)(uint16_t)(w[i] >> 16); /* high half */
    }
    m[8] = (int16_t)(uint16_t)w[4];
    for(i = 0; i < 3; i++) { /* sh2o_k_dot3b_c's row, written once (one call site: inlined at -Os) */
        const int16_t *r = &m[3 * i];
        uint32_t lo = (uint32_t)t[i] << 12;
        uint32_t hi = ((uint32_t)t[i] >> 20) | ((0u - ((uint32_t)t[i] >> 31)) << 12);
        sh2oi_k_mac_c(&hi,&lo,(int32_t)r[0] * v[0]);
        sh2oi_k_mac_c(&hi,&lo,(int32_t)r[1] * v[1]);
        sh2oi_k_mac_c(&hi,&lo,(int32_t)r[2] * v[2]);
        out[i] = (int64_t)(((uint64_t)hi << 32) | lo);
    }
}

/* Contract tag k-gpf3-1.
 * sh2o_k_gpf3: out[i] = IR(i+1) * IR0, the IRs the low halves of w[0..3] (IR0..IR3 words); contract in sh2o/k.h */
SH2OI_FORCE_INLINE void sh2o_k_gpf3_c(const uint32_t w[4],int64_t out[3]) {
    int32_t ir0 = (int16_t)(uint16_t)w[0];
    unsigned i;
    for(i = 0; i < 3; i++) {
        int32_t p = (int16_t)(uint16_t)w[1 + i] * ir0; /* |p| <= 2^30 */
        out[i] = (int64_t)(((uint64_t)(0u - ((uint32_t)p >> 31)) << 32) | (uint32_t)p);
    }
}

/* Contract tag k-gpl3-1.
 * sh2o_k_gpl3: out[i] = ((int64)mac[i] << 12 when sf, else mac[i]) + IR(i+1) * IR0; contract in sh2o/k.h */
SH2OI_FORCE_INLINE void sh2o_k_gpl3_c(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]) {
    int32_t ir0 = (int16_t)(uint16_t)w[0];
    unsigned i;
    for(i = 0; i < 3; i++) {
        uint32_t sx = 0u - (mac[i] >> 31); /* the addend's sign word */
        uint32_t lo = sf ? mac[i] << 12 : mac[i];
        uint32_t hi = sf ? (mac[i] >> 20) | (sx << 12) : sx;
        sh2oi_k_mac_c(&hi,&lo,(int16_t)(uint16_t)w[1 + i] * ir0);
        out[i] = (int64_t)(((uint64_t)hi << 32) | lo);
    }
}

/* Contract tag k-clz32-1.
 * sh2o_k_clz32: leading zero count of v, 0..32 (32 for v = 0); contract in sh2o/k.h */
SH2OI_INLINE unsigned sh2o_k_clz32_c(uint32_t v) {
    static const uint8_t clz4[16] SH2OI_RODATA(sh2oi_k_clz4) = {4,3,2,2,1,1,1,1,0,0,0,0,0,0,0,0};
    unsigned n = 0;
    if(v < 0x10000u) {
        n += 16;
        v <<= 16;
    }
    if(v < 0x1000000u) {
        n += 8;
        v <<= 8;
    }
    if(v < 0x10000000u) {
        n += 4;
        v <<= 4;
    }
    return n + clz4[v >> 28];
}

/* Contract tag k-unr-recip-1.
 * sh2o_k_unr_recip: the UNR reciprocal of a normalised divisor d (8000h..FFFFh); contract in sh2o/k.h */
SH2OI_INLINE uint32_t sh2o_k_unr_recip_c(uint32_t d) {
    /* unr[i] = max(0, (40000h / (i + 100h) + 1) / 2 - 101h), i 0..100h (psx-spx, GTE division); generated from
     * the formula */
    static const uint8_t unr[0x101] SH2OI_RODATA(sh2oi_k_unr) = {
        0xFF,0xFD,0xFB,0xF9,0xF7,0xF5,0xF3,0xF1,0xEF,0xEE,0xEC,0xEA,0xE8,0xE6,0xE4,0xE3,0xE1,0xDF,
        0xDD,0xDC,0xDA,0xD8,0xD6,0xD5,0xD3,0xD1,0xD0,0xCE,0xCD,0xCB,0xC9,0xC8,0xC6,0xC5,0xC3,0xC1,
        0xC0,0xBE,0xBD,0xBB,0xBA,0xB8,0xB7,0xB5,0xB4,0xB2,0xB1,0xB0,0xAE,0xAD,0xAB,0xAA,0xA9,0xA7,
        0xA6,0xA4,0xA3,0xA2,0xA0,0x9F,0x9E,0x9C,0x9B,0x9A,0x99,0x97,0x96,0x95,0x94,0x92,0x91,0x90,
        0x8F,0x8D,0x8C,0x8B,0x8A,0x89,0x87,0x86,0x85,0x84,0x83,0x82,0x81,0x7F,0x7E,0x7D,0x7C,0x7B,
        0x7A,0x79,0x78,0x77,0x75,0x74,0x73,0x72,0x71,0x70,0x6F,0x6E,0x6D,0x6C,0x6B,0x6A,0x69,0x68,
        0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,0x5F,0x5E,0x5D,0x5D,0x5C,0x5B,0x5A,0x59,0x58,0x57,
        0x56,0x55,0x54,0x53,0x53,0x52,0x51,0x50,0x4F,0x4E,0x4D,0x4D,0x4C,0x4B,0x4A,0x49,0x48,0x48,
        0x47,0x46,0x45,0x44,0x43,0x43,0x42,0x41,0x40,0x3F,0x3F,0x3E,0x3D,0x3C,0x3C,0x3B,0x3A,0x39,
        0x39,0x38,0x37,0x36,0x36,0x35,0x34,0x33,0x33,0x32,0x31,0x31,0x30,0x2F,0x2E,0x2E,0x2D,0x2C,
        0x2C,0x2B,0x2A,0x2A,0x29,0x28,0x28,0x27,0x26,0x26,0x25,0x24,0x24,0x23,0x22,0x22,0x21,0x20,
        0x20,0x1F,0x1E,0x1E,0x1D,0x1D,0x1C,0x1B,0x1B,0x1A,0x19,0x19,0x18,0x18,0x17,0x16,0x16,0x15,
        0x15,0x14,0x14,0x13,0x12,0x12,0x11,0x11,0x10,0x0F,0x0F,0x0E,0x0E,0x0D,0x0D,0x0C,0x0C,0x0B,
        0x0A,0x0A,0x09,0x09,0x08,0x08,0x07,0x07,0x06,0x06,0x05,0x05,0x04,0x04,0x03,0x03,0x02,0x02,
        0x01,0x01,0x00,0x00,0x00,
    };
    uint32_t u = (uint32_t)unr[(d - 0x7FC0u) >> 7] + 0x101u;
    uint32_t r = (0x2000080u - d * u) >> 8; /* >= FF0181h for every d of the domain: unsigned */
    return (0x80u + r * u) >> 8;
}

/* Contract tag k-unr-div-1.
 * sh2o_k_unr_div: the GTE's RTPS/RTPT divide for h < 2 sz; contract in sh2o/k.h */
SH2OI_INLINE uint32_t sh2o_k_unr_div_c(uint32_t h,uint32_t sz) {
    /* 1 << z by table: the SH-2 has no variable shift instruction (gcc calls a libgcc helper) */
    static const uint32_t pow2[16] SH2OI_RODATA(sh2oi_k_pow2) = {1,2,4,8,0x10,0x20,0x40,0x80,
                                    0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000};
    uint32_t f = pow2[(sh2o_k_clz32_c(sz) - 16u) & 15u];         /* normalisation: sz << z in 8000h..FFFFh */
    uint64_t p = (uint64_t)(h * f) * sh2o_k_unr_recip_c(sz * f); /* 32x32 widening */
    uint32_t lo = (uint32_t)p;
    uint32_t hi = (uint32_t)(p >> 32);
    uint32_t q;
    lo += 0x8000u;
    hi += lo < 0x8000u;          /* carry of the rounding add */
    q = (hi << 16) | (lo >> 16); /* >> 16 in two words (no libgcc shift) */
    return q > 0x1FFFFu ? 0x1FFFFu : q;
}

#endif
