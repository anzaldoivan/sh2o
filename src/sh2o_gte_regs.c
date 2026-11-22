/* GTE register reads and writes (MFC2/CFC2, MTC2/CTC2).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* New code from psx-spx's register descriptions; the read view is the model's read_view. */
#include <stdint.h>

#include "sh2o_gte_internal.h"
#include "sh2o_count_internal.h"

/* IRGB/ORGB channel: IR SAR 7 saturated 0..1Fh */
static uint32_t sh2oi_orgb5(uint32_t ir)
{
    int32_t x = sh2oi_asr(sh2oi_s16(ir),7);
    return (uint32_t)(x < 0 ? 0 : x > 0x1F ? 0x1F : x);
}

/* IRGB/ORGB read: IR1-3 SAR 7 saturated 0..1Fh, packed 5:5:5 */
static uint32_t sh2oi_orgb(const sh2o_gte_t *g)
{
    return sh2oi_orgb5(g->r[9]) | sh2oi_orgb5(g->r[10]) << 5 | sh2oi_orgb5(g->r[11]) << 10;
}

/* read kinds by register: a table, not a switch (gcc lowers a sparse switch to bit tests, a variable shift) */
enum
{
    SH2OI_FULL,
    SH2OI_SEXT,
    SH2OI_ZEXT,
    SH2OI_SXYP,
    SH2OI_ORGB,
    SH2OI_LZCR,
    SH2OI_FLAG
};
static const uint8_t sh2oi_kind[64] = {
    [1] = SH2OI_SEXT,[3] = SH2OI_SEXT,[5] = SH2OI_SEXT,                     /* VZ0-2 */
    [8] = SH2OI_SEXT,[9] = SH2OI_SEXT,[10] = SH2OI_SEXT,[11] = SH2OI_SEXT, /* IR0-3 */
    [36] = SH2OI_SEXT,[44] = SH2OI_SEXT,[52] = SH2OI_SEXT,                    /* RT33, L33, LB3 */
    [58] = SH2OI_SEXT,[59] = SH2OI_SEXT,[61] = SH2OI_SEXT,[62] = SH2OI_SEXT, /* H (hw), DQA, ZSF3, ZSF4 */
    [7] = SH2OI_ZEXT,[16] = SH2OI_ZEXT,[17] = SH2OI_ZEXT,[18] = SH2OI_ZEXT,[19] = SH2OI_ZEXT, /* OTZ, SZ0-3 */
    [15] = SH2OI_SXYP,[28] = SH2OI_ORGB,[29] = SH2OI_ORGB,[31] = SH2OI_LZCR,[63] = SH2OI_FLAG,
};

/* Contract tag gte-regs-1. Contract in sh2o/gte.h. */
uint32_t sh2o_gte_read(const sh2o_gte_t *g,unsigned reg)
{
    uint32_t w;
    unsigned k;
    if(reg > 63)
        return 0;
    w = g->r[reg];
    k = sh2oi_kind[reg];
    if(k == SH2OI_FULL)
        return w;
    if(k == SH2OI_SEXT)
        return (uint32_t)sh2oi_s16(w);
    if(k == SH2OI_ZEXT)
        return w & 0xFFFFu;
    if(k == SH2OI_SXYP) /* SXYP mirrors SXY2 */
        return g->r[14];
    if(k == SH2OI_ORGB) /* IRGB, ORGB */
        return sh2oi_orgb(g);
    if(k == SH2OI_LZCR)
    { /* LZCR of LZCS */
        uint32_t v = g->r[30];
        return sh2o_k_clz32(v & 0x80000000u ? ~v : v);
    }
    w &= 0x7FFFF000u; /* FLAG */
    return w | (w & SH2OI_FLAG_B31 ? 0x80000000u : 0u);
}

/* Contract tag gte-regs-1. Contract in sh2o/gte.h. */
void sh2o_gte_write(sh2o_gte_t *g,unsigned reg,uint32_t v)
{
    SH2OI_COUNT_WRITE(reg);
    switch(reg)
    {
        case 15: /* SXYP: push the SXY FIFO */
            g->r[12] = g->r[13];
            g->r[13] = g->r[14];
            g->r[14] = v;
            return;
        case 28: /* IRGB: IR1-3 = 5-bit fields << 7 */
            g->r[28] = v;
            g->r[9] = (v & 0x1Fu) << 7;
            g->r[10] = (v >> 5 & 0x1Fu) << 7;
            g->r[11] = (v >> 10 & 0x1Fu) << 7;
            return;
        case 29:
        case 31: /* ORGB, LZCR: read-only */ return;
        case 63: /* FLAG: bits 12-30 */ g->r[63] = v & 0x7FFFF000u; return;
        default:
            if(reg < 64)
                g->r[reg] = v;
            return;
    }
}
