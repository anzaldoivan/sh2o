/* A consumer program that calls the MVMVA list of sh2o/list.h.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <stdio.h>
#include "sh2o/list.h"

int main(void) {
    static sh2o_gte_t g;
    static const uint32_t v[2][2] = {{0x00020001u,0x0003u},{0xFFFF0004u,0x0005u}};
    uint32_t out[2][6];
    unsigned k;
    sh2o_gte_write(&g,32,0x1000u);      /* RT11 = 1.0 */
    sh2o_gte_write(&g,34,0x1000u);      /* RT22 = 1.0 */
    sh2o_gte_write(&g,36,0x1000u);      /* RT33 = 1.0 */
    sh2o_gte_mvmva_list(&g,0x0480012u,v,2u,out); /* MVMVA sf 1, RT, V0, TR */
    for(k = 0; k < 2u; k++)
        printf("item %u: ir %ld %ld %ld mac %ld %ld %ld\n",k,(long)(int32_t)out[k][0],(long)(int32_t)out[k][1],
            (long)(int32_t)out[k][2],(long)(int32_t)out[k][3],(long)(int32_t)out[k][4],(long)(int32_t)out[k][5]);
    return 0;
}
