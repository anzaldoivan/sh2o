/* Internal configuration shared by the library sources.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_INTERNAL_H
#define SH2O_INTERNAL_H
#include <stdint.h>
#include "sh2o/cdefs.h"

/* GTE results are 16-bit and 32-bit two's-complement words. Every exact
 * contract depends on these widths. */
_Static_assert(sizeof(int16_t)==2&&sizeof(int32_t)==4,"16-bit and 32-bit words required");

/* Converts the low 32 bits to a signed value without the implementation-defined
 * conversion of an out-of-range unsigned value. */
static inline int32_t sh2oi_wrap32(uint32_t bits) {
    if(bits<=UINT32_C(0x7FFFFFFF))
        return (int32_t)bits;
    return -(int32_t)(~bits)-1;
}

#endif
