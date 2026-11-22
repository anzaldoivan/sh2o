/* MVMVA lists over the GTE image.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Contracts in sh2o/list.h.
 * New code. VXY0, VZ0 are plain stores and MAC1-3 full words in the GTE image (sh2o/gte.h), so they are stored and
 * read in place; IR1-3 are read as MFC2 does (sign-extended halves). Includes only <stdint.h> and sh2o headers. */
#include <stdint.h>

#include "sh2o/gte.h"
#include "sh2o/list.h"
#include "sh2o_count_internal.h"

/* Contract tag gte-mvmva-list-1. Contract in sh2o/list.h. */
void sh2o_gte_mvmva_list(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6])
{
    uint32_t *r = g->r;
    unsigned k;
    SH2OI_COUNT_LIST(n);
    for(k = 0; k < n; k++)
    {
        r[0] = v[k][0];
        r[1] = v[k][1];
        SH2OI_COUNT_IN();
        sh2o_gte_mvmva(g,cmd);
        SH2OI_COUNT_OUT();
        out[k][0] = (uint32_t)(int32_t)(int16_t)(uint16_t)(r[9] & 0xFFFFu);
        out[k][1] = (uint32_t)(int32_t)(int16_t)(uint16_t)(r[10] & 0xFFFFu);
        out[k][2] = (uint32_t)(int32_t)(int16_t)(uint16_t)(r[11] & 0xFFFFu);
        out[k][3] = r[25];
        out[k][4] = r[26];
        out[k][5] = r[27];
    }
}
