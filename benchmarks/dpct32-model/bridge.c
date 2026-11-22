/* Bridge for the SH-2 model run of the dpct32 family (the 32-bit DPCT kernel beside sh2o_gte_dpct).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the library functions sh2o_gte_write, sh2o_gte_read, sh2o_gte_dpct and sh2o_gte_dpct32
 * directly (each takes at most three arguments and gives its result in R0). This file has no wrapper: it only keeps
 * these functions in the ELF and gives the harness an I/O buffer. Buffer layout (bytes): 0..255 one sh2o_gte_t
 * image. */
#include "sh2o/gte.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_dpct32_io[256];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts one sh2o_gte_t image at the start of the io buffer");

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h changes and this file does not. */
typedef struct {
    void (*write)(sh2o_gte_t *g,unsigned reg,uint32_t v);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    void (*dpct)(sh2o_gte_t *g,uint32_t cmd);
    void (*dpct32)(sh2o_gte_t *g,uint32_t cmd);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_gte_write,
    sh2o_gte_read,
    sh2o_gte_dpct,
    sh2o_gte_dpct32,
};
