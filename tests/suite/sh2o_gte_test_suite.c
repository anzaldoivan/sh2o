/* The gte suite: known answers and the digest of the GTE commands.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* sh2o_gte_test_suite() prints nothing. tests/test_gte.c calls it on the host; a program on the SH-2 can call it
 * too. SH-2 clean: constant shifts only, no divide or modulo. Case set (made again in Python by
 * tests/gte_model_check.py): LCG x = x * 1664525 + 1013904223 from SH2O_GTE_SEED; for each opcode of sh2o_gte_ops
 * in order, SH2O_GTE_PER_OP cases: one draw d for the command (lm = d bit 16, sf 17, and for MVMVA mx bits 18-19,
 * v 20-21, cv 22-23), then 64 draws, the words of registers 0..63; then the 256 MVMVA selectors s = 0..255
 * (lm = s bit 0, sf 1, cv 2-3, v 4-5, mx 6-7), 64 draws each. Odd cases within a group (k odd) AND each word with
 * 0x0FFF0FFF (tame). Words are written through sh2o_gte_write in sh2o_gte_order on a zeroed image, then
 * sh2o_gte_command. Digest: FNV-1a 32 over the 64 words of sh2o_gte_read (0..63) per case, each word's 4 bytes
 * most significant first. */
#include <stdint.h>

#include "sh2o_gte_test.h"

_Static_assert(SH2O_GTE_CASES==3840u,"14 x 256 cases and the 256 MVMVA selectors");

/* the 14 modelled opcodes, ascending */
static const uint8_t sh2o_gte_ops[SH2O_GTE_OPS]={0x01,0x06,0x10,0x12,0x13,0x16,0x1B,0x1E,0x20,0x28,0x2D,0x30,0x3D,
    0x3E};

