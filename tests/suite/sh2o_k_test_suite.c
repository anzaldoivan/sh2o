/* The k suite: dot3b known answers, and the digests of dot3b, mvb3, gpf3 and gpl3.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* sh2o_k_test_suite() prints nothing. tests/test_k.c calls it on the host; a program on the SH-2 can call it too. */
#include <stdint.h>

#include "sh2o_k_test.h"

_Static_assert(SH2O_VEC_EDGE==109375u,"7 tr x 125 m x 125 v edge vectors");
_Static_assert(SH2O_VEC_RANDOM==4096u,"4,096 random vectors");
_Static_assert(SH2O_VEC_COUNT==113471u,"113,471 vectors");
_Static_assert(SH2O_VEC_EDGE_TR0==15625u,"the first edge block is 125 x 125 vectors");

typedef struct sh2o_ka {
    int16_t m[3];
    int16_t v[3];
    int32_t tr;
    int64_t want;
} sh2o_ka;

/* known answers, worked by hand */
static const sh2o_ka sh2o_ka_dot3b[]={
    {{0,0,0},{0,0,0},1,4096},
    {{1,2,3},{4,5,6},-1,-4064},
    {{-1,0,0},{1,0,0},0,-1},
    {{-1,0,0},{1,0,0},1,4095}, /* carry out of the low word */
    {{-0x7FFF-1,-0x7FFF-1,-0x7FFF-1},{-0x7FFF-1,-0x7FFF-1,-0x7FFF-1},0x7FFFFFFF,INT64_C(8799314243584)},
    {{0x7FFF,0x7FFF,0x7FFF},{-0x7FFF-1,-0x7FFF-1,-0x7FFF-1},-0x7FFFFFFF-1,-INT64_C(8799314149376)},
};

/* the dot3b digest over vectors first..first+n-1; s = {generator x, hash} */
void sh2o_k_test_dot3b_part(uint32_t first,uint32_t n,uint32_t s[2]) {
    uint32_t x=s[0],h=s[1],i;
    sh2o_vec t;
    for(i=first;i<first+n;i++) {
        sh2o_vec_get(i,&x,&t);
        h=sh2o_fnv64(h,sh2o_k_dot3b(t.m,t.v,t.tr));
    }
    s[0]=x;
    s[1]=h;
}

/* the digest of sh2o_k_dot3b over the vectors */
uint32_t sh2o_k_test_digest(void) {
    uint32_t s[2]={SH2O_VEC_SEED,0x811C9DC5u};
    sh2o_k_test_dot3b_part(0,SH2O_VEC_COUNT,s);
    return s[1];
}

#if SH2OI_NATIVE
/* Test mutants of sh2o_k_mvb3_w. Each must change the digest on the SH-2. */
/* A dropped MAC step: the last product of row 1 (m11 * v1) is skipped, the pointers stay in step. */
static void sh2o_k_mvb3_w_mut_drop(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) {
    const int16_t *pm=(const int16_t *)(const void *)w;
    int16_t q[4];
    const int16_t *pv=q;
    uint32_t h0=((uint32_t)t[0]>>20)|((0u-((uint32_t)t[0]>>31))<<12),l0=(uint32_t)t[0]<<12;
    uint32_t h1=((uint32_t)t[1]>>20)|((0u-((uint32_t)t[1]>>31))<<12),l1=(uint32_t)t[1]<<12;
    uint32_t h2=((uint32_t)t[2]>>20)|((0u-((uint32_t)t[2]>>31))<<12),l2=(uint32_t)t[2]<<12;
    q[0]=v[1];
    q[1]=v[0];
    q[2]=v[2];
    q[3]=v[1];
    __asm__ volatile("lds %2, mach\n\t"
                    "lds %3, macl\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "sts mach, %2\n\t"
                    "sts macl, %3\n\t"
                    "lds %4, mach\n\t"
                    "lds %5, macl\n\t"
                    "add #-4, %0\n\t"
                    "add #-4, %1\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "add #2, %0\n\t" /* dropped: mac.w @%0+, @%1+ */
                    "add #2, %1\n\t"
                    "sts mach, %4\n\t"
                    "sts macl, %5\n\t"
                    "lds %6, mach\n\t"
                    "lds %7, macl\n\t"
                    "add #-8, %1\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "add #2, %0\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "sts mach, %6\n\t"
                    "sts macl, %7"
                    : "+r"(pm), "+r"(pv), "+r"(h0), "+r"(l0), "+r"(h1), "+r"(l1), "+r"(h2), "+r"(l2)
                    : "m"(*(const uint32_t (*)[5])w), "m"(*(const int16_t (*)[4])q)
                    : "mach", "macl");
    out[0]=(int64_t)(((uint64_t)h0<<32)|l0);
    out[1]=(int64_t)(((uint64_t)h1<<32)|l1);
    out[2]=(int64_t)(((uint64_t)h2<<32)|l2);
}

