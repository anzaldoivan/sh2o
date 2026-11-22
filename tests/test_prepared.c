/* Byte-exact test of the prepared MVMVA against the oracle and against sh2o_gte_mvmva.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Each check compares the 64 words of an image after sh2o_gte_mvmva_prepared with those of a copy after
 * sh2o_gte_mvmva; the shadow is loaded with all parts from the image before the call, unless the check says so.
 * 1. Every MVMVA (op 12h) cmd vector of tests/gte_oracle.h, written as tests/test_dpct32.c writes it: the 64 words
 *    of sh2o_gte_read must equal the vector's result, and the image that of sh2o_gte_mvmva.
 * 2. The digest: for each of the SH2O_VEC_COUNT vectors of tests/suite/sh2o_k_test.h, a zeroed image with the
 *    rotation words and the translation of sh2o_k_test_mvb3_in and V0 = v, cmd 80012h (sf 1, mx 0, v 0, cv 0,
 *    lm 0); FNV-1a 32 over MAC1-3 (each word high byte first). It must be DIGEST_PIN, as must the digest of
 *    sh2o_k_mvb3_c's sums SAR 12. The count of vectors in the gate is printed.
 * 3. OP_RUNS random images and selectors from OP_SEED (x = x * 1664525 + 1013904223; sf 1 in 7 of 8 runs; TR and
 *    BK small in 3 of 4 runs): no image may differ, and the kernel must run on OP_FAST_PIN runs.
 * 4. Partial loads: after a load of all parts, the rotation matrix changes and only its part is loaded again; then
 *    the translation changes and only its part is loaded again. Each selector of mx 0-2, cv 0, 1, 3 must then give
 *    the image of sh2o_gte_mvmva.
 * 5. The fallback: sf 0, mx 3, cv 2 and a translation outside the gate each give 0 and the image of sh2o_gte_mvmva.
 * 6. Mutants: the source built again under other names with each planted bug of SH2OI_PREPARED_MUTANT; each must
 *    give a difference in checks 1-3.
 * Any failure: exit code 1. */
