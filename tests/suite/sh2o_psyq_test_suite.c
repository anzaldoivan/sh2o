/* The psyq suite: known answers and the digest of the PsyQ libgte bodies.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* sh2o_psyq_test_suite() prints nothing. tests/test_psyq.c calls it on the host; a program on the SH-2 can call it
 * too. Constant shifts only, no divide or modulo here (the bodies' divides are src/sh2o_psyq.c's). Case set (made
 * again in Python through an R3000 model of the PS1 library objects): LCG x = x * 1664525 + 1013904223 from
 * SH2O_PSYQ_SEED; for each body of the enum below in order, SH2O_PSYQ_PER cases: 64 draws, the words of registers
 * 0..63 (case k odd: each AND 0x0FFF0FFF), written through sh2o_gte_write in the gte suite's order on a zeroed image;
 * then per region of the body, its PS1 words, one draw each (MATRIX 8, VECTOR 4, SVECTOR 2, long 1; CompMatrix's m0
 * each AND 0x0FFF0FFF, so its add never overflows; the matrix stack: top = min(draw >> 27, 20) * 32, then 160
 * words); then its integer arguments, one draw each (SetFogNearFar: a, b = (draw & FFFFh) - 8000h, h = draw & 3FFh;
 * ratan2: draws d1, d2, d3, y = d1 >> (d3 & 31), negated if d3 bit 5, x = d2 >> (d3 >> 8 & 31), negated if d3 bit
 * 13). Digest: FNV-1a 32 over per case the return word (void 0; pointer 1 + the argument index it equals, 0 none; int
 * its value), each region's PS1 words after the call, then the 64 words of sh2o_gte_read (0..63), each word's 4 bytes
 * most significant first. */
#include <stdint.h>

#include "sh2o_psyq_test.h"

_Static_assert(SH2O_PSYQ_CASES==1216u,"19 bodies x 64 cases");

/* the bodies, in case order; regions: M MATRIX, V VECTOR, S SVECTOR, L long, K stack; ints: integer arguments */
enum {
    B_APPLY,
    B_APPLYLV,
    B_COMP,
    B_INIT,
    B_POP,
    B_PUSH,
    B_ROTTRANS,
    B_BACK,
    B_COLOR,
    B_FAR,
    B_FOG,
    B_OFFSET,
    B_SCREEN,
    B_LIGHT,
    B_ROT,
    B_SETTRANS,
    B_TRANS,
    B_TRANSPOSE,
    B_RATAN2
};
static const char *const sh2o_psyq_regions[SH2O_PSYQ_BODIES]={
    "MSV","MVV","MMM","","K","K","SVL","","M","","","","","M","M","M","MV","MM",""};
static const uint8_t sh2o_psyq_ints[SH2O_PSYQ_BODIES]={0,0,0,0,0,0,0,3,0,3,3,2,1,0,0,0,0,0,3};

/* side-effect-free write order (the gte suite's): no 15, 29, 31 */
static const uint8_t sh2o_psyq_order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,
    27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

typedef struct sh2o_pka {
    int32_t y,x,want; /* ratan2(y, x): the value of the R3000 model on the library's ratan2 object */
    uint32_t brk;     /* the BRK code the model stops at (the hook's one call, return 0), or 0: no break */
} sh2o_pka;

static const sh2o_pka sh2o_pka_ratan2[]={
    {0,0,0,0},
    {0,-0x7FFFFFFF-1,0,0x1C00u}, /* 0 / 0 divide: `break 7` (PS1: BIOS hang; sh2o: hook, 0) */
    {-0x7FFFFFFF-1,0,0,0x1C00u}, /* 0 / 0 divide: `break 7` */
    {1,1,0x200,0},
    {1,-1,0x600,0},
    {-1,-1,-0x600,0},
};

_Static_assert(sizeof sh2o_pka_ratan2/sizeof sh2o_pka_ratan2[0]==6u,"six ratan2 known answers");

typedef sh2o_psyq_test_region_t sh2o_preg;

static uint32_t sh2o_psyq_brk,sh2o_psyq_brks;

/* the suite's break hook: the last code and the count of calls */
static void sh2o_psyq_on_break(uint32_t code) {
    sh2o_psyq_brk=code;
    sh2o_psyq_brks++;
}

