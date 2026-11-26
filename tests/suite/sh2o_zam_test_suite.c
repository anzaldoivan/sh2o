/* The zam suite: the zam-1 delegate against the GTE core's own kernel, all 64 registers.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Built only with SH2O_ZAM=1 (the CMake option SH2O_ZAM): tests/test_zam.c runs it on the host; an empty unit
 * otherwise. Each case runs once delegated and once with sh2o_zam_bypass set (the GTE core's own kernel) from the
 * same image; the 64 words of sh2o_gte_read must match. Case sets: (A) every sh2o_k_test.h vector placed as the rows
 * of RT/light matrix/LCM (row r = m rotated by r), V0-V2 (v rotated) and TR/BK ({tr, ~tr, tr}, {~tr, tr, tr}) in an LCG
 * image, run as MVMVA (selector = vector index bits 0-7: lm, sf, cv, vx, mx), RTPS (sf 1) and RTPT (sf = index bit 8);
 * (B) LCG images from SH2O_GTE_SEED (odd cases tamed with 0x0FFF0FFF): 256 RTPS, 256 RTPT, 256 MVMVA with drawn
 * selectors, then every MVMVA selector 0..255 x 16; (C) the gate's two sides: rows at 2^47-16 (the largest a row can
 * reach: 16*S + 65536|t| is a multiple of 16) and at 2^47, TR extremes, through MVMVA and RTPS, and sh2o_zam_mac3
 * called directly (1 inside, 0 outside with mac untouched). Outside the domain (sf 0, cv 2, mx 3) no call delegates.
 * Digest: FNV-1a 32 over the 64 words of each delegated case, most significant byte first. Each set has its own
 * generator in sh2o_zam_tally, so a run can stop after any item and continue (sh2o_zam_test_digest_part).
 * zam-2..5 trials, outside the digest and the counts above, each against its sh2o_psyq body or sh2o list:
 * ratan2 (measured, never failing) on the grid y, x in -2048..2048, 2^20 xorshift64 pairs (seed 5348325A414D5330h,
 * y then x = the high 32 bits of successive states, shifts 13, 7, 17) and the 13 x 13 edge set; TransposeMatrix on a
 * matrix of 9 distinct entries, the int16 extremes and LCG matrices, each apart and aliased (m1 = m0), m1's pad and t
 * random; SetFogNearFar on LCG images with (a, b) in s16 ranges, full 32-bit draws and an edge set, h in
 * {0, -1, 1, 3FFh, a draw & 3FFh, a 32-bit draw}: delegated cases must give the body's 64 words, both kinds must occur;
 * the MVMVA list (zam-5, at sh2o_zam_list_trial). */
#ifndef SH2O_ZAM
#define SH2O_ZAM 0
#endif

typedef int sh2o_zam_suite_unit; /* keeps the unit non-empty when SH2O_ZAM is 0 (ISO C) */

#if SH2O_ZAM
#include <stdint.h>

#include "sh2o/sh2o.h"
#include "sh2o/zam_experimental.h"
#include "sh2o_zam_test.h"

/* side-effect-free write order, as the gte suite's (no 15, 29, 31; 28 before IR1-3) */
static const uint8_t sh2o_zam_order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,
    30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

static uint32_t sh2o_zam_lcg(uint32_t *x) {
    *x = *x * 1664525u + 1013904223u;
    return *x;
}

static uint32_t sh2o_zam_fnv32(uint32_t h,uint32_t v) {
    h = (h ^ (v >> 24)) * 0x01000193u;
    h = (h ^ ((v >> 16) & 0xFFu)) * 0x01000193u;
    h = (h ^ ((v >> 8) & 0xFFu)) * 0x01000193u;
    return (h ^ (v & 0xFFu)) * 0x01000193u;
}

/* an LCG image written through the accessor; mask tames every word */
static void sh2o_zam_image(sh2o_gte_t *g,uint32_t *x,uint32_t mask) {
    uint32_t w[64];
    unsigned k;
    for(k = 0; k < 64; k++)
        w[k] = sh2o_zam_lcg(x) & mask;
    g->r[15] = g->r[29] = g->r[31] = 0;
    for(k = 0; k < sizeof sh2o_zam_order; k++)
        sh2o_gte_write(g,sh2o_zam_order[k],w[sh2o_zam_order[k]]);
}