const uint8_t sh2o_gte_order[SH2O_GTE_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,
    24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

typedef struct sh2o_gka {
    uint16_t at;   /* case index in the set: group * SH2O_GTE_PER_OP + 1 (the first tame case of the opcode) */
    uint8_t reg;   /* register read */
    uint32_t want; /* the value of the model (scripts/gte_model.py step) */
} sh2o_gka;

/* known answers: one per command, its main result register, values from the model */
static const sh2o_gka sh2o_gka_gte[]={
    {0*SH2O_GTE_PER_OP+1,14,0x03FFFF06u}, /* RTPS SXY2 */
    {1*SH2O_GTE_PER_OP+1,24,0xFFFAC4B7u}, /* NCLIP MAC0 */
    {2*SH2O_GTE_PER_OP+1,22,0x0E0000FFu}, /* DPCS RGB2 */
    {3*SH2O_GTE_PER_OP+1,25,0x010E1ADDu}, /* MVMVA MAC1 */
    {4*SH2O_GTE_PER_OP+1,22,0x0CFFFFFFu}, /* NCDS RGB2 */
    {5*SH2O_GTE_PER_OP+1,22,0x0DFFFFFFu}, /* NCDT RGB2 */
    {6*SH2O_GTE_PER_OP+1,22,0x0FFF0000u}, /* NCCS RGB2 */
    {7*SH2O_GTE_PER_OP+1,22,0x03FFFFFFu}, /* NCS RGB2 */
    {8*SH2O_GTE_PER_OP+1,22,0x0800FF00u}, /* NCT RGB2 */
    {9*SH2O_GTE_PER_OP+1,25,0x00000FBEu}, /* SQR MAC1 */
    {10*SH2O_GTE_PER_OP+1,7,0x0000185Du}, /* AVSZ3 OTZ */
    {11*SH2O_GTE_PER_OP+1,14,0x03FF03FFu}, /* RTPT SXY2 */
    {12*SH2O_GTE_PER_OP+1,22,0x06202426u}, /* GPF RGB2 */
    {13*SH2O_GTE_PER_OP+1,22,0x01FFFFFFu}, /* GPL RGB2 */
};

_Static_assert(sizeof sh2o_gka_gte/sizeof sh2o_gka_gte[0]==14u,"one known answer per opcode");

static uint32_t sh2o_gte_lcg(uint32_t *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

/* case i of the set (i = 0, 1, ... in order: advances *x) into g; -> its command word */
static uint32_t sh2o_gte_case(uint32_t i,uint32_t *x,sh2o_gte_t *g) {
    uint32_t w[64],cmd,mask,op;
    unsigned k;
    if(i<SH2O_GTE_OPS*SH2O_GTE_PER_OP) {
        uint32_t d=sh2o_gte_lcg(x);
        op=sh2o_gte_ops[i>>SH2O_GTE_PER_OP_LOG2];
        cmd=op|((d>>16)&1u)<<10|((d>>17)&1u)<<19;
        if(op==0x12u)
            cmd|=((d>>18)&3u)<<17|((d>>20)&3u)<<15|((d>>22)&3u)<<13;
    } else {
        uint32_t s=(i-SH2O_GTE_OPS*SH2O_GTE_PER_OP)&0xFFu;
        cmd=0x12u|(s&1u)<<10|((s>>1)&1u)<<19|((s>>2)&3u)<<13|((s>>4)&3u)<<15|((s>>6)&3u)<<17;
    }
    mask=(i&1u)?0x0FFF0FFFu:0xFFFFFFFFu;
    for(k=0;k<64;k++)
        w[k]=sh2o_gte_lcg(x)&mask;
    g->r[15]=g->r[29]=g->r[31]=0; /* the order writes every other register: a zeroed image (no memset call) */
    for(k=0;k<SH2O_GTE_ORDER_COUNT;k++)
        sh2o_gte_write(g,sh2o_gte_order[k],w[sh2o_gte_order[k]]);
    return cmd;
}

static uint32_t sh2o_fnv32(uint32_t h,uint32_t v) {
    h=(h^(v>>24))*0x01000193u;
    h=(h^((v>>16)&0xFFu))*0x01000193u;
    h=(h^((v>>8)&0xFFu))*0x01000193u;
    return (h^(v&0xFFu))*0x01000193u;
}

/* the digest over cases first..first+n-1 (s = {generator x, hash}); *ka = 0 when each known answer in the range
 * holds, else 1 + the index of the first failing one (0xFF: a command returned -1) */
static void sh2o_gte_run(uint32_t first,uint32_t n,uint32_t s[2],int *ka) {
    uint32_t x=s[0],h=s[1],i;
    unsigned a,r;
    sh2o_gte_t g;
    *ka=0;
    for(i=first;i<first+n;i++) {
        uint32_t cmd=sh2o_gte_case(i,&x,&g);
        if(sh2o_gte_command(&g,cmd)!=0&&!*ka)
            *ka=0xFF;
        for(r=0;r<64;r++)
            h=sh2o_fnv32(h,sh2o_gte_read(&g,r));
        for(a=0;a<sizeof sh2o_gka_gte/sizeof sh2o_gka_gte[0];a++)
            if(sh2o_gka_gte[a].at==i&&sh2o_gte_read(&g,sh2o_gka_gte[a].reg)!=sh2o_gka_gte[a].want&&!*ka)
                *ka=(int)a+1;
    }
    s[0]=x;
    s[1]=h;
}

int sh2o_gte_test_digest_part(uint32_t first,uint32_t n,uint32_t s[2]) {
    int ka;
    sh2o_gte_run(first,n,s,&ka);
    return ka?0x200+ka:0;
}

uint32_t sh2o_gte_test_digest(void) {
    uint32_t s[2]={SH2O_GTE_SEED,0x811C9DC5u};
    (void)sh2o_gte_test_digest_part(0,SH2O_GTE_CASES,s);
    return s[1];
}

/* 0 = pass; else 0x200 + 1 + the index of the failing known answer (0x2FF: a command returned -1), or 0x300 for a
 * digest mismatch */
int sh2o_gte_test_suite(void) {
    uint32_t s[2]={SH2O_GTE_SEED,0x811C9DC5u};
    int ka;
    sh2o_gte_run(0,SH2O_GTE_CASES,s,&ka);
    if(ka)
        return 0x200+ka;
    if(s[1]!=SH2O_DIGEST_GTE)
        return 0x300;
    return 0;
}
