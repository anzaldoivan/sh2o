/* Bridge for the SH-2 model run of the listprep family (the prepared MVMVA list, beside sh2o_gte_mvmva_list).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the library functions sh2o_gte_write, sh2o_gte_read, sh2o_gte_mvmva_list,
 * sh2o_gte_mvmva_list_prepared and sh2o_gte_prepared_load directly; the two lists have five arguments: the harness
 * puts the fifth (out) on the stack.
 * One resumable check runs in the ELF, ported from tests/test_list_prepared.c check 2 (same seed, same draw order);
 * each call takes a part of the runs so that no call reaches the instruction limit of the model:
 * sh2o_model_listprep_check_part(n, s): runs s[1]..s[1]+n-1; s = {generator x, next run, runs in the gate, runs
 *   that differ from sh2o_gte_mvmva_list, runs in the gate whose swapped items differ from the reference}.
 * Buffer layout (bytes): 0..255 image G0, 256..511 image G1, 512..639 the items v[16][2], 640..1023 the out rows of
 * G0's list out[16][6], 1024..1407 those of G1's list, 1408..1599 one sh2o_gte_prepared_t (184 bytes), 1600..1619
 * the check state s[5]. */
#include <stddef.h>

#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/list.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_listprep_io[2048];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts two sh2o_gte_t images at the start of the io buffer");
_Static_assert(sizeof(sh2o_gte_prepared_t)<=192u,"the harness puts one shadow at byte 1408 of the io buffer");
_Static_assert(offsetof(sh2o_gte_prepared_t,ok)==180u,"the harness reads ok at this offset");

#define SH2O_MODEL_LISTPREP_N 8u

/* The images of the check: [0] through sh2o_gte_mvmva_list, [1] and [2] through sh2o_gte_mvmva_list_prepared with
 * the items and with items 0 and 1 swapped. */
static sh2o_gte_t sh2o_model_listprep_g[3];
static uint32_t sh2o_model_listprep_v[2][SH2O_MODEL_LISTPREP_N][2];
static uint32_t sh2o_model_listprep_o[3][SH2O_MODEL_LISTPREP_N][6];

static uint32_t sh2o_model_listprep_lcg(uint32_t *x)
{
    *x=*x*1664525u+1013904223u;
    return *x;
}

/* 1 when cmd on image g is in the gate of src/sh2o_list_prepared.c without SR.S */
static uint32_t sh2o_model_listprep_gate(const sh2o_gte_t *g,uint32_t cmd)
{
    unsigned mx=cmd>>17&3u,cv=cmd>>13&3u,i;
    if(!(cmd>>19&1u)||(cmd>>15&3u)||mx==3u||cv==2u)
        return 0;
    if(cv==3u)
        return 1;
    for(i=0;i<3;i++)
        if(!(g->r[(cv?45u:37u)+i]+0x7FF3FFFFu<0xFFE7FFFFu))
            return 0;
    return 1;
}

/* 1 when image a and rows oa differ from image b and rows ob */
static uint32_t sh2o_model_listprep_differ(unsigned a,unsigned b)
{
    uint32_t d=0;
    unsigned i,j;
    for(i=0;i<64;i++)
        d|=sh2o_model_listprep_g[a].r[i]^sh2o_model_listprep_g[b].r[i];
    for(i=0;i<SH2O_MODEL_LISTPREP_N;i++)
        for(j=0;j<6;j++)
            d|=sh2o_model_listprep_o[a][i][j]^sh2o_model_listprep_o[b][i][j];
    return d!=0u;
}

/* Check 2 of tests/test_list_prepared.c over runs s[1]..s[1]+n-1. Each draw is its own statement, in the order of
 * rand_image and run_check there (left operand first). Copies and zeroing go through volatile pointers (no memcpy
 * or memset call; the ELF has no C library). */