static uint32_t sh2o_zam_pack(int32_t lo,int32_t hi) {
    return ((uint32_t)lo & 0xFFFFu) | ((uint32_t)hi & 0xFFFFu) << 16;
}

/* 3x3 matrix into the five words at base (row-major halves, as the GTE core reads them) */
static void sh2o_zam_set_mat(sh2o_gte_t *g,unsigned base,int32_t m[3][3]) {
    g->r[base] = sh2o_zam_pack(m[0][0],m[0][1]);
    g->r[base + 1] = sh2o_zam_pack(m[0][2],m[1][0]);
    g->r[base + 2] = sh2o_zam_pack(m[1][1],m[1][2]);
    g->r[base + 3] = sh2o_zam_pack(m[2][0],m[2][1]);
    g->r[base + 4] = (uint32_t)m[2][2] & 0xFFFFu;
}

static void sh2o_zam_set_vec(sh2o_gte_t *g,unsigned n,const int32_t v[3]) {
    g->r[2 * n] = sh2o_zam_pack(v[0],v[1]);
    g->r[2 * n + 1] = (uint32_t)v[2] & 0xFFFFu;
}

static void sh2o_zam_set_cvec(sh2o_gte_t *g,unsigned base,const int32_t t[3]) {
    g->r[base] = (uint32_t)t[0];
    g->r[base + 1] = (uint32_t)t[1];
    g->r[base + 2] = (uint32_t)t[2];
}

/* MVMVA selector s (lm bit 0, sf 1, cv 2-3, vx 4-5, mx 6-7) -> command word */
static uint32_t sh2o_zam_mvmva(uint32_t s) {
    return 0x12u | (s & 1u) << 10 | ((s >> 1) & 1u) << 19 | ((s >> 2) & 3u) << 13 | ((s >> 4) & 3u) << 15 |
        ((s >> 6) & 3u) << 17;
}

/* 1 when the command is outside the delegate's domain by its fields (sf 0; MVMVA cv 2 or mx 3) */
static int sh2o_zam_outside(uint32_t cmd) {
    if(!(cmd >> 19 & 1u))
        return 1;
    return (cmd & 0x3Fu) == 0x12u && ((cmd >> 13 & 3u) == 2u || (cmd >> 17 & 3u) == 3u);
}

/* one case: delegated vs the GTE core's own kernel on copies of img; want_d 1/0 requires delegated/refused, -1
 * either */
static void sh2o_zam_case(sh2o_zam_tally *t,const sh2o_gte_t *img,uint32_t cmd,int want_d,int code) {
    sh2o_gte_t a = *img,b = *img;
    uint32_t d0 = sh2o_zam_delegated,r;
    int d;
    sh2o_zam_bypass = 0;
    if(sh2o_gte_command(&a,cmd) != 0 && !t->rc)
        t->rc = code | 0xFF;
    d = sh2o_zam_delegated != d0;
    sh2o_zam_bypass = 1;
    (void)sh2o_gte_command(&b,cmd);
    sh2o_zam_bypass = 0;
    t->cases++;
    for(r = 0; r < 64; r++)
        if(sh2o_gte_read(&a,r) != sh2o_gte_read(&b,r) && !t->rc)
            t->rc = code | 1;
    if(d && sh2o_zam_outside(cmd) && !t->rc)
        t->rc = code | 2;
    if(want_d >= 0 && d != want_d && !t->rc)
        t->rc = code | 3;
    if(d) {
        t->delegated++;
        for(r = 0; r < 64; r++)
            t->h = sh2o_zam_fnv32(t->h,sh2o_gte_read(&a,r));
    } else {
        t->refused++;
    }
}

