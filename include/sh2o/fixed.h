/* Fixed-point primitives with PS1 GTE rounding and saturation.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_FIXED_H
#define SH2O_FIXED_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Signed fixed-point value with 12 fraction bits. One is 4096, as in the GTE. */
typedef int32_t sh2o_q12_t;

/* The value one in sh2o_q12_t. */
#define SH2O_Q12_ONE ((sh2o_q12_t)4096)
/* Smallest value of a GTE IR register. */
#define SH2O_IR_MIN INT32_C(-32768)
/* Largest value of a GTE IR register. */
#define SH2O_IR_MAX INT32_C(32767)

/* Multiplies two Q12 values with the GTE shift.
 *
 *  The function makes the full 64-bit product of a and b and shifts it right
 *  by 12 bits. The shift rounds toward negative infinity. The result is the
 *  low 32 bits of the shifted product. It wraps and does not saturate, as the
 *  GTE MAC registers do. All int32_t inputs are valid. On the SH-2 the
 *  function uses DMULS.L and does not use the MAC S bit. */
sh2o_q12_t sh2o_mul_q12(sh2o_q12_t a,sh2o_q12_t b);

/* Limits a value to a GTE IR register range.
 *
 *  When lm is false, the range is [SH2O_IR_MIN, SH2O_IR_MAX]. When lm is
 *  true, the range is [0, SH2O_IR_MAX]. A value out of range becomes the
 *  nearest limit. The function sets no flag register. */
int32_t sh2o_lim_ir(int32_t value,bool lm);

#ifdef __cplusplus
}
#endif
#endif
