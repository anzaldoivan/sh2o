/* SH-2 MAC.W kernels of the fixed-point kernels.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_K_SH2_INL_H
#define SH2O_K_SH2_INL_H

#if SH2OI_NATIVE

#include <stdint.h>

/* Contract tag k-dot3b-macw-1.
 * The method is the SH2ZAM "MAC.L row": docs/SH2ZAM_MAP.md, row `sh2o_k_dot3b_w`. The guide is SH2ZAM
 * saturn/BATCH_TRANSFORMS.md:193-199 and saturn/benchmarks/FUSED_SH2.md:61-65 at 7f99eb70. This kernel does not
 * link SH2ZAM. SH2ZAM has no MAC.W kernel. Its MAC.L rows are Q16.16 and they saturate. This kernel must give the
 * bytes of the C reference. MAC.W: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5.00, section 6.1.32 (verified,
 * page 193). Hardware assumption: SR.S = 0. When S = 1, MAC.W saturates the sum to 32 bits in MACL. The kernel does
 * not read or set SR. This is a known gap. A kernel that checks SR.S gets a new contract tag.
 * Successor: k-dot3b-macw-2 (sh2o_k_dot3b_w2, k_sh2_macw2.inl.h). */
/* sh2o_k_dot3b_w: as sh2o_k_dot3b_c, through MAC.W */
SH2OI_FORCE_INLINE int64_t sh2o_k_dot3b_w(const int16_t m[3],const int16_t v[3],int32_t tr)
{
    const int16_t *pm = m;
    const int16_t *pv = v;
    uint32_t hi = ((uint32_t)tr >> 20) | ((0u - ((uint32_t)tr >> 31)) << 12);
    uint32_t lo = (uint32_t)tr << 12;
    SH2O_K_ENTER();
    __asm__ volatile("lds %2, mach\n\t"
                    "lds %3, macl\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "sts mach, %2\n\t"
                    "sts macl, %3"
                    : "+r"(pm), "+r"(pv), "+r"(hi), "+r"(lo)
                    : "m"(*(const int16_t (*)[3])m), "m"(*(const int16_t (*)[3])v)
                    : "mach", "macl");
    SH2O_K_LEAVE();
    return (int64_t)(((uint64_t)hi << 32) | lo);
}

/* Contract tag k-mvb3-macw-1.
 * The method is the SH2ZAM "MAC.L row": docs/SH2ZAM_MAP.md, row `sh2o_k_mvb3_w`. The guide is SH2ZAM
 * saturn/BATCH_TRANSFORMS.md:193-199 and saturn/benchmarks/FUSED_SH2.md:61-65 at 7f99eb70. This kernel does not
 * link SH2ZAM. SH2ZAM has no MAC.W kernel. Its MAC.L rows are Q16.16 and they saturate. This kernel must give the
 * bytes of the C reference. MAC.W: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5.00, section 6.1.32 (verified,
 * page 193). Hardware assumption: SR.S = 0. When S = 1, MAC.W saturates the sum to 32 bits in MACL. The kernel does
 * not read or set SR. This is a known gap. A kernel that checks SR.S gets a new contract tag.
 * Successor: k-mvb3-macw-2 (sh2o_k_mvb3_w2, k_sh2_macw2.inl.h). */
/* sh2oi_k_mvb3_wq: sh2o_k_mvb3_w's body on q, v in the order the matrix's halfwords are walked. Big-endian memory
 * holds a word's high half first, so w[0..4] read as halfwords p0..p9 are m01 m00 m10 m02 m12 m11 m21 m20 (pad) m22;
 * one pointer walks row 0 as p0 p1 p3, row 1 as p2 p4 p5, row 2 as p6 p7 p9 (add #2 skips a half, add #-4 steps back
 * to p2), against q = {v1, v0, v2, v1}: rows 0 and 2 take q[0..2], row 1 q[1..3]. Three MAC.W per row, the MAC
 * preloaded with (int64)t[i] << 12 (MACH: t SAR 20, MACL: t << 12) and read after the row. */
