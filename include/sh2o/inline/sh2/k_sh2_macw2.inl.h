/* SH-2 MAC.W kernels that check SR.S (the -2 forms of the k_sh2.inl.h kernels).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_K_SH2_MACW2_INL_H
#define SH2O_K_SH2_MACW2_INL_H

#if SH2OI_NATIVE

#include <stdint.h>

/* sh2oi_k_sr_s: SR.S (bit 1) as 0 or 1, read with one STC SR; never writes SR */
SH2OI_FORCE_INLINE unsigned sh2oi_k_sr_s(void) {
    uint32_t sr;
    __asm__ volatile("stc sr, %[sr]" : [sr] "=r"(sr));
    return (unsigned)(sr >> 1) & 1u;
}

/* Contract tag k-dot3b-macw-2. Replaces k-dot3b-macw-1 (sh2o_k_dot3b_w, k_sh2.inl.h).
 * The method is SH2ZAM's "SR.S, read and use a path without MAC.L": guide saturn/BATCH_TRANSFORMS.md:201-206,
 * kernel sh2zi_mac_s_clear at 7f99eb70 (file:line in the validation record). Own code; this kernel does not link
 * SH2ZAM. It reads SR once and never writes it: S set, the C reference (no MAC); S clear, the -1 MAC.W body, whose
 * SR.S = 0 assumption then holds. Gives the bytes of the C reference for either S. */
/* sh2o_k_dot3b_w2: as sh2o_k_dot3b_c; sh2o_k_dot3b_w when SR.S = 0 */
SH2OI_FORCE_INLINE int64_t sh2o_k_dot3b_w2(const int16_t m[3],const int16_t v[3],int32_t tr) {
    if(sh2oi_k_sr_s())
        return sh2o_k_dot3b_c(m,v,tr);
    return sh2o_k_dot3b_w(m,v,tr);
}

/* Contract tag k-mvb3-macw-2. Replaces k-mvb3-macw-1 (sh2o_k_mvb3_w, k_sh2.inl.h).
 * The method is SH2ZAM's "SR.S, read and use a path without MAC.L": guide saturn/BATCH_TRANSFORMS.md:201-206,
 * kernel sh2zi_mac_s_clear at 7f99eb70 (file:line in the validation record). Own code; this kernel does not link
 * SH2ZAM. It reads SR once and never writes it: S set, the C reference (no MAC); S clear, the -1 MAC.W body, whose
 * SR.S = 0 assumption then holds. Gives the bytes of the C reference for either S. */
/* sh2o_k_mvb3_w2: as sh2o_k_mvb3_c; sh2o_k_mvb3_w when SR.S = 0 */
SH2OI_FORCE_INLINE void sh2o_k_mvb3_w2(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) {
    if(sh2oi_k_sr_s()) {
        sh2o_k_mvb3_c(w,v,t,out);
    } else {
        sh2o_k_mvb3_w(w,v,t,out);
    }
}

/* Contract tag k-gpf3-macw-2. Replaces k-gpf3-macw-1 (sh2o_k_gpf3_w, k_sh2.inl.h).
 * The method is SH2ZAM's "SR.S, read and use a path without MAC.L": guide saturn/BATCH_TRANSFORMS.md:201-206,
 * kernel sh2zi_mac_s_clear at 7f99eb70 (file:line in the validation record). Own code; this kernel does not link
 * SH2ZAM. It reads SR once and never writes it: S set, the C reference (no MAC); S clear, the -1 MAC.W body, whose
 * SR.S = 0 assumption then holds. Gives the bytes of the C reference for either S. */
/* sh2o_k_gpf3_w2: as sh2o_k_gpf3_c; sh2o_k_gpf3_w when SR.S = 0 */
SH2OI_FORCE_INLINE void sh2o_k_gpf3_w2(const uint32_t w[4],int64_t out[3]) {
    if(sh2oi_k_sr_s()) {
        sh2o_k_gpf3_c(w,out);
    } else {
        sh2o_k_gpf3_w(w,out);
    }
}

/* Contract tag k-gpl3-macw-2. Replaces k-gpl3-macw-1 (sh2o_k_gpl3_w, k_sh2.inl.h).
 * The method is SH2ZAM's "SR.S, read and use a path without MAC.L": guide saturn/BATCH_TRANSFORMS.md:201-206,
 * kernel sh2zi_mac_s_clear at 7f99eb70 (file:line in the validation record). Own code; this kernel does not link
 * SH2ZAM. It reads SR once and never writes it: S set, the C reference (no MAC); S clear, the -1 MAC.W body, whose
 * SR.S = 0 assumption then holds. Gives the bytes of the C reference for either S. */
/* sh2o_k_gpl3_w2: as sh2o_k_gpl3_c; sh2o_k_gpl3_w when SR.S = 0 */
SH2OI_FORCE_INLINE void sh2o_k_gpl3_w2(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]) {
    if(sh2oi_k_sr_s()) {
        sh2o_k_gpl3_c(w,mac,sf,out);
    } else {
        sh2o_k_gpl3_w(w,mac,sf,out);
    }
}

#endif /* SH2OI_NATIVE */

#endif