/* (A) the image and commands of shared vector i (i = 0, 1, ... in order: t->xa and t->ya advance) */
void sh2o_zam_test_vector_image(uint32_t i,sh2o_zam_tally *t,sh2o_gte_t *g,uint32_t cmd[3]) {
    sh2o_vec o;
    int32_t m[3][3],v[3][3],tr[3],bk[3];
    unsigned r,j;
    sh2o_vec_get(i,&t->xa,&o);
    sh2o_zam_image(g,&t->ya,(i & 1u) ? 0x0FFF0FFFu : 0xFFFFFFFFu);
    for(r = 0; r < 3; r++)
        for(j = 0; j < 3; j++) {
            m[r][j] = o.m[(j + r) % 3u];
            v[r][j] = o.v[(j + r) % 3u];
        }
    tr[0] = tr[2] = bk[1] = bk[2] = o.tr;
    tr[1] = bk[0] = (int32_t)~(uint32_t)o.tr;
    sh2o_zam_set_mat(g,32,m);
    sh2o_zam_set_mat(g,40,m);
    sh2o_zam_set_mat(g,48,m);
    for(r = 0; r < 3; r++)
        sh2o_zam_set_vec(g,r,v[r]);
    sh2o_zam_set_cvec(g,37,tr);
    sh2o_zam_set_cvec(g,45,bk);
    cmd[0] = sh2o_zam_mvmva(i & 0xFFu);
    cmd[1] = 0x01u | 1u << 19 | ((i >> 7) & 1u) << 10;
    cmd[2] = 0x30u | ((i >> 8) & 1u) << 19 | ((i >> 7) & 1u) << 10;
}

/* (A) shared vector i (i = 0, 1, ... in order: t->xa and t->ya advance) */
static void sh2o_zam_vector(sh2o_zam_tally *t,uint32_t i) {
    sh2o_gte_t g;
    uint32_t cmd[3];
    sh2o_zam_test_vector_image(i,t,&g,cmd);
    sh2o_zam_case(t,&g,cmd[0],-1,0x1000);
    sh2o_zam_case(t,&g,cmd[1],-1,0x1100);
    sh2o_zam_case(t,&g,cmd[2],-1,0x1200);
}

/* (B) seeded LCG image, case i (i = 0, 1, ... in order: t->xb advances) */
static void sh2o_zam_seeded(sh2o_zam_tally *t,uint32_t i) {
    static const uint32_t ops[3]={0x01u,0x30u,0x12u};
    sh2o_gte_t g;
    uint32_t cmd;
    if(i < 3u * 256u) {
        uint32_t d = sh2o_zam_lcg(&t->xb),op = ops[i >> 8];
        cmd = op | ((d >> 16) & 1u) << 10 | ((d >> 17) & 1u) << 19;
        if(op == 0x12u)
            cmd |= ((d >> 18) & 3u) << 17 | ((d >> 20) & 3u) << 15 | ((d >> 22) & 3u) << 13;
    } else {
        cmd = sh2o_zam_mvmva((i - 3u * 256u) >> 4);
    }
    sh2o_zam_image(&g,&t->xb,(i & 1u) ? 0x0FFF0FFFu : 0xFFFFFFFFu);
    sh2o_zam_case(t,&g,cmd,-1,0x2000);
}

/* one gate row: m row 0, V0, TR0 (rows 1-2 and TR1-2 zero); want = expected delegate result */
typedef struct sh2o_zam_edge {
    int32_t m0[3],v[3],tr;
    int want;
} sh2o_zam_edge;

/* (C) the gate's boundary: S = sum |m||v|; row = 16*S + 65536*|tr| against 2^47-1 */
static const sh2o_zam_edge sh2o_zam_edges[]={
    {{63,0,0},{65,0,0},0x7FFFFFFF,1},                               /* S 4095: 2^47-16 */
    {{-63,0,0},{65,0,0},-0x7FFFFFFF,1},                             /* 2^47-16, negative */
    {{64,0,0},{64,0,0},0x7FFFFFFF,0},                               /* S 4096: 2^47 */
    {{-64,0,0},{64,0,0},-0x7FFFFFFF,0},                             /* 2^47, negative */
    {{0,0,0},{0,0,0},-0x7FFFFFFF - 1,0},                            /* TR INT32_MIN alone: 2^47 */
    {{0,0,0},{0,0,0},-0x7FFFFFFF,1},                                /* 2^47-65536 */
    {{0,0,0},{0,0,0},0x7FFFFFFF,1},                                 /* 2^47-65536 */
    {{1,0,0},{-1,0,0},-0x7FFFFFFF - 1,0},                           /* INT32_MIN with any product */
    {{-0x8000,-0x8000,-0x8000},{-0x8000,-0x8000,-0x8000},2146697215,1}, /* S 3*2^30: 2^47-65536 */
    {{-0x8000,-0x8000,-0x8000},{-0x8000,-0x8000,-0x8000},2146697216,0}, /* 2^47 */
    {{-0x8000,0x7FFF,-0x8000},{0x7FFF,-0x8000,-0x8000},0,1},        /* products alone: far inside */
};