SH2OI_FORCE_INLINE void sh2oi_k_mvb3_wq(const uint32_t w[5],const int16_t q[4],const int32_t t[3],int64_t out[3])
{
    const int16_t *pm = (const int16_t*)(const void*)w;
    const int16_t *pv = q;
    uint32_t h0 = ((uint32_t)t[0] >> 20) | ((0u - ((uint32_t)t[0] >> 31)) << 12),l0 = (uint32_t)t[0] << 12;
    uint32_t h1 = ((uint32_t)t[1] >> 20) | ((0u - ((uint32_t)t[1] >> 31)) << 12),l1 = (uint32_t)t[1] << 12;
    uint32_t h2 = ((uint32_t)t[2] >> 20) | ((0u - ((uint32_t)t[2] >> 31)) << 12),l2 = (uint32_t)t[2] << 12;
    SH2O_K_ENTER();
    __asm__ volatile("lds %2, mach\n\t"
                    "lds %3, macl\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p0 m01 * v1 */
                    "mac.w @%0+, @%1+\n\t" /* p1 m00 * v0 */
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p3 m02 * v2 */
                    "sts mach, %2\n\t"
                    "sts macl, %3\n\t"
                    "lds %4, mach\n\t"
                    "lds %5, macl\n\t"
                    "add #-4, %0\n\t"
                    "add #-4, %1\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p2 m10 * v0 */
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p4 m12 * v2 */
                    "mac.w @%0+, @%1+\n\t" /* p5 m11 * v1 */
                    "sts mach, %4\n\t"
                    "sts macl, %5\n\t"
                    "lds %6, mach\n\t"
                    "lds %7, macl\n\t"
                    "add #-8, %1\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p6 m21 * v1 */
                    "mac.w @%0+, @%1+\n\t" /* p7 m20 * v0 */
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t" /* p9 m22 * v2 */
                    "sts mach, %6\n\t"
                    "sts macl, %7"
                    : "+r"(pm), "+r"(pv), "+r"(h0), "+r"(l0), "+r"(h1), "+r"(l1), "+r"(h2), "+r"(l2)
                    : "m"(*(const uint32_t (*)[5])w), "m"(*(const int16_t (*)[4])q)
                    : "mach", "macl");
    SH2O_K_LEAVE();
    out[0] = (int64_t)(((uint64_t)h0 << 32) | l0);
    out[1] = (int64_t)(((uint64_t)h1 << 32) | l1);
    out[2] = (int64_t)(((uint64_t)h2 << 32) | l2);
}

/* sh2o_k_mvb3_w: as sh2o_k_mvb3_c, through MAC.W (sh2oi_k_mvb3_wq's walk) */
SH2OI_FORCE_INLINE void sh2o_k_mvb3_w(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3])
{
    int16_t q[4];
    q[0] = v[1];
    q[1] = v[0];
    q[2] = v[2];
    q[3] = v[1];
    sh2oi_k_mvb3_wq(w,q,t,out);
}

/* Contract tag k-gpf3-macw-1.
 * The method is the SH2ZAM "MAC.L row": docs/SH2ZAM_MAP.md, row `sh2o_k_gpf3_w`. The guide is SH2ZAM
 * saturn/BATCH_TRANSFORMS.md:193-199 and saturn/benchmarks/FUSED_SH2.md:61-65 at 7f99eb70. This kernel does not
 * link SH2ZAM. SH2ZAM has no MAC.W kernel. Its MAC.L rows are Q16.16 and they saturate. This kernel must give the
 * bytes of the C reference. MAC.W: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5.00, section 6.1.32 (verified,
 * page 193). Hardware assumption: SR.S = 0. When S = 1, MAC.W saturates the sum to 32 bits in MACL. The kernel does
 * not read or set SR. This is a known gap. A kernel that checks SR.S gets a new contract tag.
 * Successor: k-gpf3-macw-2 (sh2o_k_gpf3_w2, k_sh2_macw2.inl.h). */
/* sh2oi_k_gpf3_wp: sh2o_k_gpf3_w's body on halfword pointers into w (the IR0..IR3 words), pi at IR1's value half and
 * p0 at IR0's. Big-endian memory holds a word's low half second: IRn's value is halfword 2n + 1 of the IR0..IR3
 * words. Per lane CLRMAC, one MAC.W (pi steps 2 to the next word's low half, p0 back to IR0), the product read from
 * both MAC words. */
SH2OI_FORCE_INLINE void sh2oi_k_gpf3_wp(const uint32_t w[4],const int16_t *pi,const int16_t *p0,int64_t out[3])
{
    uint32_t h0,l0,h1,l1,h2,l2;
    SH2O_K_ENTER();
    __asm__ volatile("clrmac\n\t"
                    "mac.w @%0+, @%1+\n\t" /* IR1 * IR0 */
                    "add #2, %0\n\t"
                    "add #-2, %1\n\t"
                    "sts mach, %2\n\t"
                    "sts macl, %3\n\t"
                    "clrmac\n\t"
                    "mac.w @%0+, @%1+\n\t" /* IR2 * IR0 */
                    "add #2, %0\n\t"
                    "add #-2, %1\n\t"
                    "sts mach, %4\n\t"
                    "sts macl, %5\n\t"
                    "clrmac\n\t"
                    "mac.w @%0+, @%1+\n\t" /* IR3 * IR0 */
                    "sts mach, %6\n\t"
                    "sts macl, %7"
                    : "+r"(pi), "+r"(p0), "=&r"(h0), "=&r"(l0), "=&r"(h1), "=&r"(l1), "=&r"(h2), "=&r"(l2)
                    : "m"(*(const uint32_t (*)[4])w)
                    : "mach", "macl");
    SH2O_K_LEAVE();
    out[0] = (int64_t)(((uint64_t)h0 << 32) | l0);
    out[1] = (int64_t)(((uint64_t)h1 << 32) | l1);
    out[2] = (int64_t)(((uint64_t)h2 << 32) | l2);
}

