/* Byte-exact test of the prepared MVMVA list against the oracle and against sh2o_gte_mvmva_list.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* 1. Every vector of tests/list_oracle.h, written as tests/test_list.c writes it, through
 *    sh2o_gte_mvmva_list_prepared: the 16 out rows and the 64 words of sh2o_gte_read must equal the vector's
 *    result, and the image that of sh2o_gte_mvmva_list. The count of vectors on the kernel path is printed: a probe
 *    build of the source (its fallback renamed) counts the lists that do not fall back, and it must equal the count
 *    of the gate computed here (sf 1, v 0, mx < 3, cv != 2, each t of the translation inside |t| < 7FF40000h).
 * 2. LIST_RUNS random lists from LIST_SEED (x = x * 1664525 + 1013904223): even runs cmd 4A486012h and 8 items,
 *    odd runs a random selector (sf 1) and 1-8 items; a random image (TR and BK small in 3 of 4 runs); unit-normal
 *    sized items when run & 2. Draw order: cmd and n (odd runs), the image, then each item's two words. Out rows and
 *    image must equal those of sh2o_gte_mvmva_list; the count of runs in the gate is printed (LIST_ACCEPT_PIN).
 *    Input mutant: on the runs in the gate, items 0 and 1 swapped must differ from the reference (count printed).
 * 3. Mutants: the source built again under other names with each planted bug of SH2OI_LIST_PREPARED_MUTANT; each
 *    must give a difference in check 1 or 2.
 * Any failure: exit code 1. */