/* The wrong half order: v is read as if the low half of a word came first in memory (the little-endian order). */
static void sh2o_k_mvb3_w_mut_half(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) {
    int16_t q[4];
    q[0]=v[0];
    q[1]=v[1];
    q[2]=v[2];
    q[3]=v[0];
    sh2oi_k_mvb3_wq(w,q,t,out);
}
#endif

/* The digest of sh2o_k_mvb3 over the vectors, the inputs from sh2o_k_test_mvb3_in; FNV-1a 32 of the three sums.
 * which: 0 the dispatch line, 1 sh2o_k_mvb3_c; on the SH-2 also 2 sh2o_k_mvb3_w, 3 and 4 its mutants (a dropped
 * step, the wrong half order); 0 for a which that the build does not have. */
uint32_t sh2o_k_test_mvb3_digest(unsigned which) {
    uint32_t s[2]={SH2O_VEC_SEED,0x811C9DC5u};
    sh2o_k_test_mvb3_part(which,0,SH2O_VEC_COUNT,s);
    return s[1];
}

/* the mvb3 digest over vectors first..first+n-1; s = {generator x, hash}; the hash is 0 for a which that the
 * build does not have */
void sh2o_k_test_mvb3_part(unsigned which,uint32_t first,uint32_t n,uint32_t s[2]) {
    uint32_t x=s[0],h=s[1],i,w[5];
    int32_t t[3];
    int64_t o[3];
    sh2o_vec e;
    if(which>(SH2OI_NATIVE?4u:1u)) {
        s[1]=0;
        return;
    }
    for(i=first;i<first+n;i++) {
        sh2o_vec_get(i,&x,&e);
        sh2o_k_test_mvb3_in(&e,w,t);
        switch(which) {
        case 0:
            sh2o_k_mvb3(w,e.v,t,o);
            break;
        case 1:
            sh2o_k_mvb3_c(w,e.v,t,o);
            break;
#if SH2OI_NATIVE
        case 2:
            sh2o_k_mvb3_w(w,e.v,t,o);
            break;
        case 3:
            sh2o_k_mvb3_w_mut_drop(w,e.v,t,o);
            break;
        default:
            sh2o_k_mvb3_w_mut_half(w,e.v,t,o);
            break;
#endif
        }
        h=sh2o_fnv64(h,o[0]);
        h=sh2o_fnv64(h,o[1]);
        h=sh2o_fnv64(h,o[2]);
    }
    s[0]=x;
    s[1]=h;
}

#if SH2OI_NATIVE
/* Test mutants of sh2o_k_gpf3_w and sh2o_k_gpl3_w. Each must change its digest on the SH-2. */
/* A dropped MAC step: the product of lane 1 is skipped (the pointers stay in step); its MAC words are read as
 * cleared. */
