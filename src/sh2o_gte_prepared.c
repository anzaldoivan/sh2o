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
 * Outside its gate it calls sh2o_gte_mvmva, the only out-of-line call (tests/sh2/prepared_static.txt). */
#include <stdint.h>

#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o_count_internal.h"

/* Test-only planted bugs (tests/test_prepared.c includes this file with each): 1 the preload is t SHL 12, not
 * t SHL 16; 2 the rows are R SHL 3, not R SHL 4; 3 the gate takes |t| <= 7FF40000h. 0 (default): none. */
#ifndef SH2OI_PREPARED_MUTANT
#define SH2OI_PREPARED_MUTANT 0
#endif

/* Derive private helper names from the loader entry for the source-inclusion tests. */
#define SH2OI_LOAD_HELPER_NAME(entry,suffix) entry ## suffix
#define SH2OI_LOAD_HELPER(entry,suffix) SH2OI_LOAD_HELPER_NAME(entry,suffix)
#define sh2oi_load_rows SH2OI_LOAD_HELPER(sh2o_gte_prepared_load,_rows)
#define sh2oi_load_words SH2OI_LOAD_HELPER(sh2o_gte_prepared_load,_words)
#if SH2OI_PREPARED_MUTANT == 2
#define SH2OI_LOAD_CSHIFT 3
#else
#define SH2OI_LOAD_CSHIFT 4
#endif
#if SH2OI_PREPARED_MUTANT == 1
#define SH2OI_LOAD_TSHIFT 12
#else
#define SH2OI_LOAD_TSHIFT 16
#endif
/* Shift the signed interval |t| < 7FF40000h into an unsigned range test. */
#if SH2OI_PREPARED_MUTANT == 3
#define SH2OI_LOAD_OUT(u) ((u) + 0x7FF3FFFFu > 0xFFE7FFFFu)
#else
#define SH2OI_LOAD_OUT(u) ((u) + 0x7FF3FFFFu >= 0xFFE7FFFFu)
#endif
#define SH2OI_LOAD_ROW(w) ((int32_t)((uint32_t)(int32_t)(int16_t)(uint16_t)(w) << SH2OI_LOAD_CSHIFT))
/* Fill the high bits of the MACH word from the sign bit of the input word. */
#define SH2OI_LOAD_MACH(w) (((w) >> (32 - SH2OI_LOAD_TSHIFT)) | ((0u - ((w) >> 31)) << SH2OI_LOAD_TSHIFT))
SH2OI_FORCE_INLINE void sh2oi_load_rows(int32_t *c,const uint32_t *w) {
    uint32_t u;
    u = w[0]; c[0] = SH2OI_LOAD_ROW(u); c[1] = SH2OI_LOAD_ROW(u >> 16);
    u = w[1]; c[2] = SH2OI_LOAD_ROW(u); c[3] = SH2OI_LOAD_ROW(u >> 16);
    u = w[2]; c[4] = SH2OI_LOAD_ROW(u); c[5] = SH2OI_LOAD_ROW(u >> 16);
    u = w[3]; c[6] = SH2OI_LOAD_ROW(u); c[7] = SH2OI_LOAD_ROW(u >> 16);
    c[8] = SH2OI_LOAD_ROW(w[4]);
}
SH2OI_FORCE_INLINE uint8_t sh2oi_load_words(uint32_t *p,const uint32_t *w) {
    uint32_t a = w[0],b = w[1],c = w[2];
    p[0] = SH2OI_LOAD_MACH(a);
    p[1] = a << SH2OI_LOAD_TSHIFT;
    p[2] = SH2OI_LOAD_MACH(b);
    p[3] = b << SH2OI_LOAD_TSHIFT;
    p[4] = SH2OI_LOAD_MACH(c);
    p[5] = c << SH2OI_LOAD_TSHIFT;
    return (uint8_t)!(SH2OI_LOAD_OUT(a) | SH2OI_LOAD_OUT(b) | SH2OI_LOAD_OUT(c));
}

/* Contract tag gte-mvmva-prepared-1. Contract in sh2o/gte_prepared.h. */
void sh2o_gte_prepared_load(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts) {
    SH2OI_COUNT_NAME(SH2O_COUNT_GTE_PREPARED_LOAD);
    if(parts & SH2O_GTE_PREPARED_RT)
        sh2oi_load_rows(p->c[0],g->r+32);
    if(parts & SH2O_GTE_PREPARED_LIGHT)
        sh2oi_load_rows(p->c[1],g->r+40);
    if(parts & SH2O_GTE_PREPARED_COLOR)
        sh2oi_load_rows(p->c[2],g->r+48);
    if(parts & SH2O_GTE_PREPARED_TR)
        p->ok[0] = sh2oi_load_words(p->pre[0],g->r+37);
    if(parts & SH2O_GTE_PREPARED_BK)
        p->ok[1] = sh2oi_load_words(p->pre[1],g->r+45);
    if(parts & SH2O_GTE_PREPARED_NONE) {
        for(unsigned j = 0;j < 6;j++)
            p->pre[2][j] = 0;
        p->ok[2] = 1;
    }
}
#undef SH2OI_LOAD_MACH
#undef SH2OI_LOAD_ROW
#undef SH2OI_LOAD_OUT
#undef SH2OI_LOAD_TSHIFT
#undef SH2OI_LOAD_CSHIFT
#undef sh2oi_load_words
#undef sh2oi_load_rows
#undef SH2OI_LOAD_HELPER
#undef SH2OI_LOAD_HELPER_NAME

/* Contract tag gte-mvmva-prepared-1. Contract in sh2o/gte_prepared.h. */
int sh2o_gte_mvmva_prepared(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd) {
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
    ) {
        SH2OI_COUNT_IN();
        sh2o_gte_mvmva(g,cmd);
        SH2OI_COUNT_OUT();
        return 0;
    }
    SH2OI_COUNT_STORE(SH2O_COUNT_OP + 0x12u,1u);
    if(vx == 3u) {
        v[0] = (int16_t)r[9];
        v[1] = (int16_t)r[10];
        v[2] = (int16_t)r[11];
    } else {
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
    for(i = 0; i < 3; i++) {
        int32_t m = mac[i];
        r[25 + i] = (uint32_t)m;
        if(m < lo) {
            f |= b;
            m = lo;
        } else if(m > 0x7FFF) {
            f |= b;
            m = 0x7FFF;
        }
        r[9 + i] = (uint32_t)m;
        b >>= 1;
    }
    r[63] = f;
    return 1;
}
