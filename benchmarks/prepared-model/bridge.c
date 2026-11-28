/* Bridge for the SH-2 model run of the prepared family (MVMVA with a prepared shadow, beside sh2o_gte_mvmva).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the library functions sh2o_gte_write, sh2o_gte_read, sh2o_gte_mvmva,
 * sh2o_gte_prepared_load and sh2o_gte_mvmva_prepared directly (each takes at most three arguments and gives its
 * result in R0). The two kernels are inline, so each has one out-of-line wrapper, as benchmarks/k-model/bridge.c
 * does: sh2o_model_prepared_mac3 (the prepared kernel: sh2oi_gte_prepared_mac3_l on the SH-2, else
 * sh2oi_gte_prepared_mac3_c, as src/sh2o_gte_prepared.c selects it) and sh2o_model_prepared_mac3_a (the kernel of
 * sh2o_gte_mvmva: sh2o_k_mvb3, as benchmarks/gte-model/bridge.c sh2o_model_gte_mac3 does). The wrapper's call stays
 * in the kernel figure.
 * Two resumable checks run in the ELF, ported from tests/test_prepared.c (same seeds, same draw order); each call
 * takes a part of the work so that no call reaches the instruction limit of the model:
 * sh2o_model_prepared_digest_part(first, n, s): check 2, the digest over vectors first..first+n-1 of
 *   tests/suite/sh2o_k_test.h; s = {generator x, FNV-1a 32 hash, vectors on the kernel path}.
 * sh2o_model_prepared_op_part(n, s): check 3, n random images and selectors; s = {generator x, runs that differ
 *   from sh2o_gte_mvmva, runs on the kernel path}.
 * Buffer layout (bytes): 0..255 one sh2o_gte_t image, 256..447 one sh2o_gte_prepared_t (184 bytes), 448..453 the
 * kernels' v[3] (int16), 456..467 the prepared kernel's mac[3] (int32), 468..479 the kernel t[3] of
 * sh2o_k_mvb3 (int32), 480..503 its out[3] (int64), 512..527 the check state s[4]. */
#include <stddef.h>

#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o_k_test.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_prepared_io[640];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts one sh2o_gte_t image at the start of the io buffer");
_Static_assert(sizeof(sh2o_gte_prepared_t)<=192u,"the harness puts one shadow at byte 256 of the io buffer");
_Static_assert(offsetof(sh2o_gte_prepared_t,c)==0u&&offsetof(sh2o_gte_prepared_t,pre)==108u&&
    offsetof(sh2o_gte_prepared_t,ok)==180u,"the harness reads the shadow at these offsets");

void sh2o_model_prepared_mac3(const int32_t c[9],const uint32_t pre[6],const int16_t v[3],int32_t mac[3]);
void sh2o_model_prepared_mac3(const int32_t c[9],const uint32_t pre[6],const int16_t v[3],int32_t mac[3]) {
#if SH2OI_NATIVE
    sh2oi_gte_prepared_mac3_l(c,pre,v,mac);
#else
    sh2oi_gte_prepared_mac3_c(c,pre,v,mac);
#endif
}