static void sh2o_k_gpf3_w_mut_drop(const uint32_t w[4],int64_t out[3]) {
    const int16_t *pi=(const int16_t *)(const void *)w+3,*p0=(const int16_t *)(const void *)w+1;
    uint32_t h0,l0,h1,l1,h2,l2;
    __asm__ volatile("clrmac\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "add #2, %0\n\t"
                    "add #-2, %1\n\t"
                    "sts mach, %2\n\t"
                    "sts macl, %3\n\t"
                    "clrmac\n\t"
                    "add #4, %0\n\t" /* dropped: mac.w @%0+, @%1+ ; add #2, %0 ; add #-2, %1 */
                    "sts mach, %4\n\t"
                    "sts macl, %5\n\t"
                    "clrmac\n\t"
                    "mac.w @%0+, @%1+\n\t"
                    "sts mach, %6\n\t"
                    "sts macl, %7"
                    : "+r"(pi), "+r"(p0), "=&r"(h0), "=&r"(l0), "=&r"(h1), "=&r"(l1), "=&r"(h2), "=&r"(l2)
                    : "m"(*(const uint32_t (*)[4])w)
                    : "mach", "macl");
    out[0]=(int64_t)(((uint64_t)h0<<32)|l0);
    out[1]=(int64_t)(((uint64_t)h1<<32)|l1);
    out[2]=(int64_t)(((uint64_t)h2<<32)|l2);
}

/* The wrong half order: the IRs are read as if the low half of a word came first in memory. */
static void sh2o_k_gpf3_w_mut_half(const uint32_t w[4],int64_t out[3]) {
    const int16_t *h=(const int16_t *)(const void *)w;
    sh2oi_k_gpf3_wp(w,h+2,h,out);
}

/* the addend of sh2o_k_gpl3_w, split as in sh2o_k_gpl3_c */
static void sh2o_k_gpl3_split(const uint32_t mac[3],unsigned sf,uint32_t hi[3],uint32_t lo[3]) {
    unsigned i;
    for(i=0;i<3;i++) {
        uint32_t sx=0u-(mac[i]>>31);
        lo[i]=sf?mac[i]<<12:mac[i];
        hi[i]=sf?(mac[i]>>20)|(sx<<12):sx;
    }
}

/* The high word of the MAC preload is dropped (MACH 0: the sign of the addend and its bits above 32 are lost). */
static void sh2o_k_gpl3_w_mut_drop(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]) {
    static const uint32_t zero[3]={0,0,0};
    const int16_t *h=(const int16_t *)(const void *)w;
    uint32_t hi[3],lo[3];
    sh2o_k_gpl3_split(mac,sf,hi,lo);
    sh2oi_k_gpl3_wp(w,h+3,h+1,zero,lo,out);
}

static void sh2o_k_gpl3_w_mut_half(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]) {
    const int16_t *h=(const int16_t *)(const void *)w;
    uint32_t hi[3],lo[3];
    sh2o_k_gpl3_split(mac,sf,hi,lo);
    sh2oi_k_gpl3_wp(w,h+2,h,hi,lo,out);
}
#endif

/* The digests of sh2o_k_gpf3 (h[0]) and sh2o_k_gpl3 (h[1]) over the first n vectors (SH2O_VEC_COUNT: all), in one
 * pass for both: the vector draw is the cost on the SH-2. The inputs come from sh2o_k_test_gp3_in, sf = bit 0 of
 * the vector index; FNV-1a 32 of the three sums. which: as in sh2o_k_test_mvb3_digest; h is 0 for a which that
 * the build does not have. */
void sh2o_k_test_gp3_digest(unsigned which,uint32_t n,uint32_t h[2]) {
    uint32_t s[3]={SH2O_VEC_SEED,0x811C9DC5u,0x811C9DC5u};
    sh2o_k_test_gp3_part(which,0,n,s);
    h[0]=s[1];
    h[1]=s[2];
}