_Static_assert(sizeof sh2o_zam_edges / sizeof sh2o_zam_edges[0] + 1u == SH2O_ZAM_GATE_ITEMS,
    "(C): the edge rows and the domain checks");

/* (C) item e: edge row e (e < 11; t->xc advances), else the domain checks by entry */
static void sh2o_zam_gate(sh2o_zam_tally *t,unsigned e) {
    static const int32_t zero[3]={0,0,0};
    unsigned k;
    sh2o_gte_t g;
    if(e < sizeof sh2o_zam_edges / sizeof sh2o_zam_edges[0]) {
        const sh2o_zam_edge *c = &sh2o_zam_edges[e];
        int32_t m[3][3]={{0,0,0},{0,0,0},{0,0,0}},tr[3]={0,0,0},mac[3]={0x11,0x22,0x33};
        for(k = 0; k < 3; k++)
            m[0][k] = c->m0[k];
        tr[0] = c->tr;
        if(sh2o_zam_mac3((const int32_t (*)[3])m,c->v,tr,mac) != c->want && !t->rc)
            t->rc = 0x3000 | (int)e << 4 | 1;
        if(!c->want && (mac[0] != 0x11 || mac[1] != 0x22 || mac[2] != 0x33) && !t->rc)
            t->rc = 0x3000 | (int)e << 4 | 2;
        sh2o_zam_image(&g,&t->xc,0xFFFFFFFFu);
        sh2o_zam_set_mat(&g,32,m);
        sh2o_zam_set_vec(&g,0,c->v);
        sh2o_zam_set_cvec(&g,37,tr);
        for(k = 0; k < 2; k++) {
            sh2o_zam_case(t,&g,sh2o_zam_mvmva(2u | k),c->want,0x3000 | (int)e << 4 | 4); /* mx 0, v 0, TR */
            sh2o_zam_case(t,&g,0x01u | 1u << 19 | k << 10,c->want,0x3000 | (int)e << 4 | 8);
        }
    } else { /* domain by entries: |m| or |v| above 0x8000 refused, mac untouched */
        int32_t m[3][3]={{0x8001,0,0},{0,0,0},{0,0,0}},v[3]={1,0,0},mac[3]={7,7,7};
        if(sh2o_zam_mac3((const int32_t (*)[3])m,v,zero,mac) || mac[0] != 7 || mac[1] != 7 || mac[2] != 7)
            if(!t->rc)
                t->rc = 0x3F01;
        m[0][0] = 1;
        v[1] = -0x8001;
        if(sh2o_zam_mac3((const int32_t (*)[3])m,v,zero,mac) || mac[0] != 7)
            if(!t->rc)
                t->rc = 0x3F02;
    }
}

void sh2o_zam_test_seed(sh2o_zam_tally *t) {
    t->cases = t->delegated = t->refused = 0;
    t->h = 0x811C9DC5u;
    t->rc = 0;
    t->xa = SH2O_VEC_SEED;
    t->ya = SH2O_GTE_SEED;
    t->xb = SH2O_GTE_SEED;
    t->xc = SH2O_GTE_SEED ^ 0x5A5A5A5Au;
    sh2o_zam_delegated = sh2o_zam_refused = 0;
}

int sh2o_zam_test_digest_part(uint32_t first,uint32_t n,sh2o_zam_tally *t) {
    uint32_t i;
    for(i = first; i < first + n; i++) {
        if(i < SH2O_VEC_COUNT) {
            sh2o_zam_vector(t,i);
        } else if(i < SH2O_VEC_COUNT + SH2O_ZAM_SEEDED) {
            sh2o_zam_seeded(t,i - SH2O_VEC_COUNT);
        } else {
            sh2o_zam_gate(t,(unsigned)(i - SH2O_VEC_COUNT - SH2O_ZAM_SEEDED));
        }
    }
    return t->rc;
}

static uint64_t sh2o_zam_xs = 0x5348325A414D5330ull;

static uint32_t sh2o_zam_next32(void) {
    sh2o_zam_xs ^= sh2o_zam_xs << 13;
    sh2o_zam_xs ^= sh2o_zam_xs >> 7;
    sh2o_zam_xs ^= sh2o_zam_xs << 17;
    return (uint32_t)(sh2o_zam_xs >> 32);
}

