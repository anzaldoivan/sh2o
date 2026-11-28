/* SH-2 MAC.L kernel of the prepared MVMVA.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_GTE_PREPARED_SH2_INL_H
#define SH2O_GTE_PREPARED_SH2_INL_H

#if SH2OI_NATIVE

#include <stdint.h>

/* Contract tag gte-mvmva-prepared-1.
 * SH2ZAM guide: saturn/BATCH_TRANSFORMS.md:193-206 and :29-58 at 7f99eb70 (MAC.L rows closed by XTRCT, the SR.S
 * read, the plan prepared at the load). SH2ZAM is not linked: its transform is Q16.16 over a point array and has
 * no GTE tail; this kernel must give the GTE bytes.
 * sh2oi_gte_prepared_mac3_l: as sh2oi_gte_prepared_mac3_c, through MAC.L. Per row: MACH:MACL = pre (t SHL 16),
 * three MAC.L over c (R SHL 4) and the widened v, XTRCT gives bits 47-16 = t + floor(16 R.v / 65536) =
 * (t * 4096 + R.v) SAR 12. Hardware assumption: SR.S = 0 (with S = 1, MAC.L saturates the sum to 48 bits). The
 * kernel does not read or set SR; the caller reads SR.S and does not call it when S is set. */
SH2OI_FORCE_INLINE void sh2oi_gte_prepared_mac3_l(const int32_t c[9],const uint32_t pre[6],const int16_t v[3],
    int32_t mac[3]) {
    int32_t p[3];
    const int32_t *pm = c;
    const int32_t *pv = p;
    uint32_t h0 = pre[0],l0 = pre[1],h1 = pre[2],l1 = pre[3],h2 = pre[4],l2 = pre[5];
    p[0] = v[0];
    p[1] = v[1];
    p[2] = v[2];
    SH2O_K_ENTER();
    __asm__ volatile("lds %[h0], mach\n\t"
                    "lds %[l0], macl\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "add #-12, %[v]\n\t"
                    "sts mach, %[h0]\n\t"
                    "sts macl, %[l0]\n\t"
                    "lds %[h1], mach\n\t"
                    "lds %[l1], macl\n\t"
                    "xtrct %[h0], %[l0]\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "add #-12, %[v]\n\t"
                    "sts mach, %[h1]\n\t"
                    "sts macl, %[l1]\n\t"
                    "lds %[h2], mach\n\t"
                    "lds %[l2], macl\n\t"
                    "xtrct %[h1], %[l1]\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "mac.l @%[m]+, @%[v]+\n\t"
                    "sts mach, %[h2]\n\t"
                    "sts macl, %[l2]\n\t"
                    "xtrct %[h2], %[l2]"
                    : [m] "+r"(pm), [v] "+r"(pv), [h0] "+r"(h0), [l0] "+r"(l0), [h1] "+r"(h1), [l1] "+r"(l1), [h2] "+r"(h2), [l2] "+r"(l2)
                    : [c_mem] "m"(*(const int32_t (*)[9])c), [p_mem] "m"(*(const int32_t (*)[3])p)
                    : "mach", "macl");
    SH2O_K_LEAVE();
    mac[0] = (int32_t)l0;
    mac[1] = (int32_t)l1;
    mac[2] = (int32_t)l2;
}

#endif /* SH2OI_NATIVE */
#endif
