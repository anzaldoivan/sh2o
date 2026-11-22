/* A consumer program that calls the PSY-Q bodies of sh2o/psyq.h.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <stdio.h>
#include "sh2o/psyq.h"

static void on_break(uint32_t code) {
    printf("break 0x%lX\n",(unsigned long)code);
}

int main(void) {
    static sh2o_gte_t g;
    static const sh2o_psyq_matrix_t m = {{{4096,0,0},{0,4096,0},{0,0,4096}},0,{10,20,30}};
    static const sh2o_psyq_svector_t v = {1,2,3,0};
    sh2o_psyq_vector_t out;
    int32_t flag;
    sh2o_psyq_break = on_break;
    sh2o_psyq_InitGeom(&g);
    sh2o_psyq_SetRotMatrix(&g,&m);
    sh2o_psyq_SetTransMatrix(&g,&m);
    sh2o_psyq_RotTrans(&g,&v,&out,&flag);
    printf("rottrans %ld %ld %ld flag %ld\n",(long)out.vx,(long)out.vy,(long)out.vz,(long)flag);
    printf("ratan2 0x%lX\n",(unsigned long)sh2o_psyq_ratan2(&g,1,1));
    printf("ratan2 0,INT32_MIN %ld\n",(long)sh2o_psyq_ratan2(&g,0,INT32_MIN));
    return 0;
}