static void sh2o_zam_ratan2_one(sh2o_zam_trial *t,int32_t y,int32_t x) {
    int32_t a = sh2o_zam_ratan2(y,x),b = sh2o_psyq_ratan2(0,y,x);
    long e = (long)((((uint32_t)a - (uint32_t)b + 2048u) & 4095u)) - 2048;
    t->cases++;
    t->delegated++;
    if(a != b) {
        t->mismatches++;
        if((e < 0 ? -e : e) > t->maxerr)
            t->maxerr = e < 0 ? -e : e;
    }
}

/* zam-2: set 0 grid, 1 xorshift, 2 edges */
void sh2o_zam_ratan2_trial(sh2o_zam_trial t[3]) {
    static const int32_t ev[13]={0,1,-1,0x1FFFFF,0x200000,0x200001,-0x1FFFFF,-0x200000,-0x200001,-0x7FFFFFFF - 1,
        -0x7FFFFFFF,0x7FFFFFFF,0x7FFFFFFE};
    int32_t y,x;
    uint32_t i;
    unsigned a,b;
    for(y = -2048; y <= 2048; y++)
        for(x = -2048; x <= 2048; x++)
            sh2o_zam_ratan2_one(&t[0],y,x);
    sh2o_zam_xs = 0x5348325A414D5330ull;
    for(i = 0; i < 0x100000u; i++) {
        y = (int32_t)sh2o_zam_next32();
        x = (int32_t)sh2o_zam_next32();
        sh2o_zam_ratan2_one(&t[1],y,x);
    }
    for(a = 0; a < 13; a++)
        for(b = 0; b < 13; b++)
            sh2o_zam_ratan2_one(&t[2],ev[a],ev[b]);
}

static int sh2o_zam_same_matrix(const sh2o_psyq_matrix_t *a,const sh2o_psyq_matrix_t *b) {
    unsigned i,j;
    for(i = 0; i < 3; i++) {
        if(a->t[i] != b->t[i])
            return 0;
        for(j = 0; j < 3; j++)
            if(a->m[i][j] != b->m[i][j])
                return 0;
    }
    return a->pad == b->pad;
}

static void sh2o_zam_rand_matrix(sh2o_psyq_matrix_t *m,uint32_t *x) {
    unsigned i,j;
    for(i = 0; i < 3; i++) {
        for(j = 0; j < 3; j++)
            m->m[i][j] = (int16_t)(uint16_t)(sh2o_zam_lcg(x) >> 16);
        m->t[i] = (int32_t)sh2o_zam_lcg(x);
    }
    m->pad = (int16_t)(uint16_t)(sh2o_zam_lcg(x) >> 16);
}

/* zam-3: source k (k < 4099) into *src, then m1 drawn from *x: k 0 a fixed matrix, k 1 and 2 the extremes (pad, t as
 * k 0's), k >= 3 drawn from *x (sh2o_zam_test.h) */
void sh2o_zam_transpose_case(uint32_t k,uint32_t *x,sh2o_psyq_matrix_t *src,sh2o_psyq_matrix_t *m1) {
    unsigned i,j;
    if(k < 3u) {
        for(i = 0; i < 3; i++)
            for(j = 0; j < 3; j++)
                src->m[i][j] = k == 0 ? (int16_t)(0x101 * (3 * i + j + 1) - 0x800) :
                    (int16_t)((i + j + k - 1) & 1 ? -0x8000 : 0x7FFF);
        src->pad = 0x55;
        src->t[0] = 1,src->t[1] = 2,src->t[2] = 3;
    } else {
        sh2o_zam_rand_matrix(src,x);
    }
    sh2o_zam_rand_matrix(m1,x);
}

