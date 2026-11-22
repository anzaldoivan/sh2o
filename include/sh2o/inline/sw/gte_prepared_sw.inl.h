/* C kernel of the prepared MVMVA.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The arithmetic of the SH-2 kernel (gte_prepared_sh2.inl.h) in C: the host and SH2O_FORCE_PORTABLE use it.
 * Included by sh2o/gte_prepared.h. */
#ifndef SH2O_GTE_PREPARED_SW_INL_H
#define SH2O_GTE_PREPARED_SW_INL_H

#include <stdint.h>

/* Contract tag gte-mvmva-prepared-1.
 * sh2oi_gte_prepared_mac3_c(c, pre, v, mac): per row i, s = (pre[2i] SHL 32 | pre[2i+1]) + c[3i]*v[0] +
 * c[3i+1]*v[1] + c[3i+2]*v[2] in 64 bits (wraps), mac[i] = bits 47-16 of s. With c = R SHL 4 and pre = t SHL 16,
 * this is the MVMVA sf=1 MAC (t SHL 12 + R.v) SAR 12 while every |t| < 7FF40000h. The sum is unsigned: no
 * undefined overflow. */
SH2OI_FORCE_INLINE void sh2oi_gte_prepared_mac3_c(const int32_t c[9],const uint32_t pre[6],const int16_t v[3],
    int32_t mac[3])
{
    unsigned i;
    for(i = 0; i < 3; i++)
    {
        uint64_t s = ((uint64_t)pre[2 * i] << 32) | pre[2 * i + 1];
        s += (uint64_t)((int64_t)c[3 * i] * v[0]);
        s += (uint64_t)((int64_t)c[3 * i + 1] * v[1]);
        s += (uint64_t)((int64_t)c[3 * i + 2] * v[2]);
        mac[i] = (int32_t)(uint32_t)(s >> 16);
    }
}

#endif /* SH2O_GTE_PREPARED_SW_INL_H */