/* the gpf3 and gpl3 digests over vectors first..first+n-1; s = {generator x, gpf3 hash, gpl3 hash}; the hashes
 * are 0 for a which that the build does not have */
void sh2o_k_test_gp3_part(unsigned which,uint32_t first,uint32_t n,uint32_t s[3]) {
    uint32_t x=s[0],i,w[4],mac[3],h[2];
    int64_t o[3],p[3];
    sh2o_vec e;
    if(which>(SH2OI_NATIVE?4u:1u)) {
        s[1]=s[2]=0;
        return;
    }
    h[0]=s[1];
    h[1]=s[2];
    for(i=first;i<first+n;i++) {
        unsigned sf=i&1u;
        sh2o_vec_get(i,&x,&e);
        sh2o_k_test_gp3_in(&e,w,mac);
        switch(which) {
        case 0:
            sh2o_k_gpf3(w,o);
            sh2o_k_gpl3(w,mac,sf,p);
            break;
        case 1:
            sh2o_k_gpf3_c(w,o);
            sh2o_k_gpl3_c(w,mac,sf,p);
            break;
#if SH2OI_NATIVE
        case 2:
            sh2o_k_gpf3_w(w,o);
            sh2o_k_gpl3_w(w,mac,sf,p);
            break;
        case 3:
            sh2o_k_gpf3_w_mut_drop(w,o);
            sh2o_k_gpl3_w_mut_drop(w,mac,sf,p);
            break;
        default:
            sh2o_k_gpf3_w_mut_half(w,o);
            sh2o_k_gpl3_w_mut_half(w,mac,sf,p);
            break;
#endif
        }
        h[0]=sh2o_fnv64(sh2o_fnv64(sh2o_fnv64(h[0],o[0]),o[1]),o[2]);
        h[1]=sh2o_fnv64(sh2o_fnv64(sh2o_fnv64(h[1],p[0]),p[1]),p[2]);
    }
    s[0]=x;
    s[1]=h[0];
    s[2]=h[1];
}

/* 0 = pass; else 1 + the index of the failing known answer, or the code of the digest that differs
 * (sh2o_k_test.h) */
int sh2o_k_test_suite(void) {
    uint32_t hg[2];
    unsigned k;
    for(k=0;k<sizeof sh2o_ka_dot3b/sizeof sh2o_ka_dot3b[0];k++) {
        const sh2o_ka *a=&sh2o_ka_dot3b[k];
        if(sh2o_k_dot3b(a->m,a->v,a->tr)!=a->want)
            return (int)k+1;
    }
    if(sh2o_k_test_digest()!=SH2O_DIGEST_DOT3B)
        return 0x100;
    if(sh2o_k_test_mvb3_digest(0)!=SH2O_DIGEST_MVB3||sh2o_k_test_mvb3_digest(1)!=SH2O_DIGEST_MVB3)
        return 0x101;
    /* gp3: the dispatch line; also the C off the SH-2. On the SH-2 a full C pass doubles the run time, and the
     * host already checks the C. */
    sh2o_k_test_gp3_digest(0,SH2O_VEC_COUNT,hg);
    if(hg[0]!=SH2O_DIGEST_GPF3)
        return 0x102;
    if(hg[1]!=SH2O_DIGEST_GPL3)
        return 0x103;
#if !SH2OI_NATIVE
    sh2o_k_test_gp3_digest(1,SH2O_VEC_COUNT,hg);
    if(hg[0]!=SH2O_DIGEST_GPF3)
        return 0x102;
    if(hg[1]!=SH2O_DIGEST_GPL3)
        return 0x103;
#endif
    sh2o_k_test_gp3_digest(0,SH2O_VEC_EDGE_TR0,hg);
    if(hg[0]!=SH2O_DIGEST_GPF3_EDGE)
        return 0x104;
    if(hg[1]!=SH2O_DIGEST_GPL3_EDGE)
        return 0x105;
    return 0;
}