/* zam-3: one source matrix, apart (m1 random) and aliased */
static void sh2o_zam_transpose_one(sh2o_zam_trial *t,const sh2o_psyq_matrix_t *src,const sh2o_psyq_matrix_t *m1) {
    sh2o_psyq_matrix_t a0 = *src,a1 = *m1,b0 = *src,b1 = *m1;
    int ok;
    ok = sh2o_zam_TransposeMatrix(&a0,&a1) == &a1 && sh2o_psyq_TransposeMatrix(0,&b0,&b1) == &b1 &&
        sh2o_zam_same_matrix(&a0,&b0) && sh2o_zam_same_matrix(&a1,&b1);
    a0 = b0 = *src;
    ok = ok && sh2o_zam_TransposeMatrix(&a0,&a0) == &a0 && sh2o_psyq_TransposeMatrix(0,&b0,&b0) == &b0 &&
        sh2o_zam_same_matrix(&a0,&b0);
    t->cases += 2;
    t->delegated += 2;
    t->mismatches += !ok;
}

void sh2o_zam_transpose_trial(sh2o_zam_trial *t) {
    sh2o_psyq_matrix_t m,m1;
    uint32_t x = SH2O_ZAM_TR_SEED,k;
    for(k = 0; k < SH2O_ZAM_TR_SOURCES; k++) {
        sh2o_zam_transpose_case(k,&x,&m,&m1);
        sh2o_zam_transpose_one(t,&m,&m1);
    }
}

/* zam-4: item k's (a, b, h) into abh and its LCG image into *img, drawn from *x: k < 6 x 4096 a drawn (a, b, h),
 * then the edge set (a outermost, then b, then h) */
void sh2o_zam_fog_case(uint32_t k,uint32_t *x,sh2o_gte_t *img,int32_t abh[3]) {
    static const int32_t ev[8]={-0x7FFFFFFF - 1,-0x8000,-1,0,1,100,0x7FFF,0x7FFFFFFF};
    if(k < 6 * 4096u) {
        uint32_t r0 = sh2o_zam_lcg(x),r1 = sh2o_zam_lcg(x),r2 = sh2o_zam_lcg(x);
        if(k & 1u) {
            abh[0] = (int32_t)r0;
            abh[1] = (int32_t)r1;
        } else {
            abh[0] = (int32_t)(r0 & 0xFFFFu) - 0x8000;
            abh[1] = (int32_t)(r1 & 0xFFFFu) - 0x8000;
        }
        switch((k >> 1) % 6u) {
            case 0: abh[2] = 0; break;
            case 1: abh[2] = -1; break;
            case 2: abh[2] = 1; break;
            case 3: abh[2] = 0x3FF; break;
            case 4: abh[2] = (int32_t)(r2 & 0x3FFu); break;
            default: abh[2] = (int32_t)r2; break;
        }
    } else {
        uint32_t e = k - 6 * 4096u,hn = e & 3u;
        abh[0] = ev[e >> 5];
        abh[1] = ev[(e >> 2) & 7u];
        abh[2] = hn == 0 ? 0 : hn == 1 ? -1 : hn == 2 ? 1 : 0x3FF;
    }
    sh2o_zam_image(img,x,(*x & 1u) ? 0x0FFF0FFFu : 0xFFFFFFFFu);
}

/* zam-4: one (a, b, h) on the image img; 1 when the delegated words differ from the body's */
static void sh2o_zam_fog_one(sh2o_zam_trial *t,const sh2o_gte_t *img,int32_t a,int32_t b,int32_t h) {
    sh2o_gte_t ga,gb;
    uint32_t r;
    int d;
    ga = gb = *img;
    d = sh2o_zam_SetFogNearFar(&ga,a,b,h);
    t->cases++;
    if(!d) {
        t->refused++;
        for(r = 0; r < 64; r++)
            if(sh2o_gte_read(&ga,r) != sh2o_gte_read(img,r)) {
                t->mismatches++; /* a refusal wrote */
                return;
            }
        return;
    }
    t->delegated++;
    sh2o_psyq_SetFogNearFar(&gb,a,b,h);
    for(r = 0; r < 64; r++)
        if(sh2o_gte_read(&ga,r) != sh2o_gte_read(&gb,r)) {
            t->mismatches++;
            return;
        }
}

/* zam-4 items first..first+n-1 (sh2o_zam_fog_case) */
void sh2o_zam_fog_trial_part(uint32_t first,uint32_t n,sh2o_zam_trial *t,uint32_t *x) {
    uint32_t k;
    for(k = first; k < first + n; k++) {
        sh2o_gte_t img;
        int32_t abh[3];
        sh2o_zam_fog_case(k,x,&img,abh);
        sh2o_zam_fog_one(t,&img,abh[0],abh[1],abh[2]);
    }
}