/* sh2o_k_gpf3_w: as sh2o_k_gpf3_c, through MAC.W (sh2oi_k_gpf3_wp's walk) */
SH2OI_FORCE_INLINE void sh2o_k_gpf3_w(const uint32_t w[4],int64_t out[3])
{
    const int16_t *h = (const int16_t*)(const void*)w;
    sh2oi_k_gpf3_wp(w,h + 3,h + 1,out);
}

/* Contract tag k-gpl3-macw-1.
 * The method is the SH2ZAM "MAC.L row": docs/SH2ZAM_MAP.md, row `sh2o_k_gpl3_w`. The guide is SH2ZAM
 * saturn/BATCH_TRANSFORMS.md:193-199 and saturn/benchmarks/FUSED_SH2.md:61-65 at 7f99eb70. This kernel does not
 * link SH2ZAM. SH2ZAM has no MAC.W kernel. Its MAC.L rows are Q16.16 and they saturate. This kernel must give the
 * bytes of the C reference. MAC.W: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5.00, section 6.1.32 (verified,
 * page 193). Hardware assumption: SR.S = 0. When S = 1, MAC.W saturates the sum to 32 bits in MACL. The kernel does
 * not read or set SR. This is a known gap. A kernel that checks SR.S gets a new contract tag.
 * Successor: k-gpl3-macw-2 (sh2o_k_gpl3_w2, k_sh2_macw2.inl.h). */
/* sh2oi_k_gpl3_wp: sh2o_k_gpl3_w's body on sh2oi_k_gpf3_wp's pointers, each lane's MAC preloaded with hi:lo[i] */
SH2OI_FORCE_INLINE void sh2oi_k_gpl3_wp(const uint32_t w[4],const int16_t *pi,const int16_t *p0,const uint32_t hi[3],
                                    const uint32_t lo[3],int64_t out[3])
{
    uint32_t h0 = hi[0],l0 = lo[0],h1 = hi[1],l1 = lo[1],h2 = hi[2],l2 = lo[2];
    SH2O_K_ENTER();
    __asm__ volatile("lds %2, mach\n\t"
                    "lds %3, macl\n\t"
                    "mac.w @%0+, @%1+\n\t" /* + IR1 * IR0 */
                    "add #2, %0\n\t"
                    "add #-2, %1\n\t"
                    "sts mach, %2\n\t"
                    "sts macl, %3\n\t"
                    "lds %4, mach\n\t"
                    "lds %5, macl\n\t"
                    "mac.w @%0+, @%1+\n\t" /* + IR2 * IR0 */
                    "add #2, %0\n\t"
                    "add #-2, %1\n\t"
                    "sts mach, %4\n\t"
                    "sts macl, %5\n\t"
                    "lds %6, mach\n\t"
                    "lds %7, macl\n\t"
                    "mac.w @%0+, @%1+\n\t" /* + IR3 * IR0 */
                    "sts mach, %6\n\t"
                    "sts macl, %7"
                    : "+r"(pi), "+r"(p0), "+r"(h0), "+r"(l0), "+r"(h1), "+r"(l1), "+r"(h2), "+r"(l2)
                    : "m"(*(const uint32_t (*)[4])w)
                    : "mach", "macl");
    SH2O_K_LEAVE();
    out[0] = (int64_t)(((uint64_t)h0 << 32) | l0);
    out[1] = (int64_t)(((uint64_t)h1 << 32) | l1);
    out[2] = (int64_t)(((uint64_t)h2 << 32) | l2);
}

/* sh2o_k_gpl3_w: as sh2o_k_gpl3_c, through MAC.W (the addend as (int64)mac << 12 or mac, split into MACH:MACL) */
SH2OI_FORCE_INLINE void sh2o_k_gpl3_w(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3])
{
    const int16_t *h = (const int16_t*)(const void*)w;
    uint32_t hi[3],lo[3];
    unsigned i;
    for(i = 0; i < 3; i++)
    {
        uint32_t sx = 0u - (mac[i] >> 31);
        lo[i] = sf ? mac[i] << 12 : mac[i];
        hi[i] = sf ? (mac[i] >> 20) | (sx << 12) : sx;
    }
    sh2oi_k_gpl3_wp(w,h + 3,h + 1,hi,lo,out);
}

#endif /* SH2OI_NATIVE */

#endif /* SH2O_K_SH2_INL_H */
