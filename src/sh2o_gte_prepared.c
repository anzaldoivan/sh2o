/* MVMVA with a prepared shadow of the matrices and translations.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Contract tag gte-mvmva-prepared-1.
 * SH2ZAM guide: saturn/BATCH_TRANSFORMS.md:193-206 and :29-58 at 7f99eb70 (MAC.L rows closed by XTRCT, the SR.S
 * read, the plan prepared at the load). SH2ZAM is not linked: its transform is Q16.16 over a point array and has
 * no GTE tail; this kernel must give the GTE bytes.
 * The load makes the rows (R SHL 4) and the MACH, MACL words of t SHL 16 with 32-bit operations and constant
 * shifts only (no libgcc helper). The entry decodes the selector, reads the shadow, runs the kernel
 * (sh2oi_gte_prepared_mac3_l on the SH-2, sh2oi_gte_prepared_mac3_c elsewhere) and the sf=1 tail on int32 MACs.
 * Outside its gate it calls sh2o_gte_mvmva, the only call of this file (tests/sh2/prepared_static.txt). The file
 * has no static function: tests/test_prepared.c includes it again with each planted bug. */
#include <stdint.h>

#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o_count_internal.h"

/* Test-only planted bugs (tests/test_prepared.c includes this file with each): 1 the preload is t SHL 12, not
 * t SHL 16; 2 the rows are R SHL 3, not R SHL 4; 3 the gate takes |t| <= 7FF40000h. 0 (default): none. */
#ifndef SH2OI_PREPARED_MUTANT
#define SH2OI_PREPARED_MUTANT 0
#endif

/* Contract tag gte-mvmva-prepared-1. Contract in sh2o/gte_prepared.h. */
void sh2o_gte_prepared_load(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts)
{
    unsigned k,i,bit = 1u; /* bit: the part of k (constant shifts only: the SH-2 has no variable shift) */
    SH2OI_COUNT_NAME(SH2O_COUNT_GTE_PREPARED_LOAD);
    /* rows: matrix k from its five words at 32 + 8k, element j the low (j even) or high half of word j / 2 */
    for(k = 0; k < 3; k++,bit <<= 1)
    {
        const uint32_t *w = &g->r[32 + 8 * k];
        int32_t *c = p->c[k];
        if(!(parts & bit))
            continue;
        for(i = 0; i < 9; i++)
        {
            uint32_t h = (i & 1u) ? w[i >> 1] >> 16 : w[i >> 1];
#if SH2OI_PREPARED_MUTANT == 2
            c[i] = (int32_t)((((h & 0xFFFFu) ^ 0x8000u) - 0x8000u) << 3);
#else
            c[i] = (int32_t)((((h & 0xFFFFu) ^ 0x8000u) - 0x8000u) << 4);
#endif
        }
    }
    /* preload words and gate: translation k at 37 + 8k (k 0, 1), none (k 2) */
    for(k = 0; k < 3; k++,bit <<= 1)
    {
        uint32_t *pre = p->pre[k];
        uint8_t ok = 1;
        if(!(parts & bit))
            continue;
        for(i = 0; i < 3; i++)
        {
            uint32_t u = k < 2 ? g->r[37 + 8 * k + i] : 0u;
#if SH2OI_PREPARED_MUTANT == 1
            pre[2 * i] = (u >> 20) | ((0u - (u >> 31)) << 12);
            pre[2 * i + 1] = u << 12;
#else
            pre[2 * i] = (u >> 16) | ((0u - (u >> 31)) << 16); /* MACH: t SAR 16 */
            pre[2 * i + 1] = u << 16;                          /* MACL */
#endif
#if SH2OI_PREPARED_MUTANT == 3
            if(!(u + 0x7FF3FFFFu <= 0xFFE7FFFFu))
#else
            if(!(u + 0x7FF3FFFFu < 0xFFE7FFFFu)) /* |t| < 7FF40000h */
#endif
                ok = 0;
        }
        p->ok[k] = ok;
    }
}

/* Contract tag gte-mvmva-prepared-1. Contract in sh2o/gte_prepared.h. */
int sh2o_gte_mvmva_prepared(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd)
{
    unsigned mx = cmd >> 17 & 3u,vx = cmd >> 15 & 3u,cv = cmd >> 13 & 3u,ts = cv == 3u ? 2u : cv;
    uint32_t *r = g->r;
    uint32_t f = 0,b = 1u << 24; /* FLAG and its bit 24-i */
    int32_t lo = (cmd >> 10 & 1u) ? 0 : -0x8000; /* lower IR limit (lm) */
    int16_t v[3];
    int32_t mac[3];
    unsigned i;
    SH2OI_COUNT_NAME(SH2O_COUNT_GTE_MVMVA_PREPARED);
    if(!(cmd >> 19 & 1u) || mx == 3u || cv == 2u || !p->ok[ts]
#if SH2OI_NATIVE
        || sh2oi_k_sr_s()
#endif
    )
    {
        SH2OI_COUNT_IN();
        sh2o_gte_mvmva(g,cmd);
        SH2OI_COUNT_OUT();
        return 0;
    }
    SH2OI_COUNT_STORE(SH2O_COUNT_OP + 0x12u,1u);
    if(vx == 3u)
    {
        v[0] = (int16_t)r[9];
        v[1] = (int16_t)r[10];
        v[2] = (int16_t)r[11];
    }
    else
    {
        v[0] = (int16_t)r[2 * vx];
        v[1] = (int16_t)(r[2 * vx] >> 16);
        v[2] = (int16_t)r[2 * vx + 1];
    }
#if SH2OI_NATIVE
    sh2oi_gte_prepared_mac3_l(p->c[mx],p->pre[ts],v,mac);
#else
    sh2oi_gte_prepared_mac3_c(p->c[mx],p->pre[ts],v,mac);
#endif
    /* the sf=1 tail: MAC1-3, IR1-3 saturated (lm), FLAG 24-22; FLAG reset at the command start */
    for(i = 0; i < 3; i++)
    {
        int32_t m = mac[i];
        r[25 + i] = (uint32_t)m;
        if(m < lo)
        {
            f |= b;
            m = lo;
        }
        else if(m > 0x7FFF)
        {
            f |= b;
            m = 0x7FFF;
        }
        r[9 + i] = (uint32_t)m;
        b >>= 1;
    }
    r[63] = f;
    return 1;
}
