/* Byte-exact test of sh2o_gte_dpct32 against the oracle and against sh2o_gte_dpct.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* 1. Every DPCT (op 2Ah) cmd vector of tests/gte_oracle.h: its 64 registers written through sh2o_gte_write in the
 *    write order of tests/suite/sh2o_gte_test_suite.c (sh2o_gte_order) on a zeroed image, then sh2o_gte_dpct32 with
 *    the vector's word; the 64 words of sh2o_gte_read must equal the vector's result.
 * 2. DIFF_IMAGES images from an LCG (x = x * 1664525 + 1013904223 from DIFF_SEED), edge-biased (FC near INT32_MIN
 *    and INT32_MAX and at the step-1 overflow bound INT32_MIN + 16 b, IR0 -8000h and 7FFFh, RGB bytes 0 and FFh,
 *    the CODE byte, every sf and lm), run through sh2o_gte_dpct and sh2o_gte_dpct32 on copies: all 64 words of the
 *    image must be equal.
 * 3. Mutants: the kernel source built again under other names with each planted bug of SH2OI_DPCT32_MUTANT; each
 *    must differ on steps 1 and 2 at least once.
 * Any failure: exit code 1. */
#include "sh2o/gte.h"
#include "gte_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The mutants: the kernel's source under other names. */
void sh2oi_dpct32_mutant_1(sh2o_gte_t *g,uint32_t cmd);
void sh2oi_dpct32_mutant_2(sh2o_gte_t *g,uint32_t cmd);
void sh2oi_dpct32_mutant_3(sh2o_gte_t *g,uint32_t cmd);
#define sh2o_gte_dpct32 sh2oi_dpct32_mutant_1
#define SH2OI_DPCT32_MUTANT 1
#include "../src/sh2o_gte_dpct32.c"
#undef SH2OI_DPCT32_MUTANT
#undef sh2o_gte_dpct32
#define sh2o_gte_dpct32 sh2oi_dpct32_mutant_2
#define SH2OI_DPCT32_MUTANT 2
#include "../src/sh2o_gte_dpct32.c"
#undef SH2OI_DPCT32_MUTANT
#undef sh2o_gte_dpct32
#define sh2o_gte_dpct32 sh2oi_dpct32_mutant_3
#define SH2OI_DPCT32_MUTANT 3
#include "../src/sh2o_gte_dpct32.c"
#undef SH2OI_DPCT32_MUTANT
#undef sh2o_gte_dpct32

#define DIFF_IMAGES 1048576u
#define DIFF_SEED 0x44504354u
#define MUTANTS 3u

typedef void (*dpct_fn)(sh2o_gte_t *g,uint32_t cmd);

static const dpct_fn mutants[MUTANTS]={sh2oi_dpct32_mutant_1,sh2oi_dpct32_mutant_2,sh2oi_dpct32_mutant_3};
static const char *const mutant_names[MUTANTS]={"step-1 overflow FLAG dropped","step-2 SAR on sf 0",
    "RGB0 read once"};

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

/* One oracle vector through f; returns the number of read words that differ from the vector's result. */
static unsigned oracle_run(const gte_vector_t *vector,dpct_fn f) {
    sh2o_gte_t g;
    unsigned differ=0;
    memset(&g,0,sizeof g);
    for(unsigned k=0;k<sizeof order;k++)
        sh2o_gte_write(&g,order[k],get_u32(vector->in+4*order[k]));
    f(&g,get_u32(vector->in+256));
    for(unsigned r=0;r<64;r++)
        differ+=sh2o_gte_read(&g,r)!=get_u32(vector->out+4+4*r);
    return differ;
}

/* An RGB byte: 0, FFh or any. */
static uint32_t rgb_byte(uint32_t *x) {
    uint32_t d=lcg(x);
    switch(d>>30) {
    case 0: return 0;
    case 1: return 0xFFu;
    default: return d>>8&0xFFu;
    }
}

/* An FC word, near the edges for a colour byte b. */
static uint32_t fc_word(uint32_t *x,uint32_t b) {
    uint32_t d=lcg(x),s=(lcg(x)>>16&0x1Fu)-16u; /* s: -16..15 */
    switch(d>>29) {
    case 0: return 0x80000000u+s+16u;                  /* INT32_MIN .. */
    case 1: return 0x7FFFFFFFu-(s+16u);                /* .. INT32_MAX */
    case 2: return 0x80000000u+(b<<4)+s;               /* the step-1 overflow bound */
    case 3: return (b<<4)+s+((d&1u)?0x8000u:0xFFFF8000u); /* step-1 IR saturation, sf 1 */
    case 4: return (b<<4)+(lcg(x)>>20)-0x800u;          /* sf 1 in range */
    case 5: return (lcg(x)>>12)-0x80000u;               /* sf 0 near the IR range */
    default: return lcg(x);
    }
}