void sh2o_model_prepared_mac3_a(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
void sh2o_model_prepared_mac3_a(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) {
    sh2o_k_mvb3(w,v,t,out);
}

/* The images of the checks: [0] through sh2o_gte_mvmva_prepared, [1] the copy through sh2o_gte_mvmva. */
static sh2o_gte_t sh2o_model_prepared_g[2];
static sh2o_gte_prepared_t sh2o_model_prepared_p;

static uint32_t sh2o_model_prepared_lcg(uint32_t *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

/* FNV-1a 32 over a word's four bytes, high first */
static uint32_t sh2o_model_prepared_fnv32(uint32_t h,uint32_t u) {
    h=(h^(u>>24))*0x01000193u;
    h=(h^(u>>16&0xFFu))*0x01000193u;
    h=(h^(u>>8&0xFFu))*0x01000193u;
    return (h^(u&0xFFu))*0x01000193u;
}

/* Check 2 of tests/test_prepared.c over vectors first..first+n-1. Each image is zeroed through a volatile pointer:
 * GCC keeps the loop (no memset call; the ELF has no C library). */
void sh2o_model_prepared_digest_part(uint32_t first,uint32_t n,uint32_t s[3]);
void sh2o_model_prepared_digest_part(uint32_t first,uint32_t n,uint32_t s[3]) {
    uint32_t x=s[0],h=s[1],gated=s[2],i,w[5];
    int32_t t[3];
    sh2o_vec e;
    sh2o_gte_t *g=&sh2o_model_prepared_g[0];
    for(i=first;i<first+n;i++) {
        volatile uint32_t *z=g->r;
        unsigned j;
        sh2o_vec_get(i,&x,&e);
        sh2o_k_test_mvb3_in(&e,w,t);
        for(j=0;j<64;j++)
            z[j]=0u;
        for(j=0;j<5;j++)
            g->r[32+j]=w[j];
        for(j=0;j<3;j++)
            g->r[37+j]=(uint32_t)t[j];
        g->r[0]=(uint16_t)e.v[0]|(uint32_t)(uint16_t)e.v[1]<<16;
        g->r[1]=(uint16_t)e.v[2];
        sh2o_gte_prepared_load(&sh2o_model_prepared_p,g,SH2O_GTE_PREPARED_ALL);
        gated+=(uint32_t)sh2o_gte_mvmva_prepared(g,&sh2o_model_prepared_p,0x80012u);
        for(j=0;j<3;j++)
            h=sh2o_model_prepared_fnv32(h,g->r[25+j]);
    }
    s[0]=x;
    s[1]=h;
    s[2]=gated;
}

/* Check 3 of tests/test_prepared.c: n runs from the generator s[0]. Each draw is its own statement, in the order of
 * rand_image there (left operand first). The copy goes through a volatile pointer (no memcpy call). */
void sh2o_model_prepared_op_part(uint32_t n,uint32_t s[3]);
void sh2o_model_prepared_op_part(uint32_t n,uint32_t s[3]) {
    uint32_t x=s[0],differ=s[1],fast=s[2],k;
    sh2o_gte_t *g1=&sh2o_model_prepared_g[0],*g0=&sh2o_model_prepared_g[1];
    for(k=0;k<n;k++) {
        volatile uint32_t *copy=g0->r;
        uint32_t cmd=(sh2o_model_prepared_lcg(&x)&0x0007E400u)|0x00080012u,d=0;
        unsigned i;
        if((sh2o_model_prepared_lcg(&x)&7u)==0)
            cmd&=~0x00080000u;
        for(i=0;i<64;i++)
            g1->r[i]=sh2o_model_prepared_lcg(&x);
        if(sh2o_model_prepared_lcg(&x)&3u)
            for(i=0;i<3;i++) {
                uint32_t a,sh;
                a=sh2o_model_prepared_lcg(&x);
                sh=sh2o_model_prepared_lcg(&x)&15u;
                g1->r[37+i]=(uint32_t)((int32_t)a>>sh);
                a=sh2o_model_prepared_lcg(&x);
                sh=sh2o_model_prepared_lcg(&x)&15u;
                g1->r[45+i]=(uint32_t)((int32_t)a>>sh);
            }
        for(i=0;i<64;i++)
            copy[i]=g1->r[i];
        sh2o_gte_mvmva(g0,cmd);
        sh2o_gte_prepared_load(&sh2o_model_prepared_p,g1,SH2O_GTE_PREPARED_ALL);
        fast+=(uint32_t)sh2o_gte_mvmva_prepared(g1,&sh2o_model_prepared_p,cmd);
        for(i=0;i<64;i++)
            d|=g0->r[i]^g1->r[i];
        differ+=(uint32_t)(d!=0u);
    }
    s[0]=x;
    s[1]=differ;
    s[2]=fast;
}

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h or sh2o/gte_prepared.h changes and this
 * file does not. */
typedef struct {
    void (*write)(sh2o_gte_t *g,unsigned reg,uint32_t v);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    void (*mvmva)(sh2o_gte_t *g,uint32_t cmd);
    void (*load)(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
    int (*mvmva_prepared)(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);
    void (*mac3)(const int32_t c[9],const uint32_t pre[6],const int16_t v[3],int32_t mac[3]);
    void (*mac3_a)(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
    void (*digest_part)(uint32_t first,uint32_t n,uint32_t s[3]);
    void (*op_part)(uint32_t n,uint32_t s[3]);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_gte_write,
    sh2o_gte_read,
    sh2o_gte_mvmva,
    sh2o_gte_prepared_load,
    sh2o_gte_mvmva_prepared,
    sh2o_model_prepared_mac3,
    sh2o_model_prepared_mac3_a,
    sh2o_model_prepared_digest_part,
    sh2o_model_prepared_op_part,
};
