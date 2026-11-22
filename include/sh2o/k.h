/* Fixed-point kernels that the GTE module builds on.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_K_H
#define SH2O_K_H

#include <stdint.h>

#include "sh2o/cdefs.h"
#include "sh2o/inline/sw/k_sw.inl.h"
#include "sh2o/inline/sh2/k_sh2.inl.h"
#include "sh2o/inline/sh2/k_sh2_macw2.inl.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Each kernel: a contract below, a C reference (inline/sw), SH-2 candidates (inline/sh2) and one dispatch line. */

/* Contract tag k-dot3b-1.
 * sh2o_k_dot3b(m, v, tr): ((int64)tr << 12) + m.v, exact as int64 for any int16 m, v, int32 tr (|sum| < 2^44);
 * no MAC flags. Dispatch: the C reference everywhere. */
SH2OI_FORCE_INLINE int64_t sh2o_k_dot3b(const int16_t m[3],const int16_t v[3],int32_t tr)
{
    return sh2o_k_dot3b_c(m,v,tr);
}

/* Contract tag k-mvb3-1.
 * sh2o_k_mvb3(w, v, t, out): out[i] = ((int64)t[i] << 12) + M[i].v, M the row-major int16 3x3 of G's five words;
 * exact as int64, no flags (the caller keeps them). Dispatch: `_w2` on the SH-2, else C. */
SH2OI_FORCE_INLINE void sh2o_k_mvb3(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3])
{
#if SH2OI_NATIVE
    sh2o_k_mvb3_w2(w,v,t,out);
#else
    sh2o_k_mvb3_c(w,v,t,out);
#endif
}

/* Contract tag k-gpf3-1.
 * sh2o_k_gpf3(w, out): out[i] = IR(i+1) * IR0 (GPF before its shift), w G's IR0..IR3 words; exact, no flags.
 * Dispatch: the C everywhere. */
SH2OI_FORCE_INLINE void sh2o_k_gpf3(const uint32_t w[4],int64_t out[3])
{
    sh2o_k_gpf3_c(w,out);
}

/* Contract tag k-gpl3-1.
 * sh2o_k_gpl3(w, mac, sf, out): out[i] = (mac[i] << 12 when sf, else mac[i]) + IR(i+1) * IR0, exact as int64; no
 * flags (G calls it only when the sum cannot pass 43 bits). Dispatch: the C everywhere. */
SH2OI_FORCE_INLINE void sh2o_k_gpl3(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3])
{
    sh2o_k_gpl3_c(w,mac,sf,out);
}

/* Contract tag k-clz32-1.
 * sh2o_k_clz32(v): leading zero count of any uint32 (0 -> 32), a nibble table after three halving steps.
 * Dispatch: the C everywhere. */
SH2OI_FORCE_INLINE unsigned sh2o_k_clz32(uint32_t v)
{
    return sh2o_k_clz32_c(v);
}

/* Contract tag k-unr-recip-1.
 * sh2o_k_unr_recip(d): the nocash UNR reciprocal of d in 8000h..FFFFh (psx-spx GTE division; undefined
 * outside it); equal to the model's for every d. Dispatch: the C everywhere. */
SH2OI_FORCE_INLINE uint32_t sh2o_k_unr_recip(uint32_t d)
{
    return sh2o_k_unr_recip_c(d);
}

/* Contract tag k-unr-div-1.
 * sh2o_k_unr_div(h, sz): the RTPS/RTPT divide, about ((h << 17) / sz + 1) / 2 for h < 2*sz (the caller handles
 * the rest); every (h, sz) checked. Dispatch: the C everywhere. */
SH2OI_FORCE_INLINE uint32_t sh2o_k_unr_div(uint32_t h,uint32_t sz)
{
    return sh2o_k_unr_div_c(h,sz);
}

#ifdef __cplusplus
}
#endif
#endif /* SH2O_K_H */