void sh2o_zam_fog_trial(sh2o_zam_trial *t) {
    uint32_t x = SH2O_ZAM_FOG_SEED;
    sh2o_zam_fog_trial_part(0,SH2O_ZAM_FOG_ITEMS,t,&x);
}

/* the suite: see sh2o_zam_test.h; nvec, nseeded: the (A) and (B) prefixes run ((C) always whole); each set seeds its
 * own LCG, so a prefix is exactly the full suite's first cases */
int sh2o_zam_test_suite(sh2o_zam_tally *t,uint32_t nvec,uint32_t nseeded) {
    sh2o_zam_test_seed(t);
    (void)sh2o_zam_test_digest_part(0,nvec,t);
    (void)sh2o_zam_test_digest_part(SH2O_VEC_COUNT,nseeded,t);
    (void)sh2o_zam_test_digest_part(SH2O_VEC_COUNT + SH2O_ZAM_SEEDED,SH2O_ZAM_GATE_ITEMS,t);
    if(!t->rc && t->delegated == 0)
        t->rc = 0x4000;
    if(!t->rc && t->refused == 0)
        t->rc = 0x4001;
    return t->rc;
}

/* Test-only entry for an image on the SH-2: the suite above and the zam-3/zam-4 trials (not the zam-2
 * measurement), each stamped by now() before and after, into w[SH2O_ZAM_W_*] (sh2o_zam_test.h). The trials run
 * first: the suite is the long one. The suite here is the subset SH2O_ZAM_SUB_VEC, SH2O_ZAM_SUB_SEEDED
 * (sh2o_zam_test.h; the full one does not fit one boot of the image). mac_available: SH2ZAM's MAC.L gate on a plan
 * of one point (the shape sh2o_zam_mac3 builds). Returns the suite's rc, or the trials' code as tests/test_zam.c's. */
#include <sh2zam/sh2zam.h>
#include <sh2zam/batch_experimental.h>

int sh2o_zam_test_entry(uint32_t *w,uint32_t (*now)(void)) {
    sh2o_zam_tally t;
    sh2o_zam_trial tr={0,0,0,0,0},fog={0,0,0,0,0};
    sh2z_vec3_t point={1,2,3};
    sh2z_mat34_t rows={{{16,0,0,0},{0,16,0,0},{0,0,16,0}}};
    sh2z_mesh_view_t view;
    sh2z_transform_plan_t plan;
    int rc;
    w[SH2O_ZAM_W_MAC] = sh2z_mesh_view_init(&view,&point,1) && sh2z_transform_plan_init(&plan,&view,&rows) &&
        sh2z_transform_plan_mac_available(&plan);
    w[SH2O_ZAM_W_T0] = now();
    sh2o_zam_transpose_trial(&tr);
    sh2o_zam_fog_trial(&fog);
    w[SH2O_ZAM_W_T1] = now();
    w[SH2O_ZAM_W_TR_CASES] = (uint32_t)tr.cases;
    w[SH2O_ZAM_W_TR_DELEG] = (uint32_t)tr.delegated;
    w[SH2O_ZAM_W_TR_MISM] = (uint32_t)tr.mismatches;
    w[SH2O_ZAM_W_FOG_CASES] = (uint32_t)fog.cases;
    w[SH2O_ZAM_W_FOG_DELEG] = (uint32_t)fog.delegated;
    w[SH2O_ZAM_W_FOG_REFUSED] = (uint32_t)fog.refused;
    w[SH2O_ZAM_W_FOG_MISM] = (uint32_t)fog.mismatches;
    w[SH2O_ZAM_W_Z0] = now();
    rc = sh2o_zam_test_suite(&t,SH2O_ZAM_SUB_VEC,SH2O_ZAM_SUB_SEEDED);
    w[SH2O_ZAM_W_Z1] = now();
    if(!rc && tr.mismatches)
        rc = 0x5001;
    if(!rc && fog.mismatches)
        rc = 0x6001;
    if(!rc && (fog.delegated == 0 || fog.refused == 0))
        rc = 0x6002;
    w[SH2O_ZAM_W_CASES] = t.cases;
    w[SH2O_ZAM_W_DELEG] = t.delegated;
    w[SH2O_ZAM_W_REFUSED] = t.refused;
    w[SH2O_ZAM_W_DIGEST] = t.h;
    w[SH2O_ZAM_W_MAC3_DELEG] = sh2o_zam_delegated;
    w[SH2O_ZAM_W_MAC3_REFUSED] = sh2o_zam_refused;
    w[SH2O_ZAM_W_RC] = (uint32_t)rc;
    return rc;
}

