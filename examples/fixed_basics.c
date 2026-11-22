/* Example: GTE-style multiply and IR limit on one value.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <sh2o/sh2o.h>
#include <stdio.h>

int main(void) {
    sh2o_q12_t half=SH2O_Q12_ONE/2;
    sh2o_q12_t scaled=sh2o_mul_q12(INT32_C(100000),half);
    printf("100000 * 0.5 = %ld, IR limit %ld\n",(long)scaled,(long)sh2o_lim_ir(scaled,false));
    return 0;
}
