/* Internal helpers of the GTE module.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_GTE_INTERNAL_H
#define SH2O_GTE_INTERNAL_H

#include <stdint.h>

#include "sh2o_internal.h"
#include "sh2o/gte.h"
#include "sh2o/k.h"

/* Names here take the sh2oi_ prefix, macros SH2OI_. Includes only <stdint.h> and sh2o headers. G's helpers:
 * field getters, the 44-bit MAC partial and the clamps, each with the FLAG bit psx-spx gives it. 64-bit values are
 * only added, compared and split into words here (no libgcc shift or multiply on the SH-2). Shifts: constant and
 * logical only (the SH-2 calls libgcc for a variable shift or an arithmetic right shift by more than 1); FLAG bits
 * by table, arithmetic shifts through sh2oi_asr. */

/* SH2O_ZAM 1: G's MAC stage (MVMVA mx < 3, cv != 2; RTPS, RTPT) tries the zam-1 delegate when sf = 1;
 * 0 (default, the core): G's own kernel only */
#ifndef SH2O_ZAM
#define SH2O_ZAM 0
#endif

/* SH2O_MVB3_SWITCH 1 (test-only, the SH-2 bench): G's MAC stage path chosen at run time (sh2o_gte.c
 * sh2o_gte_mvb3_mode; MVMVA/RTPS/RTPT's sh2oi_mac3 and GPF/GPL's); 0 (default, the core): the dispatch
 * line only */
#ifndef SH2O_MVB3_SWITCH
#define SH2O_MVB3_SWITCH 0
#endif

/* FLAG bits 30-23 and 18-13: ORed into bit 31 (psx-spx) */
#define SH2OI_FLAG_B31 0x7F87E000u

/* One command's working state: the image, FLAG accumulated, sf (1: SAR 12) and lm from the command word */
typedef struct sh2oi_cx
{
    sh2o_gte_t *g;
    uint32_t flag;
    unsigned sf;
    unsigned lm;
} sh2oi_cx;

/* field getters */
SH2OI_INLINE int32_t sh2oi_s16(uint32_t w) /* low half, sign-extended */
{
    return (int32_t)((w & 0xFFFFu) ^ 0x8000u) - 0x8000;
}

SH2OI_INLINE int32_t sh2oi_h16(uint32_t w) /* high half, sign-extended */
{
    return sh2oi_s16(w >> 16);
}

/* vector n (0-2): VXn, VYn, VZn */
SH2OI_INLINE void sh2oi_vec(const sh2o_gte_t *g,unsigned n,int32_t v[3])
{
    v[0] = sh2oi_s16(g->r[2 * n]);
    v[1] = sh2oi_h16(g->r[2 * n]);
    v[2] = sh2oi_s16(g->r[2 * n + 1]);
}

/* 3x3 matrix of five words from base (32 RT, 40 light matrix, 48 LCM), row-major halves */
SH2OI_INLINE void sh2oi_mat(const sh2o_gte_t *g,unsigned base,int32_t m[3][3])
{
    const uint32_t *w = &g->r[base];
    m[0][0] = sh2oi_s16(w[0]);
    m[0][1] = sh2oi_h16(w[0]);
    m[0][2] = sh2oi_s16(w[1]);
    m[1][0] = sh2oi_h16(w[1]);
    m[1][1] = sh2oi_s16(w[2]);
    m[1][2] = sh2oi_h16(w[2]);
    m[2][0] = sh2oi_s16(w[3]);
    m[2][1] = sh2oi_h16(w[3]);
    m[2][2] = sh2oi_s16(w[4]);
}

/* RGBC colour bytes R, G, B */
SH2OI_INLINE void sh2oi_rgb(const sh2o_gte_t *g,int32_t c[3])
{
    c[0] = (int32_t)(g->r[6] & 0xFFu);
    c[1] = (int32_t)(g->r[6] >> 8 & 0xFFu);
    c[2] = (int32_t)(g->r[6] >> 16 & 0xFFu);
}

/* three 32-bit words from base (37 TR, 45 BK, 53 FC) */
SH2OI_INLINE void sh2oi_cvec(const sh2o_gte_t *g,unsigned base,int32_t t[3])
{
    t[0] = (int32_t)g->r[base];
    t[1] = (int32_t)g->r[base + 1];
    t[2] = (int32_t)g->r[base + 2];
}

/* x SAR n (n a constant 1..31) by logical shifts: bias to unsigned, shift, unbias */
SH2OI_FORCE_INLINE int32_t sh2oi_asr(int32_t x,unsigned n)
{
    return (int32_t)((((uint32_t)x ^ 0x80000000u) >> n) - (0x80000000u >> n));
}

/* 64-bit values as two words */
SH2OI_INLINE int64_t sh2oi_w64(int32_t hi,uint32_t lo)
{
    return (int64_t)(((uint64_t)(uint32_t)hi << 32) | lo);
}

SH2OI_INLINE int32_t sh2oi_hi32(int64_t v)
{
    return (int32_t)(uint32_t)((uint64_t)v >> 32);
}

/* low word of v SAR 12, SAR 16 */
SH2OI_INLINE int32_t sh2oi_sar12(int64_t v)
{
    return (int32_t)(((uint32_t)v >> 12) | ((uint32_t)sh2oi_hi32(v) << 20));
}