#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "suite/sh2o_k_test.h"
#include "gte_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The mutants: the source under other names. */
void sh2oi_prepared_mutant_load_1(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
int sh2oi_prepared_mutant_mvmva_1(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);
void sh2oi_prepared_mutant_load_2(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
int sh2oi_prepared_mutant_mvmva_2(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);
void sh2oi_prepared_mutant_load_3(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
int sh2oi_prepared_mutant_mvmva_3(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);
#define sh2o_gte_prepared_load sh2oi_prepared_mutant_load_1
#define sh2o_gte_mvmva_prepared sh2oi_prepared_mutant_mvmva_1
#define SH2OI_PREPARED_MUTANT 1
#include "../src/sh2o_gte_prepared.c"
#undef SH2OI_PREPARED_MUTANT
#undef sh2o_gte_prepared_load
#undef sh2o_gte_mvmva_prepared
#define sh2o_gte_prepared_load sh2oi_prepared_mutant_load_2
#define sh2o_gte_mvmva_prepared sh2oi_prepared_mutant_mvmva_2
#define SH2OI_PREPARED_MUTANT 2
#include "../src/sh2o_gte_prepared.c"
#undef SH2OI_PREPARED_MUTANT
#undef sh2o_gte_prepared_load
#undef sh2o_gte_mvmva_prepared
#define sh2o_gte_prepared_load sh2oi_prepared_mutant_load_3
#define sh2o_gte_mvmva_prepared sh2oi_prepared_mutant_mvmva_3
#define SH2OI_PREPARED_MUTANT 3
#include "../src/sh2o_gte_prepared.c"
#undef SH2OI_PREPARED_MUTANT
#undef sh2o_gte_prepared_load
#undef sh2o_gte_mvmva_prepared

#define DIGEST_PIN 0x63A08AA3u
#define OP_RUNS 4096u
#define OP_SEED 0x53504B31u
#define OP_FAST_PIN 2297u
#define MUTANTS 3u

typedef void (*load_fn)(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
typedef int (*op_fn)(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);

typedef struct arm {
    load_fn load;
    op_fn op;
    const char *name;
} arm;

static const arm arms[1+MUTANTS]={
    {sh2o_gte_prepared_load,sh2o_gte_mvmva_prepared,"prepared"},
    {sh2oi_prepared_mutant_load_1,sh2oi_prepared_mutant_mvmva_1,"mutant 1 (preload t SHL 12)"},
    {sh2oi_prepared_mutant_load_2,sh2oi_prepared_mutant_mvmva_2,"mutant 2 (rows R SHL 3)"},
    {sh2oi_prepared_mutant_load_3,sh2oi_prepared_mutant_mvmva_3,"mutant 3 (gate <=)"},
};

/* The write order of tests/suite/sh2o_gte_test_suite.c (sh2o_gte_order): every register but 15, 29 and 31, 28
 * before IR1-3. */
static const unsigned char order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,30,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

static uint32_t get_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}

static uint32_t lcg(uint32_t *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

/* FNV-1a 32 over a word's four bytes, high first */
static uint32_t fnv32(uint32_t h,uint32_t u) {
    h=(h^(u>>24))*0x01000193u;
    h=(h^(u>>16&0xFFu))*0x01000193u;
    h=(h^(u>>8&0xFFu))*0x01000193u;
    return (h^(u&0xFFu))*0x01000193u;
}

/* in through a (shadow of all parts) and on a copy through sh2o_gte_mvmva: 1 when the 64 words differ;
 * *fast += the arm's return value; *out the arm's image when out is not NULL */
static int run(const arm *a,const sh2o_gte_t *in,uint32_t cmd,unsigned *fast,sh2o_gte_t *out) {
    sh2o_gte_t g0=*in,g1=*in;
    sh2o_gte_prepared_t p;
    sh2o_gte_mvmva(&g0,cmd);
    a->load(&p,&g1,SH2O_GTE_PREPARED_ALL);
    *fast+=(unsigned)a->op(&g1,&p,cmd);
    if(out)
        *out=g1;
    return memcmp(g0.r,g1.r,sizeof g0.r)!=0;
}

/* Check 1 for arm a: the count of vectors that differ (from the oracle or from sh2o_gte_mvmva). */
static unsigned oracle_check(const arm *a,unsigned *vectors,unsigned *fast) {
    unsigned bad=0;
    *vectors=0;
    *fast=0;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        sh2o_gte_t g,o;
        uint32_t cmd;
        unsigned d=0;
        if(vector->op!=GTE_OP_CMD||(get_u32(vector->in+256)&0x3Fu)!=0x12u)
            continue;
        (*vectors)++;
        cmd=get_u32(vector->in+256);
        memset(&g,0,sizeof g);
        for(unsigned k=0;k<sizeof order;k++)
            sh2o_gte_write(&g,order[k],get_u32(vector->in+4*order[k]));
        d=(unsigned)run(a,&g,cmd,fast,&o);
        for(unsigned r=0;r<64;r++)
            d+=sh2o_gte_read(&o,r)!=get_u32(vector->out+4+4*r);
        if(d&&a==&arms[0])
            fprintf(stderr,"prepared: oracle vector %zu (cmd %08x) differs\n",i,cmd);
        bad+=d!=0;
    }
    return bad;
}

/* Check 2: the digest of arm a (a NULL: sh2o_k_mvb3_c's sums SAR 12); *gated the vectors on the kernel path */
static uint32_t digest(const arm *a,unsigned *gated) {
    uint32_t x=SH2O_VEC_SEED,h=0x811C9DC5u,w[5],i;
    int32_t t[3];
    sh2o_vec e;
    *gated=0;
    for(i=0;i<SH2O_VEC_COUNT;i++) {
        uint32_t mac[3];
        sh2o_vec_get(i,&x,&e);
        sh2o_k_test_mvb3_in(&e,w,t);
        if(a) {
            sh2o_gte_t g;
            sh2o_gte_prepared_t p;
            memset(&g,0,sizeof g);
            for(unsigned j=0;j<5;j++)
                g.r[32+j]=w[j];
            for(unsigned j=0;j<3;j++)
                g.r[37+j]=(uint32_t)t[j];
            g.r[0]=(uint16_t)e.v[0]|(uint32_t)(uint16_t)e.v[1]<<16;
            g.r[1]=(uint16_t)e.v[2];
            a->load(&p,&g,SH2O_GTE_PREPARED_ALL);
            *gated+=(unsigned)a->op(&g,&p,0x80012u);
            for(unsigned j=0;j<3;j++)
                mac[j]=g.r[25+j];
        } else {
            int64_t o[3];
            sh2o_k_mvb3_c(w,e.v,t,o);
            for(unsigned j=0;j<3;j++)
                mac[j]=(uint32_t)(int32_t)(o[j]>>12);
        }
        for(unsigned j=0;j<3;j++)
            h=fnv32(h,mac[j]);
    }
    return h;
}

/* a random image: every word drawn; TR and BK small in most runs so that the gate takes them */
static void rand_image(sh2o_gte_t *g,uint32_t *x) {
    for(unsigned i=0;i<64;i++)
        g->r[i]=lcg(x);
    if(lcg(x)&3u)
        for(unsigned i=0;i<3;i++) {
            uint32_t a,s;
            a=lcg(x);
            s=lcg(x)&15u;
            g->r[37+i]=(uint32_t)((int32_t)a>>s);
            a=lcg(x);
            s=lcg(x)&15u;
            g->r[45+i]=(uint32_t)((int32_t)a>>s);
        }
}

/* Check 3 for arm a: the count of runs that differ; *fast the runs on the kernel path */
static unsigned op_check(const arm *a,unsigned *fast) {
    uint32_t x=OP_SEED;
    unsigned bad=0;
    *fast=0;
    for(unsigned k=0;k<OP_RUNS;k++) {
        sh2o_gte_t g;
        uint32_t cmd=(lcg(&x)&0x0007E400u)|0x00080012u;
        if((lcg(&x)&7u)==0)
            cmd&=~0x00080000u;
        rand_image(&g,&x);
        bad+=(unsigned)run(a,&g,cmd,fast,NULL);
    }
    return bad;
}

/* g and a copy through sh2o_gte_mvmva after cmd with the shadow p: 1 when the 64 words differ or the return value
 * is not want (want < 0: any) */
static int same(const sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd,int want) {
    sh2o_gte_t g0=*g,g1=*g;
    int rv;
    sh2o_gte_mvmva(&g0,cmd);
    rv=sh2o_gte_mvmva_prepared(&g1,p,cmd);
    return memcmp(g0.r,g1.r,sizeof g0.r)!=0||(want>=0&&rv!=want);
}

/* Check 4: the count of selectors that differ after the partial loads */
static unsigned partial_check(void) {
    static const unsigned cvs[3]={0,1,3};
    uint32_t x=0x50415254u;
    unsigned bad=0;
    for(unsigned k=0;k<256;k++) {
        sh2o_gte_t g;
        sh2o_gte_prepared_t p;
        for(unsigned i=0;i<64;i++)
            g.r[i]=lcg(&x);
        for(unsigned i=0;i<3;i++) {
            g.r[37+i]=(uint32_t)((int32_t)lcg(&x)>>12);
            g.r[45+i]=(uint32_t)((int32_t)lcg(&x)>>12);
        }
        sh2o_gte_prepared_load(&p,&g,SH2O_GTE_PREPARED_ALL);
        for(unsigned i=0;i<5;i++)
            g.r[32+i]=lcg(&x);
        sh2o_gte_prepared_load(&p,&g,SH2O_GTE_PREPARED_RT);
        for(unsigned mx=0;mx<3;mx++)
            for(unsigned c=0;c<3;c++)
                bad+=(unsigned)same(&g,&p,0x00080012u|mx<<17|cvs[c]<<13|(k&3u)<<15|(k>>2&1u)<<10,1);
        for(unsigned i=0;i<3;i++)
            g.r[37+i]=(uint32_t)((int32_t)lcg(&x)>>12);
        sh2o_gte_prepared_load(&p,&g,SH2O_GTE_PREPARED_TR);
        for(unsigned mx=0;mx<3;mx++)
            for(unsigned c=0;c<3;c++)
                bad+=(unsigned)same(&g,&p,0x00080012u|mx<<17|cvs[c]<<13|(k&3u)<<15|(k>>2&1u)<<10,1);
    }
    return bad;
}

/* Check 5: the count of fallback cases that differ or do not give 0 */
static unsigned fallback_check(void) {
    uint32_t x=0x46414C4Cu;
    unsigned bad=0;
    for(unsigned k=0;k<256;k++) {
        sh2o_gte_t g;
        sh2o_gte_prepared_t p;
        uint32_t v=(k&3u)<<15|(k>>2&1u)<<10;
        for(unsigned i=0;i<64;i++)
            g.r[i]=lcg(&x);
        g.r[37]=0x7FF40000u;
        g.r[38]=g.r[39]=0;
        g.r[45]=g.r[46]=g.r[47]=0;
        sh2o_gte_prepared_load(&p,&g,SH2O_GTE_PREPARED_ALL);
        bad+=(unsigned)same(&g,&p,0x00000012u|v,0);              /* sf 0 */
        bad+=(unsigned)same(&g,&p,0x00080012u|v|3u<<17|1u<<13,0); /* mx 3 */
        bad+=(unsigned)same(&g,&p,0x00080012u|v|2u<<13,0);        /* cv 2 */
        bad+=(unsigned)same(&g,&p,0x00080012u|v,0);               /* TR outside the gate */
        bad+=(unsigned)same(&g,&p,0x00080012u|v|1u<<13,1);        /* BK inside */
    }
    return bad;
}

int main(void) {
    unsigned vectors,fast,gated,bad=0,d;
    uint32_t h,ref;
    /* 1 */
    d=oracle_check(&arms[0],&vectors,&fast);
    printf("prepared: oracle: %u MVMVA vectors (%u on the kernel path), %u differ\n",vectors,fast,d);
    if(d||vectors==0)
        bad=1;
    /* 2 */
    ref=digest(NULL,&gated);
    h=digest(&arms[0],&gated);
    printf("prepared: digest %08x (reference %08x, pin %08x), %u of %u vectors on the kernel path\n",(unsigned)h,
        (unsigned)ref,(unsigned)DIGEST_PIN,gated,(unsigned)SH2O_VEC_COUNT);
    if(h!=DIGEST_PIN||ref!=DIGEST_PIN)
        bad=1;
    /* 3 */
    d=op_check(&arms[0],&fast);
    printf("prepared: random: %u runs (%u on the kernel path, pin %u), %u differ\n",OP_RUNS,fast,OP_FAST_PIN,d);
    if(d||fast!=OP_FAST_PIN)
        bad=1;
    /* 4, 5 */
    d=partial_check();
    printf("prepared: partial loads: %u differ\n",d);
    if(d)
        bad=1;
    d=fallback_check();
    printf("prepared: fallback: %u differ\n",d);
    if(d)
        bad=1;
    /* 6 */
    for(unsigned m=1;m<=MUTANTS;m++) {
        unsigned ov,ovf,rf,dv,dr;
        dv=oracle_check(&arms[m],&ov,&ovf);
        h=digest(&arms[m],&gated);
        dr=op_check(&arms[m],&rf);
        printf("prepared: %s: %u of %u oracle vectors, digest %08x, %u of %u runs differ%s\n",arms[m].name,dv,ov,
            (unsigned)h,dr,OP_RUNS,dv||dr||h!=DIGEST_PIN?"":"  NOT CAUGHT");
        if(!dv&&!dr&&h==DIGEST_PIN)
            bad=1;
    }
    return bad?EXIT_FAILURE:EXIT_SUCCESS;
}
