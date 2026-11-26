/* Bulk vectors, digest and pinned digests of the k suite.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_K_TEST_H
#define SH2O_K_TEST_H

/* A program on the SH-2 can compile tests/suite/sh2o_k_test_suite.c with this header: they use no C library.
 * tests/test_k.c runs the suite on the host, and tests/k_bulk_check.py compares its dump with a Python model. */
#include <stdint.h>

#include "sh2o/k.h"
#ifdef __cplusplus
extern "C" {
#endif

#define SH2O_VEC_EDGE 109375u /* 7 tr x 125 m x 125 v */
#define SH2O_VEC_RANDOM 4096u
#define SH2O_VEC_COUNT (SH2O_VEC_EDGE + SH2O_VEC_RANDOM) /* 113,471 */
#define SH2O_VEC_SEED 0x53415432u
#define SH2O_DIGEST_DOT3B 0x61683DE4u /* FNV-1a 32 of sh2o_k_dot3b over the vectors */
#define SH2O_DIGEST_MVB3 0x9D49BB58u  /* FNV-1a 32 of sh2o_k_mvb3 over the vectors (sh2o_k_test_mvb3_digest) */
#define SH2O_DIGEST_GPF3 0x61BC75C3u  /* FNV-1a 32 of sh2o_k_gpf3 over the vectors (sh2o_k_test_gp3_digest) */
#define SH2O_DIGEST_GPL3 0xCDB04460u  /* FNV-1a 32 of sh2o_k_gpl3 over the vectors (sh2o_k_test_gp3_digest) */
#define SH2O_VEC_EDGE_TR0 15625u /* the edge block with tr = INT32_MIN: each m, v edge pair */
#define SH2O_DIGEST_GPF3_EDGE 0x7C2BE3B8u /* as SH2O_DIGEST_GPF3, over the first SH2O_VEC_EDGE_TR0 vectors */
#define SH2O_DIGEST_GPL3_EDGE 0xAACA9F5Fu /* as SH2O_DIGEST_GPL3, over the first SH2O_VEC_EDGE_TR0 vectors */

typedef struct sh2o_vec {
    int16_t m[3];
    int16_t v[3];
    int32_t tr;
} sh2o_vec;

/* The vectors: first the edge cross product (tr outermost, then m, then v, element 0 fastest), then the LCG
 * x = x * 1664525 + 1013904223 from the seed: for each vector tr (32 bits), then m[0..2] and v[0..2] (the low
 * 16 bits of each draw). Call it with i = 0, 1, ... in order: the random part advances *x. */
static inline void sh2o_vec_get(uint32_t i,uint32_t *x,sh2o_vec *o) {
    static const int32_t sh2o_tr_edge[7]={-0x7FFFFFFF-1,-0x7FFFFFFF,-1,0,1,0x7FFFFFFE,0x7FFFFFFF};
    static const int16_t sh2o_s_edge[5]={-0x7FFF-1,-1,0,1,0x7FFF};
    int k;
    if(i<SH2O_VEC_EDGE) {
        uint32_t n=(i/125u)%125u;
        uint32_t d=i%125u;
        o->tr=sh2o_tr_edge[i/15625u];
        for(k=0;k<3;k++) {
            o->m[k]=sh2o_s_edge[n%5u];
            o->v[k]=sh2o_s_edge[d%5u];
            n/=5u;
            d/=5u;
        }
        return;
    }
    *x=*x*1664525u+1013904223u;
    o->tr=(int32_t)*x;
    for(k=0;k<3;k++) {
        *x=*x*1664525u+1013904223u;
        o->m[k]=(int16_t)(uint16_t)*x;
    }
    for(k=0;k<3;k++) {
        *x=*x*1664525u+1013904223u;
        o->v[k]=(int16_t)(uint16_t)*x;
    }
}

/* FNV-1a 32 over the 8 bytes of a value, most significant first (the value, not its bytes in memory) */
static inline uint32_t sh2o_fnv64(uint32_t h,int64_t r) {
    uint64_t u=(uint64_t)r;
    int s;
    for(s=56;s>=0;s-=8)
        h=(h^(uint32_t)((u>>s)&0xFFu))*0x01000193u;
    return h;
}

/* The mvb3 inputs of vector e: rows m, (m1 m2 m0), (m2 m0 m1) in five words (the high half of w[4] is A5A5h,
 * not used), t = (tr, ~tr, tr). */
static inline void sh2o_k_test_mvb3_in(const sh2o_vec *e,uint32_t w[5],int32_t t[3]) {
    w[0]=(uint16_t)e->m[0]|(uint32_t)(uint16_t)e->m[1]<<16;
    w[1]=(uint16_t)e->m[2]|(uint32_t)(uint16_t)e->m[1]<<16;
    w[2]=(uint16_t)e->m[2]|(uint32_t)(uint16_t)e->m[0]<<16;
    w[3]=(uint16_t)e->m[2]|(uint32_t)(uint16_t)e->m[0]<<16;
    w[4]=(uint16_t)e->m[1]|0xA5A50000u;
    t[0]=e->tr;
    t[1]=~e->tr;
    t[2]=e->tr;
}

/* The gpf3 and gpl3 inputs of vector e: IR0 = m0 (high half A5A5h), IR1..IR3 = v0, v1, v2 (high halves m1, m2,
 * 5A5Ah); MAC1..MAC3 = (tr, ~tr, tr). The caller sets sf to bit 0 of the vector index. */
static inline void sh2o_k_test_gp3_in(const sh2o_vec *e,uint32_t w[4],uint32_t mac[3]) {
    w[0]=(uint16_t)e->m[0]|0xA5A50000u;
    w[1]=(uint16_t)e->v[0]|(uint32_t)(uint16_t)e->m[1]<<16;
    w[2]=(uint16_t)e->v[1]|(uint32_t)(uint16_t)e->m[2]<<16;
    w[3]=(uint16_t)e->v[2]|0x5A5A0000u;
    mac[0]=(uint32_t)e->tr;
    mac[1]=~(uint32_t)e->tr;
    mac[2]=(uint32_t)e->tr;
}

/* The suite (sh2o_k_test_suite.c) prints nothing. sh2o_k_test_suite(): 0 = pass; else 1 + the index of the
 * failing known answer, 0x100 for a dot3b digest mismatch, 0x101 mvb3, 0x102 gpf3, 0x103 gpl3, 0x104 the gpf3
 * edge digest, 0x105 the gpl3 edge digest.
 * sh2o_k_test_digest(): the digest of sh2o_k_dot3b over the vectors.
 * sh2o_k_test_mvb3_digest(which): the digest of sh2o_k_mvb3 (0 the dispatch line, 1 the C; on the SH-2 also 2
 * `_w`, 3 and 4 its test mutants: a dropped MAC step and the wrong half order); 0 for a which that the build
 * does not have.
 * sh2o_k_test_gp3_digest(which, n, h): the digests of sh2o_k_gpf3 (h[0]) and sh2o_k_gpl3 (h[1]) over the first
 * n vectors, which as for mvb3.
 * The _part functions run a digest over vectors first..first+n-1 and resume: s = {generator x, hash(es)}, seeded
 * {SH2O_VEC_SEED, 0x811C9DC5[, 0x811C9DC5]}; call them with consecutive ranges from 0 (sh2o_vec_get is sequential).
 * The digest above is the seed and one part over its range; a which that the build does not have zeroes the
 * hashes. */
int sh2o_k_test_suite(void);
uint32_t sh2o_k_test_digest(void);
uint32_t sh2o_k_test_mvb3_digest(unsigned which);
void sh2o_k_test_gp3_digest(unsigned which,uint32_t n,uint32_t h[2]);
void sh2o_k_test_dot3b_part(uint32_t first,uint32_t n,uint32_t s[2]);
void sh2o_k_test_mvb3_part(unsigned which,uint32_t first,uint32_t n,uint32_t s[2]);
void sh2o_k_test_gp3_part(unsigned which,uint32_t first,uint32_t n,uint32_t s[3]);

#ifdef __cplusplus
}
#endif
#endif