/* One edge-biased image and command word. */
static void diff_image(uint32_t *x,sh2o_gte_t *g,uint32_t *cmd) {
    uint32_t b[3],d;
    for(unsigned r=0;r<64;r++)
        g->r[r]=lcg(x);
    for(unsigned k=0;k<3;k++)
        g->r[20+k]=rgb_byte(x)|rgb_byte(x)<<8|rgb_byte(x)<<16|(lcg(x)&0xFF000000u);
    d=lcg(x);
    if(d>>30==0)
        g->r[6]=(g->r[6]&0x00FFFFFFu)|((d&1u)?0xFF000000u:0u);
    d=lcg(x);
    switch(d>>29) {
    case 0: g->r[8]=0xFFFF8000u; break;
    case 1: g->r[8]=0x7FFFu; break;
    case 2: g->r[8]=0x1000u; break;
    case 3: g->r[8]=0; break;
    case 4: g->r[8]=d&0xFFFFu; break; /* not sign-extended */
    default: break;
    }
    /* b: a byte of RGB0, RGB1 or RGB2, so that the bound is near at some step */
    for(unsigned i=0;i<3;i++) {
        d=lcg(x);
        b[i]=g->r[20+d%3u]>>(8*i)&0xFFu;
        g->r[53+i]=fc_word(x,b[i]);
    }
    d=lcg(x);
    *cmd=(d&0xFFF7FBC0u)|0x2Au|((d>>31)<<19)|((d>>30&1u)<<10); /* sf, lm from bits 31, 30 */
    if((d&0x3u)==0)
        *cmd=(*cmd&0x01FFFFFFu)|0x4A000000u;
}

int main(void) {
    unsigned vectors=0,bad=0;
    unsigned long caught[MUTANTS]={0,0,0},diff_caught[MUTANTS]={0,0,0},differ=0,ovf=0;
    uint32_t x=DIFF_SEED;
    /* 1. the oracle's DPCT vectors */
    for(size_t i=0;i<sizeof gte_vectors/sizeof gte_vectors[0];i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        unsigned d;
        if(vector->op!=GTE_OP_CMD||(get_u32(vector->in+256)&0x3Fu)!=0x2Au)
            continue;
        vectors++;
        d=oracle_run(vector,sh2o_gte_dpct32);
        if(d) {
            fprintf(stderr,"dpct32: oracle vector %zu (word %08x): %u words differ\n",i,get_u32(vector->in+256),d);
            bad=1;
        }
        for(unsigned m=0;m<MUTANTS;m++)
            caught[m]+=oracle_run(vector,mutants[m])!=0;
    }
    printf("dpct32: oracle: %u DPCT vectors, %s\n",vectors,bad?"FAIL":"0 differ");
    if(vectors==0) {
        fprintf(stderr,"dpct32: the oracle has no DPCT vector\n");
        bad=1;
    }
    /* 2. the differential against sh2o_gte_dpct */
    for(uint32_t n=0;n<DIFF_IMAGES;n++) {
        sh2o_gte_t in,a,b;
        uint32_t cmd;
        diff_image(&x,&in,&cmd);
        a=in;
        b=in;
        sh2o_gte_dpct(&a,cmd);
        sh2o_gte_dpct32(&b,cmd);
        ovf+=(a.r[63]&0x0E000000u)!=0;
        if(memcmp(a.r,b.r,sizeof a.r)) {
            if(!differ) {
                for(unsigned r=0;r<64;r++)
                    if(a.r[r]!=b.r[r]) {
                        fprintf(stderr,"dpct32: image %u (cmd %08x): word %u is %08x (C), %08x (dpct32)\n",n,cmd,r,
                            a.r[r],b.r[r]);
                        break;
                    }
            }
            differ++;
        }
        for(unsigned m=0;m<MUTANTS;m++) {
            sh2o_gte_t c=in;
            mutants[m](&c,cmd);
            diff_caught[m]+=memcmp(a.r,c.r,sizeof a.r)!=0;
        }
    }
    printf("dpct32: differential: %u images (%lu with a step-1 overflow FLAG), %lu differ\n",DIFF_IMAGES,ovf,differ);
    if(differ)
        bad=1;
    /* 3. the mutants */
    for(unsigned m=0;m<MUTANTS;m++) {
        printf("dpct32: mutant %u (%s): %lu of %u oracle vectors, %lu of %u images differ%s\n",m+1,mutant_names[m],
            caught[m],vectors,diff_caught[m],DIFF_IMAGES,caught[m]+diff_caught[m]?"":"  NOT CAUGHT");
        if(caught[m]+diff_caught[m]==0)
            bad=1;
    }
    return bad?EXIT_FAILURE:EXIT_SUCCESS;
}