void sh2o_model_listprep_check_part(uint32_t n,uint32_t s[5]);
void sh2o_model_listprep_check_part(uint32_t n,uint32_t s[5])
{
    uint32_t x=s[0],k=s[1],accepted=s[2],differ=s[3],caught=s[4],end=s[1]+n;
    sh2o_gte_t *g=sh2o_model_listprep_g;
    uint32_t (*v)[2]=sh2o_model_listprep_v[0],(*vs)[2]=sh2o_model_listprep_v[1];
    for(;k<end;k++) {
        uint32_t cmd=0x4A486012u;
        unsigned m=SH2O_MODEL_LISTPREP_N,i,j;
        if(k&1u) {
            cmd=(sh2o_model_listprep_lcg(&x)&0x0007E400u)|0x00080012u;
            m=1u+(sh2o_model_listprep_lcg(&x)&7u);
        }
        for(i=0;i<64;i++)
            g[0].r[i]=sh2o_model_listprep_lcg(&x);
        if(sh2o_model_listprep_lcg(&x)&3u)
            for(i=0;i<3;i++) {
                uint32_t a,sh;
                a=sh2o_model_listprep_lcg(&x);
                sh=sh2o_model_listprep_lcg(&x)&15u;
                g[0].r[37+i]=(uint32_t)((int32_t)a>>sh);
                a=sh2o_model_listprep_lcg(&x);
                sh=sh2o_model_listprep_lcg(&x)&15u;
                g[0].r[45+i]=(uint32_t)((int32_t)a>>sh);
            }
        for(i=0;i<m;i++) {
            v[i][0]=sh2o_model_listprep_lcg(&x);
            v[i][1]=sh2o_model_listprep_lcg(&x);
            if(k&2u) { /* unit-normal-sized items */
                v[i][0]=(uint32_t)((int32_t)v[i][0]>>20<<16)|(v[i][0]>>4&0x0FFFu);
                v[i][1]=v[i][1]>>4&0x0FFFu;
            }
            vs[i][0]=v[i][0];
            vs[i][1]=v[i][1];
        }
        if(m>1) {
            vs[0][0]=v[1][0];
            vs[0][1]=v[1][1];
            vs[1][0]=v[0][0];
            vs[1][1]=v[0][1];
        }
        for(i=0;i<64;i++) {
            volatile uint32_t *c1=g[1].r,*c2=g[2].r;
            c1[i]=g[0].r[i];
            c2[i]=g[0].r[i];
        }
        for(i=0;i<SH2O_MODEL_LISTPREP_N;i++)
            for(j=0;j<6;j++) {
                volatile uint32_t *z0=sh2o_model_listprep_o[0][i],*z1=sh2o_model_listprep_o[1][i],
                    *z2=sh2o_model_listprep_o[2][i];
                z0[j]=0u;
                z1[j]=0u;
                z2[j]=0u;
            }
        sh2o_gte_mvmva_list(&g[0],cmd,(const uint32_t (*)[2])v,m,sh2o_model_listprep_o[0]);
        sh2o_gte_mvmva_list_prepared(&g[1],cmd,(const uint32_t (*)[2])v,m,sh2o_model_listprep_o[1]);
        differ+=sh2o_model_listprep_differ(0,1);
        if(sh2o_model_listprep_gate(&g[2],cmd)) {
            accepted++;
            sh2o_gte_mvmva_list_prepared(&g[2],cmd,(const uint32_t (*)[2])vs,m,sh2o_model_listprep_o[2]);
            caught+=sh2o_model_listprep_differ(0,2);
        }
    }
    s[0]=x;
    s[1]=k;
    s[2]=accepted;
    s[3]=differ;
    s[4]=caught;
}

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h, sh2o/list.h or sh2o/gte_prepared.h
 * changes and this file does not. */
typedef struct {
    void (*write)(sh2o_gte_t *g,unsigned reg,uint32_t v);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    void (*mvmva_list)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
    void (*mvmva_list_prepared)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
    void (*load)(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
    void (*check_part)(uint32_t n,uint32_t s[5]);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_gte_write,
    sh2o_gte_read,
    sh2o_gte_mvmva_list,
    sh2o_gte_mvmva_list_prepared,
    sh2o_gte_prepared_load,
    sh2o_model_listprep_check_part,
};