/* zam-5's run i (sh2o_zam_list_case: image, command, v; returns n; call it with i = 0, 1, ... from
 * SH2O_ZAM_LIST_SEED). zam-5's proof (vectors): 1024 runs on LCG images from SH2O_GTE_SEED ^ 4C495354h (odd runs
 * tamed 0x0FFF0FFF, V0 words tamed in runs with bit 1 set): runs 0-511 a fixed command (MVMVA 4A486012h: sf 1, RT, V0, no translation,
 * lm 0; n = 8), runs 512-1023 a drawn MVMVA selector and n = 1 + run % SH2O_ZAM_LIST_MAX. Each run: the delegate (the
 * list when it refuses) against the list on the GTE core's own kernel, all 64 registers and every out word; on a
 * delegated run with n >= 2 and items 0 and 1 unequal, the list with those two swapped (planted) must differ. */
unsigned sh2o_zam_list_case(uint32_t i,uint32_t *x,sh2o_gte_t *img,uint32_t *cmd,uint32_t (*v)[2]) {
    uint32_t vm = (i & 2u) ? 0x0FFF0FFFu : 0xFFFFFFFFu;
    unsigned n = i < 512u ? 8u : 1u + i % SH2O_ZAM_LIST_MAX,k;
    sh2o_zam_image(img,x,(i & 1u) ? 0x0FFF0FFFu : 0xFFFFFFFFu);
    *cmd = i < 512u ? 0x4A486012u : sh2o_zam_mvmva(sh2o_zam_lcg(x) >> 24);
    for(k = 0; k < n; k++) {
        v[k][0] = sh2o_zam_lcg(x) & vm;
        v[k][1] = sh2o_zam_lcg(x) & vm;
    }
    return n;
}

void sh2o_zam_list_trial(sh2o_zam_ltrial *t) {
    uint32_t x = SH2O_ZAM_LIST_SEED,i;
    for(i = 0; i < 1024u; i++) {
        static uint32_t v[SH2O_ZAM_LIST_MAX][2],oa[SH2O_ZAM_LIST_MAX][6],ob[SH2O_ZAM_LIST_MAX][6];
        sh2o_gte_t img,a,b;
        uint32_t cmd,r;
        unsigned n,k;
        int d,bad = 0;
        n = sh2o_zam_list_case(i,&x,&img,&cmd,v);
        a = img;
        b = img;
        sh2o_zam_bypass = 0;
        d = sh2o_zam_mvmva_list(&a,cmd,(const uint32_t (*)[2])v,n,oa);
        sh2o_zam_bypass = 1;
        if(!d)
            sh2o_gte_mvmva_list(&a,cmd,(const uint32_t (*)[2])v,n,oa);
        sh2o_gte_mvmva_list(&b,cmd,(const uint32_t (*)[2])v,n,ob);
        for(r = 0; r < 64; r++)
            bad |= sh2o_gte_read(&a,r) != sh2o_gte_read(&b,r);
        for(k = 0; k < n; k++)
            for(r = 0; r < 6; r++)
                bad |= oa[k][r] != ob[k][r];
        t->runs++;
        t->items += n;
        t->mismatches += (unsigned long)bad;
        if(!d) {
            t->refused++;
        } else {
            t->delegated++;
            if(n >= 2u && (v[0][0] != v[1][0] || v[0][1] != v[1][1])) {
                uint32_t s0 = v[0][0],s1 = v[0][1];
                int diff = 0;
                v[0][0] = v[1][0];
                v[0][1] = v[1][1];
                v[1][0] = s0;
                v[1][1] = s1;
                b = img;
                sh2o_gte_mvmva_list(&b,cmd,(const uint32_t (*)[2])v,n,ob);
                for(k = 0; k < n; k++)
                    for(r = 0; r < 6; r++)
                        diff |= oa[k][r] != ob[k][r];
                t->planted++;
                t->caught += (unsigned long)diff;
            }
        }
        sh2o_zam_bypass = 0;
    }
}
#endif /* SH2O_ZAM */