#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/list.h"
#include "suite/sh2o_list_test.h"
#include "list_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The mutants and the probe: the source under other names. */
void sh2oi_list_prepared_mutant_1(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
void sh2oi_list_prepared_mutant_2(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
void sh2oi_list_prepared_probe(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
void sh2oi_list_prepared_probe_fallback(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,
    uint32_t (*out)[6]);
#define sh2o_gte_mvmva_list_prepared sh2oi_list_prepared_mutant_1
#define SH2OI_LIST_PREPARED_MUTANT 1
#include "../src/sh2o_list_prepared.c"
#undef SH2OI_LIST_PREPARED_MUTANT
#undef sh2o_gte_mvmva_list_prepared
#define sh2o_gte_mvmva_list_prepared sh2oi_list_prepared_mutant_2
#define SH2OI_LIST_PREPARED_MUTANT 2
#include "../src/sh2o_list_prepared.c"
#undef SH2OI_LIST_PREPARED_MUTANT
#undef sh2o_gte_mvmva_list_prepared
#define sh2o_gte_mvmva_list_prepared sh2oi_list_prepared_probe
#define sh2o_gte_mvmva_list sh2oi_list_prepared_probe_fallback
#define SH2OI_LIST_PREPARED_MUTANT 0
#include "../src/sh2o_list_prepared.c"
#undef SH2OI_LIST_PREPARED_MUTANT
#undef sh2o_gte_mvmva_list
#undef sh2o_gte_mvmva_list_prepared

#define LIST_RUNS 1024u
#define LIST_N 8u
#define LIST_SEED 0x53504B32u
#define MUTANTS 2u

typedef void (*list_fn)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);

typedef struct arm {
    list_fn list;
    const char *name;
} arm;

static const arm arms[1+MUTANTS]={
    {sh2o_gte_mvmva_list_prepared,"list prepared"},
    {sh2oi_list_prepared_mutant_1,"mutant 1 (image from item 0)"},
    {sh2oi_list_prepared_mutant_2,"mutant 2 (FLAG bit order reversed)"},
};

static unsigned long probe_fallbacks;

/* The probe's fallback: counts the call, then today's list. */
void sh2oi_list_prepared_probe_fallback(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,
    uint32_t (*out)[6]) {
    probe_fallbacks++;
    sh2o_gte_mvmva_list(g,cmd,v,n,out);
}

/* The write order of the gte suite (sh2o_gte_order), as tests/test_list.c. */
static const uint8_t order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,30,32,33,
    34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

static uint32_t get_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}

static void put_u32(uint8_t *bytes,uint32_t value) {
    for(unsigned i=0;i<4;i++)
        bytes[i]=(uint8_t)(value>>(8*i));
}

static uint32_t lcg(uint32_t *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

/* 1 when cmd on image g is in the gate (without SR.S) */
static int in_gate(const sh2o_gte_t *g,uint32_t cmd) {
    unsigned mx=cmd>>17&3u,cv=cmd>>13&3u;
    if(!(cmd>>19&1u)||(cmd>>15&3u)||mx==3u||cv==2u)
        return 0;
    if(cv==3u)
        return 1;
    for(unsigned i=0;i<3;i++) {
        uint32_t u=g->r[(cv?45u:37u)+i];
        if(!(u+0x7FF3FFFFu<0xFFE7FFFFu))
            return 0;
    }
    return 1;
}

/* Check 1 for arm a: the count of vectors that differ from the oracle or from sh2o_gte_mvmva_list.
 * *gated: vectors with n > 0 in the gate. */
static unsigned oracle_check(const arm *a,unsigned *gated) {
    unsigned bad=0;
    *gated=0;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        sh2o_gte_t g0,g1;
        uint32_t items[SH2O_LIST_MAX][2],r0[SH2O_LIST_MAX][6],r1[SH2O_LIST_MAX][6],cmd;
        uint8_t out[LIST_OUT_MAX];
        unsigned n,k,c;
        memset(&g1,0,sizeof g1);
        memset(r0,0,sizeof r0);
        memset(r1,0,sizeof r1);
        memset(out,0,sizeof out);
        for(k=0;k<61;k++)
            sh2o_gte_write(&g1,order[k],get_u32(vector->in+4*order[k]));
        g0=g1;
        cmd=get_u32(vector->in+256);
        n=(unsigned)get_u32(vector->in+260);
        if(vector->op!=LIST_OP_LIST||n>SH2O_LIST_MAX) {
            fprintf(stderr,"list_prepared: vector has unknown op %u or n %u\n",vector->op,n);
            exit(EXIT_FAILURE);
        }
        for(k=0;k<SH2O_LIST_MAX;k++)
            for(c=0;c<2;c++)
                items[k][c]=get_u32(vector->in+264+8*k+4*c);
        *gated+=(unsigned)(n>0&&in_gate(&g1,cmd));
        sh2o_gte_mvmva_list(&g0,cmd,(const uint32_t (*)[2])items,n,r0);
        a->list(&g1,cmd,(const uint32_t (*)[2])items,n,r1);
        for(k=0;k<SH2O_LIST_MAX;k++)
            for(c=0;c<6;c++)
                put_u32(out+24*k+4*c,r1[k][c]);
        for(k=0;k<64;k++)
            put_u32(out+384+4*k,sh2o_gte_read(&g1,k));
        bad+=(unsigned)(memcmp(out,vector->out,vector->out_size)!=0||memcmp(g0.r,g1.r,sizeof g0.r)!=0||
            memcmp(r0,r1,sizeof r0)!=0);
    }
    return bad;
}

/* a random image: every word drawn; TR and BK small in most runs (as tests/test_prepared.c rand_image) */
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

/* Check 2 for arm a: the count of runs that differ; *accepted the runs in the gate, *swapped those of them whose
 * swapped input differs from the reference. */
static unsigned run_check(const arm *a,unsigned *accepted,unsigned *swapped) {
    uint32_t x=LIST_SEED;
    unsigned bad=0;
    *accepted=*swapped=0;
    for(unsigned k=0;k<LIST_RUNS;k++) {
        sh2o_gte_t g0,g1,g3;
        uint32_t v[LIST_N][2],vs[LIST_N][2],o0[LIST_N][6],o1[LIST_N][6],o3[LIST_N][6];
        uint32_t cmd=k&1u?(lcg(&x)&0x0007E400u)|0x00080012u:0x4A486012u;
        unsigned n=k&1u?1u+(lcg(&x)&7u):LIST_N;
        rand_image(&g0,&x);
        for(unsigned i=0;i<n;i++) {
            v[i][0]=lcg(&x);
            v[i][1]=lcg(&x);
            if(k&2u) { /* unit-normal-sized items */
                v[i][0]=(uint32_t)((int32_t)v[i][0]>>20)<<16|(v[i][0]>>4&0x0FFFu);
                v[i][1]=v[i][1]>>4&0x0FFFu;
            }
            vs[i][0]=v[i][0];
            vs[i][1]=v[i][1];
        }
        if(n>1) {
            vs[0][0]=v[1][0];
            vs[0][1]=v[1][1];
            vs[1][0]=v[0][0];
            vs[1][1]=v[0][1];
        }
        g1=g3=g0;
        memset(o0,0,sizeof o0);
        memset(o1,0,sizeof o1);
        memset(o3,0,sizeof o3);
        sh2o_gte_mvmva_list(&g0,cmd,(const uint32_t (*)[2])v,n,o0);
        a->list(&g1,cmd,(const uint32_t (*)[2])v,n,o1);
        bad+=(unsigned)(memcmp(g0.r,g1.r,sizeof g0.r)!=0||memcmp(o0,o1,sizeof o0)!=0);
        if(in_gate(&g3,cmd)) {
            (*accepted)++;
            a->list(&g3,cmd,(const uint32_t (*)[2])vs,n,o3);
            *swapped+=(unsigned)(memcmp(g0.r,g3.r,sizeof g0.r)!=0||memcmp(o0,o3,sizeof o0)!=0);
        }
    }
    return bad;
}

static int gate_paths_check(void) {
    static const uint32_t v[2][2]={{0xffff0001u,0x55550002u},{0x0003fffcu,0xaaaafffbu}};
    static const struct { uint32_t cmd,tr; unsigned n; unsigned long fallbacks; } cases[]={
        {0x80012u,0,0,0},                   /* empty, null buffers */
        {0x80012u,1,2,0},                   /* admitted RT and TR */
        {0x00012u,1,2,1},                   /* early refusal (sf=0) */
        {0x80012u,0x7ff40000u,2,1}          /* post-load refusal (TR bound) */
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        sh2o_gte_t g={0},want,probe;
        uint32_t out[2][6],ref[2][6],probe_out[2][6];
        unsigned n=cases[i].n;
        uint32_t cmd=cases[i].cmd;
        const uint32_t (*items)[2]=n?v:NULL;
        g.r[32]=g.r[34]=g.r[36]=0x1000u;
        g.r[37]=cases[i].tr;
        want=probe=g;
        sh2o_gte_mvmva_list(&want,cmd,items,n,n?ref:NULL);
        sh2o_gte_mvmva_list_prepared(&g,cmd,items,n,n?out:NULL);
        if(n&&memcmp(out,ref,sizeof out)) {
            fprintf(stderr,"list prepared: output mismatch on gate path %u\n",i);
            return 1;
        }
        for(unsigned j=0;j<64;j++) {
            if(g.r[j]!=want.r[j]||sh2o_gte_read(&g,j)!=sh2o_gte_read(&want,j)) {
                fprintf(stderr,"list prepared: final register mismatch on gate path %u at word %u\n",i,j);
                return 1;
            }
        }
        probe_fallbacks=0;
        sh2oi_list_prepared_probe(&probe,cmd,items,n,n?probe_out:NULL);
        if(probe_fallbacks!=cases[i].fallbacks) {
            fprintf(stderr,"list prepared: fallback mismatch on gate path %u\n",i);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    const arm probe={sh2oi_list_prepared_probe,"probe"};
    unsigned gated,pg,accepted,swapped,bad=0,d,pn=0;
    if(gate_paths_check())
        return EXIT_FAILURE;
    /* 1 */
    d=oracle_check(&arms[0],&gated);
    probe_fallbacks=0;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++)
        pn+=(unsigned)(get_u32(list_vectors[i].in+260)>0);
    if(oracle_check(&probe,&pg))
        bad=1;
    printf("list_prepared: oracle: %u vectors, %u differ, %u on the kernel path (probe: %u of %u lists with n > 0 "
        "do not fall back)\n",(unsigned)LIST_VECTOR_COUNT,d,gated,pn-(unsigned)probe_fallbacks,pn);
    if(d||gated!=pn-(unsigned)probe_fallbacks)
        bad=1;
    /* 2 */
    d=run_check(&arms[0],&accepted,&swapped);
    printf("list_prepared: random: %u runs, %u in the gate, %u differ; swapped items: %u of %u caught\n",LIST_RUNS,
        accepted,d,swapped,accepted);
    if(d||!swapped)
        bad=1;
    /* 3 */
    for(unsigned m=1;m<=MUTANTS;m++) {
        unsigned dv,dr,ma,ms;
        dv=oracle_check(&arms[m],&pg);
        dr=run_check(&arms[m],&ma,&ms);
        printf("list_prepared: %s: %u of %u oracle vectors, %u of %u runs differ%s\n",arms[m].name,dv,
            (unsigned)LIST_VECTOR_COUNT,dr,LIST_RUNS,dv||dr?"":"  NOT CAUGHT");
        if(!dv&&!dr)
            bad=1;
    }
    return bad?EXIT_FAILURE:EXIT_SUCCESS;
}
