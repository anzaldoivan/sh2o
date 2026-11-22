/* Bridge table for the SH-2 model run of the fixed family.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <sh2o/fixed.h>

/* The model harness reads this table from the ELF and compares each entry
 * with the symbol address that nm gives. The typed pointers make the build
 * fail when a signature in fixed.h changes and this file does not. */
typedef struct {
    sh2o_q12_t (*mul_q12)(sh2o_q12_t a,sh2o_q12_t b);
    int32_t (*lim_ir)(int32_t value,bool lm);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_mul_q12,
    sh2o_lim_ir
};
