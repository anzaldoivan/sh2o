/* Fixed-point primitives with PS1 GTE rounding and saturation.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/sh2o.h"
#include "sh2o_internal.h"

#if SH2OI_NATIVE
/* DMULS.L writes the signed 64-bit product to MACH:MACL. It does not read the
 * S bit, so no SR check is necessary. MAC.L does read it.
 * Reference: Renesas SH-1/SH-2/SH-DSP Software Manual, rev.5, 6.1.20 (DMULS.L). */
sh2o_q12_t sh2o_mul_q12(sh2o_q12_t a,sh2o_q12_t b) {
    uint32_t hi,lo;
    __asm__ volatile(
        "dmuls.l %[a],%[b]\n\t"
        "sts mach,%[hi]\n\t"
        "sts macl,%[lo]"
        : [hi] "=r" (hi), [lo] "=r" (lo)
        : [a] "r" (a), [b] "r" (b)
        : "mach","macl");
    return sh2oi_wrap32((lo>>12)|(hi<<20));
}
#else
/* A right shift of a negative value is implementation-defined in C. The floor
 * is made from a shift of a non-negative value. The product magnitude is at
 * most 2^62, so the negation cannot overflow. */
sh2o_q12_t sh2o_mul_q12(sh2o_q12_t a,sh2o_q12_t b) {
    int64_t product=(int64_t)a*b;
    int64_t shifted=product>=0?product>>12:-((-product+4095)>>12);
    return sh2oi_wrap32((uint32_t)(uint64_t)shifted);
}
#endif

int32_t sh2o_lim_ir(int32_t value,bool lm) {
    int32_t low=lm?0:SH2O_IR_MIN;
    if(value<low)
        return low;
    if(value>SH2O_IR_MAX)
        return SH2O_IR_MAX;
    return value;
}