SH2OI_INLINE int32_t sh2oi_sar16(int64_t v)
{
    return (int32_t)(((uint32_t)v >> 16) | ((uint32_t)sh2oi_hi32(v) << 16));
}

/* (int64)t << 12: high word t SAR 20 from the sign word (as sh2o_k_dot3b_c) */
SH2OI_INLINE int64_t sh2oi_shl12(int32_t t)
{
    uint32_t sx = 0u - ((uint32_t)t >> 31);
    return sh2oi_w64((int32_t)(((uint32_t)t >> 20) | (sx << 12)),(uint32_t)t << 12);
}

/* flagged arithmetic */
/* MAC1-3 partial (i 0-2): FLAG 30-i when v >= 2^43, 27-i when v < -2^43; truncated to 44 bits */
SH2OI_INLINE int64_t sh2oi_mac44(sh2oi_cx *cx,unsigned i,int64_t v)
{
    static const uint32_t pos[3] = {1u << 30,1u << 29,1u << 28},neg[3] = {1u << 27,1u << 26,1u << 25};
    int32_t hi = sh2oi_hi32(v);
    if(hi >= 0x800)
        cx->flag |= pos[i];
    else if(hi < -0x800)
        cx->flag |= neg[i];
    hi = (int32_t)(((uint32_t)hi & 0xFFFu) ^ 0x800u) - 0x800;
    return sh2oi_w64(hi,(uint32_t)v);
}

/* MAC0 sum: FLAG 16 above 7FFFFFFFh, 15 below -80000000h; v unchanged */
SH2OI_INLINE int64_t sh2oi_mac0(sh2oi_cx *cx,int64_t v)
{
    if(v > INT64_C(0x7FFFFFFF))
        cx->flag |= 1u << 16;
    else if(v < -INT64_C(0x80000000))
        cx->flag |= 1u << 15;
    return v;
}

/* MAC register from a 44-bit sum: SAR sf, low 32 bits */
SH2OI_INLINE int32_t sh2oi_mac_sf(const sh2oi_cx *cx,int64_t v)
{
    return cx->sf ? sh2oi_sar12(v) : (int32_t)(uint32_t)v;
}

/* IR1-3 (i 0-2) from v: -8000h (0 with lm)..7FFFh, FLAG 24-i */
SH2OI_INLINE int32_t sh2oi_sat_ir(sh2oi_cx *cx,unsigned i,int32_t v,unsigned lm)
{
    static const uint32_t bit[3] = {1u << 24,1u << 23,1u << 22};
    int32_t lo = lm ? 0 : -0x8000;
    if(v < lo)
    {
        cx->flag |= bit[i];
        return lo;
    }
    if(v > 0x7FFF)
    {
        cx->flag |= bit[i];
        return 0x7FFF;
    }
    return v;
}

/* MAC1-3 = sums SAR sf; IR1-3 saturated from them (lm) */
SH2OI_INLINE void sh2oi_set_mac_ir(sh2oi_cx *cx,const int64_t a[3],unsigned lm)
{
    unsigned i;
    for(i = 0; i < 3; i++)
    {
        int32_t mac = sh2oi_mac_sf(cx,a[i]);
        cx->g->r[25 + i] = (uint32_t)mac;
        cx->g->r[9 + i] = (uint32_t)sh2oi_sat_ir(cx,i,mac,lm);
    }
}

/* colour FIFO channel: MAC SAR 4 saturated 0..FFh, FLAG bit when saturated */
SH2OI_INLINE uint32_t sh2oi_col(sh2oi_cx *cx,uint32_t mac,uint32_t bit)
{
    int32_t v = sh2oi_asr((int32_t)mac,4);
    if(v < 0 || v > 0xFF)
    {
        cx->flag |= bit;
        v = v < 0 ? 0 : 0xFF;
    }
    return (uint32_t)v;
}

/* Colour FIFO: RGB0 <- RGB1 <- RGB2 <- [MAC1-3 SAR 4 saturated 0..FFh (FLAG 21-19), CODE] */
SH2OI_INLINE void sh2oi_push_rgb(sh2oi_cx *cx)
{
    sh2o_gte_t *g = cx->g;
    uint32_t out = (g->r[6] & 0xFF000000u) | sh2oi_col(cx,g->r[25],1u << 21) |
                    sh2oi_col(cx,g->r[26],1u << 20) << 8 | sh2oi_col(cx,g->r[27],1u << 19) << 16;
    g->r[20] = g->r[21];
    g->r[21] = g->r[22];
    g->r[22] = out;
}

/* SZ FIFO: SZ0 <- SZ1 <- SZ2 <- SZ3 <- v saturated 0..FFFFh (FLAG 18) */
SH2OI_INLINE void sh2oi_push_sz(sh2oi_cx *cx,int32_t v)
{
    sh2o_gte_t *g = cx->g;
    if(v < 0 || v > 0xFFFF)
    {
        cx->flag |= 1u << 18;
        v = v < 0 ? 0 : 0xFFFF;
    }
    g->r[16] = g->r[17];
    g->r[17] = g->r[18];
    g->r[18] = g->r[19];
    g->r[19] = (uint32_t)v;
}

#endif /* SH2O_GTE_INTERNAL_H */
