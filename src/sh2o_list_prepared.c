/* MVMVA lists with a prepared shadow of the matrices and translations.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Contract tag gte-mvmva-list-prepared-1. Contract in sh2o/gte_prepared.h.
 * New code, beside sh2o_gte_mvmva_list (src/sh2o_list.c). The gate is decided once per list: sf, v, mx, cv, one
 * SR.S read (SH-2), then one load of the two parts of a stack shadow that cmd uses and its ok[ts]. On the gate each
 * item runs the prepared kernel (sh2oi_gte_prepared_mac3_l on the SH-2, sh2oi_gte_prepared_mac3_c elsewhere) and
 * the sf=1 tail into its out row; the image is written once at the end, as the fold leaves it (the last item's V0,
 * IR1-3, MAC1-3 and FLAG). Outside the gate it calls sh2o_gte_mvmva_list. These two are its only calls
 * (tests/sh2/listprep_static.txt). The file has no static function: tests/test_list_prepared.c includes it again
 * with each planted bug. */
#include <stdint.h>

#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o/list.h"
#include "sh2o_count_internal.h"

/* Test-only planted bugs (tests/test_list_prepared.c includes this file with each): 1 the image registers are left
 * from item 0, not item n-1; 2 the tail's FLAG bit order is reversed (IR1 at bit 22, IR3 at bit 24). 0 (default):
 * none. */
#ifndef SH2OI_LIST_PREPARED_MUTANT
#define SH2OI_LIST_PREPARED_MUTANT 0
#endif

/* Contract tag gte-mvmva-list-prepared-1. Contract in sh2o/gte_prepared.h. */
void sh2o_gte_mvmva_list_prepared(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6])
{
    unsigned mx = cmd >> 17 & 3u,cv = cmd >> 13 & 3u,ts = cv == 3u ? 2u : cv,k,i,e;
    uint32_t *r = g->r;
    uint32_t f = 0;
    int32_t lo = (cmd >> 10 & 1u) ? 0 : -0x8000; /* lower IR limit (lm) */
    int fast = 0;
    sh2o_gte_prepared_t p;
    SH2OI_COUNT_NAME(SH2O_COUNT_LIST_PREPARED_CALLS);
    if(!n)
        return;
    if((cmd >> 19 & 1u) && !(cmd >> 15 & 3u) && mx != 3u && cv != 2u
#if SH2OI_NATIVE
        && !sh2oi_k_sr_s()
#endif
    )
    {
        /* the part bits of mx (RT, LIGHT, COLOR) and ts (TR, BK, NONE): x + 1 + (x SHR 1) is 1 SHL x for x 0..2,
         * constant shifts only (the SH-2 has no variable shift) */
        SH2OI_COUNT_IN();
        sh2o_gte_prepared_load(&p,g,(mx + 1u + (mx >> 1)) | (ts + 1u + (ts >> 1)) << 3);
        SH2OI_COUNT_OUT();
        fast = p.ok[ts];
    }
    if(!fast)
    {
        SH2OI_COUNT_IN();
        sh2o_gte_mvmva_list(g,cmd,v,n,out);
        SH2OI_COUNT_OUT();
        return;
    }
    SH2OI_COUNT_STORE(SH2O_COUNT_LIST_ITEMS,n);
    SH2OI_COUNT_STORE(SH2O_COUNT_OP + 0x12u,n);
    for(k = 0; k < n; k++)
    {
        int16_t x[3];
        int32_t mac[3];
#if SH2OI_LIST_PREPARED_MUTANT == 2
        uint32_t b = 1u << 22;
#else
        uint32_t b = 1u << 24; /* FLAG bit 24-i */
#endif
        x[0] = (int16_t)v[k][0];
        x[1] = (int16_t)(v[k][0] >> 16);
        x[2] = (int16_t)v[k][1];
#if SH2OI_NATIVE
        sh2oi_gte_prepared_mac3_l(p.c[mx],p.pre[ts],x,mac);
#else
        sh2oi_gte_prepared_mac3_c(p.c[mx],p.pre[ts],x,mac);
#endif
        /* the sf=1 tail: MAC1-3, IR1-3 saturated (lm) as MFC2 reads them, FLAG 24-22 reset per item */
        f = 0;
        for(i = 0; i < 3; i++)
        {
            int32_t m = mac[i];
            out[k][3 + i] = (uint32_t)m;
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
            out[k][i] = (uint32_t)m;
#if SH2OI_LIST_PREPARED_MUTANT == 2
            b <<= 1;
#else
            b >>= 1;
#endif
        }
    }
    /* the image as the fold leaves it: the last item's V0, IR1-3, MAC1-3 and FLAG */
#if SH2OI_LIST_PREPARED_MUTANT == 1
    e = 0;
#else
    e = n - 1u;
#endif
    r[0] = v[e][0];
    r[1] = v[e][1];
    r[9] = out[e][0];
    r[10] = out[e][1];
    r[11] = out[e][2];
    r[25] = out[e][3];
    r[26] = out[e][4];
    r[27] = out[e][5];
    r[63] = f;
}