static uint32_t sh2o_psyq_lcg(uint32_t *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

static uint32_t sh2o_psyq_fnv(uint32_t h,uint32_t v) {
    h=(h^(v>>24))*0x01000193u;
    h=(h^((v>>16)&0xFFu))*0x01000193u;
    h=(h^((v>>8)&0xFFu))*0x01000193u;
    return (h^(v&0xFFu))*0x01000193u;
}

#define P_LO(w) ((int16_t)(uint16_t)((w)&0xFFFFu))
#define P_HI(w) ((int16_t)(uint16_t)((w)>>16))
#define P_W(lo,hi) ((uint32_t)(uint16_t)(lo)|(uint32_t)(uint16_t)(hi)<<16)

/* draw region r's PS1 words (mask: CompMatrix's m0) and unpack them by field */
static void sh2o_psyq_draw(sh2o_preg *r,uint32_t *x,uint32_t mask) {
    uint32_t w[8];
    unsigned k;
    switch(r->kind) {
    case 'M':
        for(k=0;k<8;k++)
            w[k]=sh2o_psyq_lcg(x)&mask;
        r->m.m[0][0]=P_LO(w[0]),r->m.m[0][1]=P_HI(w[0]),r->m.m[0][2]=P_LO(w[1]),r->m.m[1][0]=P_HI(w[1]);
        r->m.m[1][1]=P_LO(w[2]),r->m.m[1][2]=P_HI(w[2]),r->m.m[2][0]=P_LO(w[3]),r->m.m[2][1]=P_HI(w[3]);
        r->m.m[2][2]=P_LO(w[4]),r->m.pad=P_HI(w[4]);
        for(k=0;k<3;k++)
            r->m.t[k]=(int32_t)w[5+k];
        break;
    case 'V':
        r->v.vx=(int32_t)sh2o_psyq_lcg(x),r->v.vy=(int32_t)sh2o_psyq_lcg(x);
        r->v.vz=(int32_t)sh2o_psyq_lcg(x),r->v.pad=(int32_t)sh2o_psyq_lcg(x);
        break;
    case 'S':
        w[0]=sh2o_psyq_lcg(x),w[1]=sh2o_psyq_lcg(x);
        r->s.vx=P_LO(w[0]),r->s.vy=P_HI(w[0]),r->s.vz=P_LO(w[1]),r->s.pad=P_HI(w[1]);
        break;
    case 'L':
        r->l=(int32_t)sh2o_psyq_lcg(x);
        break;
    default:
        w[0]=sh2o_psyq_lcg(x)>>27;
        r->k.top=(int32_t)((w[0]>20u?20u:w[0])<<5);
        for(k=0;k<160;k++)
            r->k.w[k]=sh2o_psyq_lcg(x);
    }
}

/* region r's PS1 words into the digest */
static uint32_t sh2o_psyq_hash_region(uint32_t h,const sh2o_preg *r) {
    unsigned k;
    switch(r->kind) {
    case 'M':
        h=sh2o_psyq_fnv(h,P_W(r->m.m[0][0],r->m.m[0][1]));
        h=sh2o_psyq_fnv(h,P_W(r->m.m[0][2],r->m.m[1][0]));
        h=sh2o_psyq_fnv(h,P_W(r->m.m[1][1],r->m.m[1][2]));
        h=sh2o_psyq_fnv(h,P_W(r->m.m[2][0],r->m.m[2][1]));
        h=sh2o_psyq_fnv(h,P_W(r->m.m[2][2],r->m.pad));
        for(k=0;k<3;k++)
            h=sh2o_psyq_fnv(h,(uint32_t)r->m.t[k]);
        return h;
    case 'V':
        h=sh2o_psyq_fnv(h,(uint32_t)r->v.vx),h=sh2o_psyq_fnv(h,(uint32_t)r->v.vy);
        h=sh2o_psyq_fnv(h,(uint32_t)r->v.vz);
        return sh2o_psyq_fnv(h,(uint32_t)r->v.pad);
    case 'S':
        h=sh2o_psyq_fnv(h,P_W(r->s.vx,r->s.vy));
        return sh2o_psyq_fnv(h,P_W(r->s.vz,r->s.pad));
    case 'L':
        return sh2o_psyq_fnv(h,(uint32_t)r->l);
    default:
        h=sh2o_psyq_fnv(h,(uint32_t)r->k.top);
        for(k=0;k<160;k++)
            h=sh2o_psyq_fnv(h,r->k.w[k]);
        return h;
    }
}

/* draw v >> (s & 31), negated when neg (a logical shift by constant steps) */
static int32_t sh2o_psyq_mag(uint32_t v,uint32_t s,uint32_t neg) {
    for(s&=31u;s;s--)
        v>>=1;
    return (int32_t)(neg?0u-v:v);
}

/* body b on the drawn state -> its return word (-1 when a pointer return names no argument) */
static uint32_t sh2o_psyq_call(unsigned b,sh2o_gte_t *g,sh2o_preg *r,const int32_t *a) {
    const void *p;
    switch(b) {
    case B_APPLY:
        p=sh2o_psyq_ApplyMatrix(g,&r[0].m,&r[1].s,&r[2].v);
        return p==&r[2].v?3u:0xFFFFFFFFu;
    case B_APPLYLV:
        p=sh2o_psyq_ApplyMatrixLV(g,&r[0].m,&r[1].v,&r[2].v);
        return p==&r[2].v?3u:0xFFFFFFFFu;
    case B_COMP:
        p=sh2o_psyq_CompMatrix(g,&r[0].m,&r[1].m,&r[2].m);
        return p==&r[2].m?3u:0xFFFFFFFFu;
    case B_INIT: sh2o_psyq_InitGeom(g); return 0;
    case B_POP: return (uint32_t)sh2o_psyq_PopMatrix(g,&r[0].k);
    case B_PUSH: return (uint32_t)sh2o_psyq_PushMatrix(g,&r[0].k);
    case B_ROTTRANS: sh2o_psyq_RotTrans(g,&r[0].s,&r[1].v,&r[2].l); return 0;
    case B_BACK: sh2o_psyq_SetBackColor(g,a[0],a[1],a[2]); return 0;
    case B_COLOR: sh2o_psyq_SetColorMatrix(g,&r[0].m); return 0;
    case B_FAR: sh2o_psyq_SetFarColor(g,a[0],a[1],a[2]); return 0;
    case B_FOG: sh2o_psyq_SetFogNearFar(g,a[0],a[1],a[2]); return 0;
    case B_OFFSET: sh2o_psyq_SetGeomOffset(g,a[0],a[1]); return 0;
    case B_SCREEN: sh2o_psyq_SetGeomScreen(g,a[0]); return 0;
    case B_LIGHT: sh2o_psyq_SetLightMatrix(g,&r[0].m); return 0;
    case B_ROT: sh2o_psyq_SetRotMatrix(g,&r[0].m); return 0;
    case B_SETTRANS: sh2o_psyq_SetTransMatrix(g,&r[0].m); return 0;
    case B_TRANS:
        p=sh2o_psyq_TransMatrix(g,&r[0].m,&r[1].v);
        return p==&r[0].m?1u:0xFFFFFFFFu;
    case B_TRANSPOSE:
        p=sh2o_psyq_TransposeMatrix(g,&r[0].m,&r[1].m);
        return p==&r[1].m?2u:0xFFFFFFFFu;
    default: return (uint32_t)sh2o_psyq_ratan2(g,a[0],a[1]);
    }
}

void sh2o_psyq_test_case_draw(uint32_t i,uint32_t s[2],sh2o_psyq_test_case_t *c) {
    uint32_t w[64],mask,x=s[0];
    unsigned b,k,nr;
    b=i>>SH2O_PSYQ_PER_LOG2;
    c->body=b;
    c->a[0]=c->a[1]=c->a[2]=0;
    mask=(i&1u)?0x0FFF0FFFu:0xFFFFFFFFu;
    for(k=0;k<64;k++)
        w[k]=sh2o_psyq_lcg(&x)&mask;
    c->g.r[15]=c->g.r[29]=c->g.r[31]=0;
    for(k=0;k<sizeof sh2o_psyq_order;k++)
        sh2o_gte_write(&c->g,sh2o_psyq_order[k],w[sh2o_psyq_order[k]]);
    for(nr=0;sh2o_psyq_regions[b][nr];nr++) {
        c->r[nr].kind=sh2o_psyq_regions[b][nr];
        sh2o_psyq_draw(&c->r[nr],&x,(b==B_COMP&&nr==0)?0x0FFF0FFFu:0xFFFFFFFFu);
    }
    c->nr=nr;
    if(b==B_FOG) {
        c->a[0]=(int32_t)(sh2o_psyq_lcg(&x)&0xFFFFu)-0x8000;
        c->a[1]=(int32_t)(sh2o_psyq_lcg(&x)&0xFFFFu)-0x8000;
        c->a[2]=(int32_t)(sh2o_psyq_lcg(&x)&0x3FFu);
    }
    else if(b==B_RATAN2) {
        uint32_t d1=sh2o_psyq_lcg(&x),d2=sh2o_psyq_lcg(&x),d3=sh2o_psyq_lcg(&x);
        c->a[0]=sh2o_psyq_mag(d1,d3,d3&0x20u);
        c->a[1]=sh2o_psyq_mag(d2,d3>>8,d3&0x2000u);
    }
    else {
        for(k=0;k<sh2o_psyq_ints[b];k++)
            c->a[k]=(int32_t)sh2o_psyq_lcg(&x);
    }
    s[0]=x;
}

void sh2o_psyq_test_case_hash(const sh2o_psyq_test_case_t *c,uint32_t ret,uint32_t s[2]) {
    uint32_t h=sh2o_psyq_fnv(s[1],ret);
    unsigned k;
    for(k=0;k<c->nr;k++)
        h=sh2o_psyq_hash_region(h,&c->r[k]);
    for(k=0;k<64;k++)
        h=sh2o_psyq_fnv(h,sh2o_gte_read(&c->g,k));
    s[1]=h;
}

/* the digest over cases first..first+n-1 (s = {generator x, hash}); *ka = 0 when every known answer in the range
 * holds and every pointer return is right, else 1 + the first failing known answer's index, or 0xFF for a pointer
 * return. The ratan2 known answers run when the range ends at case SH2O_PSYQ_CASES. */
static void sh2o_psyq_run(uint32_t first,uint32_t n,uint32_t s[2],int *ka) {
    static sh2o_psyq_test_case_t c;
    uint32_t i;
    unsigned k;
    *ka=0;
    for(i=first;i<first+n;i++) {
        uint32_t ret;
        sh2o_psyq_test_case_draw(i,s,&c);
        ret=sh2o_psyq_call(c.body,&c.g,c.r,c.a);
        if(ret==0xFFFFFFFFu&&c.body!=B_POP&&c.body!=B_PUSH&&c.body!=B_RATAN2&&!*ka)
            *ka=0xFF;
        sh2o_psyq_test_case_hash(&c,ret,s);
    }
    if(first+n!=SH2O_PSYQ_CASES)
        return;
    sh2o_psyq_break=sh2o_psyq_on_break;
    for(k=0;k<sizeof sh2o_pka_ratan2/sizeof sh2o_pka_ratan2[0];k++) {
        sh2o_psyq_brk=sh2o_psyq_brks=0;
        if((sh2o_psyq_ratan2(&c.g,sh2o_pka_ratan2[k].y,sh2o_pka_ratan2[k].x)!=sh2o_pka_ratan2[k].want||
            sh2o_psyq_brks!=(sh2o_pka_ratan2[k].brk!=0)||sh2o_psyq_brk!=sh2o_pka_ratan2[k].brk)&&!*ka)
            *ka=(int)k+1;
    }
    sh2o_psyq_break=0;
}

int sh2o_psyq_test_digest_part(uint32_t first,uint32_t n,uint32_t s[2]) {
    int ka;
    sh2o_psyq_run(first,n,s,&ka);
    return ka?0x400+ka:0;
}

uint32_t sh2o_psyq_test_digest(void) {
    uint32_t s[2]={SH2O_PSYQ_SEED,0x811C9DC5u};
    (void)sh2o_psyq_test_digest_part(0,SH2O_PSYQ_CASES,s);
    return s[1];
}

/* 0 = pass; else 0x400 + 1 + the failing known answer's index (0x4FF: a pointer return), or 0x500 for a digest
 * mismatch */
int sh2o_psyq_test_suite(void) {
    uint32_t s[2]={SH2O_PSYQ_SEED,0x811C9DC5u};
    int ka=sh2o_psyq_test_digest_part(0,SH2O_PSYQ_CASES,s);
    if(ka)
        return ka;
    if(s[1]!=SH2O_DIGEST_PSYQ)
        return 0x500;
    return 0;
}
