/* Suite and driver of the PsyQ libgte bodies.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Modes: no argument, the test: it runs the suite (tests/suite), prints its result and digest, and fails unless the
 * suite returns 0 with the digest SH2O_DIGEST_PSYQ. --mutant <name>: the same run; it passes only when the suite
 * returns non-zero (CMakeLists.txt builds a mutant from a changed copy of src/ and include/). The test and the
 * mutants first run the case set once with a counting sh2o_psyq_break hook: the test fails unless no seeded case calls
 * it. --plant-break: that count only; it passes only when it is not 0 (psyq_plant_break: a copy of the suite where
 * SetFogNearFar's h is 0, a zero divisor).
 * --driver: the bodies fed one case per stdin line.
 * In:  "<routine> <64 hex words> <n> (<kind> <hex words>) x n <a> (<arg>) x a"
 *      the 64 words go through sh2o_gte_write in the order below (no 15, 29, 31) on a zeroed image; regions: kind M
 *      (8 words, a PS1 MATRIX), V (4, VECTOR), S (2, SVECTOR), L (1, long), K (161: PushMatrix/PopMatrix's top, stack
 *      words); words are the PS1 little-endian words, unpacked here by field; arg "r<k>" = region k's pointer, else
 *      hex.
 * Out: "<ret> (<region hex words>) x n <64 hex words>"  ret: "-" (void), "r<k>" (returned region k's pointer, "r?"
 *      none), else hex; then each region repacked, then sh2o_gte_read(0..63). A body that calls sh2o_psyq_break: ret
 *      is "brk<hex code>" (the model's break), "brk?" when it called it more than once; regions and registers
 *      follow. An unknown routine: exit code 2. */
#define _POSIX_C_SOURCE 200809L /* strtok_r */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh2o/sh2o.h"
#include "sh2o_psyq_test.h"

static const uint8_t order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,30,32,33,
    34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

typedef struct region {
    char kind;
    sh2o_psyq_matrix_t m;
    sh2o_psyq_vector_t v;
    sh2o_psyq_svector_t s;
    int32_t l;
    sh2o_psyq_mstack_t k;
} region;

static unsigned nwords(char kind) {
    return kind=='M'?8u:kind=='V'?4u:kind=='S'?2u:kind=='L'?1u:161u;
}

#define LO(w) ((int16_t)(uint16_t)((w)&0xFFFFu))
#define HI(w) ((int16_t)(uint16_t)((w)>>16))
#define W(lo,hi) ((uint32_t)(uint16_t)(lo)|(uint32_t)(uint16_t)(hi)<<16)

static void unpack(region *r,const uint32_t *w) {
    unsigned k;
    switch(r->kind) {
    case 'M':
        r->m.m[0][0]=LO(w[0]),r->m.m[0][1]=HI(w[0]),r->m.m[0][2]=LO(w[1]),r->m.m[1][0]=HI(w[1]);
        r->m.m[1][1]=LO(w[2]),r->m.m[1][2]=HI(w[2]),r->m.m[2][0]=LO(w[3]),r->m.m[2][1]=HI(w[3]);
        r->m.m[2][2]=LO(w[4]),r->m.pad=HI(w[4]);
        for(k=0;k<3;k++)
            r->m.t[k]=(int32_t)w[5+k];
        break;
    case 'V':
        r->v.vx=(int32_t)w[0],r->v.vy=(int32_t)w[1],r->v.vz=(int32_t)w[2],r->v.pad=(int32_t)w[3];
        break;
    case 'S': r->s.vx=LO(w[0]),r->s.vy=HI(w[0]),r->s.vz=LO(w[1]),r->s.pad=HI(w[1]); break;
    case 'L': r->l=(int32_t)w[0]; break;
    default:
        r->k.top=(int32_t)w[0];
        for(k=0;k<160;k++)
            r->k.w[k]=w[1+k];
    }
}

static void pack(const region *r,uint32_t *w) {
    unsigned k;
    switch(r->kind) {
    case 'M':
        w[0]=W(r->m.m[0][0],r->m.m[0][1]),w[1]=W(r->m.m[0][2],r->m.m[1][0]);
        w[2]=W(r->m.m[1][1],r->m.m[1][2]),w[3]=W(r->m.m[2][0],r->m.m[2][1]),
        w[4]=W(r->m.m[2][2],r->m.pad);
        for(k=0;k<3;k++)
            w[5+k]=(uint32_t)r->m.t[k];
        break;
    case 'V':
        w[0]=(uint32_t)r->v.vx,w[1]=(uint32_t)r->v.vy,w[2]=(uint32_t)r->v.vz,w[3]=(uint32_t)r->v.pad;
        break;
    case 'S': w[0]=W(r->s.vx,r->s.vy),w[1]=W(r->s.vz,r->s.pad); break;
    case 'L': w[0]=(uint32_t)r->l; break;
    default:
        w[0]=(uint32_t)r->k.top;
        for(k=0;k<160;k++)
            w[1+k]=r->k.w[k];
    }
}

static void *ptr(region *r) {
    switch(r->kind) {
    case 'M': return &r->m;
    case 'V': return &r->v;
    case 'S': return &r->s;
    case 'L': return &r->l;
    default: return &r->k;
    }
}

static uint32_t brk_code,brk_calls;

static void on_break(uint32_t code) {
    brk_code=code;
    brk_calls++;
}

static int driver(void) {
    static char line[16384];
    static region reg[4];
    while(fgets(line,sizeof line,stdin)) {
        char *save,*name=strtok_r(line," \t\r\n",&save),*t;
        uint32_t w[64],rw[161];
        void *ap[4]={0};
        uint32_t av[4]={0};
        unsigned n,na,i,k;
        sh2o_gte_t g;
        const void *ret=NULL;
        int rkind=0; /* 0 void, 1 pointer, 2 value */
        uint32_t rv=0;
        if(!name)
            continue;
        sh2o_psyq_break=on_break;
        brk_code=brk_calls=0;
        for(k=0;k<64;k++)
            w[k]=(uint32_t)strtoul(strtok_r(NULL," \t\r\n",&save),NULL,16);
        memset(&g,0,sizeof g);
        for(k=0;k<sizeof order;k++)
            sh2o_gte_write(&g,order[k],w[order[k]]);
        n=(unsigned)strtoul(strtok_r(NULL," \t\r\n",&save),NULL,10);
        for(i=0;i<n;i++) {
            reg[i].kind=strtok_r(NULL," \t\r\n",&save)[0];
            for(k=0;k<nwords(reg[i].kind);k++)
                rw[k]=(uint32_t)strtoul(strtok_r(NULL," \t\r\n",&save),NULL,16);
            unpack(&reg[i],rw);
        }
        na=(unsigned)strtoul(strtok_r(NULL," \t\r\n",&save),NULL,10);
        for(i=0;i<na;i++) {
            t=strtok_r(NULL," \t\r\n",&save);
            if(t[0]=='r') {
                ap[i]=ptr(&reg[t[1]-'0']);
            } else {
                av[i]=(uint32_t)strtoul(t,NULL,16);
            }
        }
#define I(k) ((int32_t)av[k])
        if(!strcmp(name,"SetRotMatrix")) {
            sh2o_psyq_SetRotMatrix(&g,ap[0]);
        } else if(!strcmp(name,"SetLightMatrix")) {
            sh2o_psyq_SetLightMatrix(&g,ap[0]);
        } else if(!strcmp(name,"SetColorMatrix")) {
            sh2o_psyq_SetColorMatrix(&g,ap[0]);
        } else if(!strcmp(name,"SetTransMatrix")) {
            sh2o_psyq_SetTransMatrix(&g,ap[0]);
        } else if(!strcmp(name,"SetBackColor")) {
            sh2o_psyq_SetBackColor(&g,I(0),I(1),I(2));
        } else if(!strcmp(name,"SetFarColor")) {
            sh2o_psyq_SetFarColor(&g,I(0),I(1),I(2));
        } else if(!strcmp(name,"SetGeomOffset")) {
            sh2o_psyq_SetGeomOffset(&g,I(0),I(1));
        } else if(!strcmp(name,"SetGeomScreen")) {
            sh2o_psyq_SetGeomScreen(&g,I(0));
        } else if(!strcmp(name,"InitGeom")) {
            sh2o_psyq_InitGeom(&g);
        } else if(!strcmp(name,"SetFogNearFar")) {
            sh2o_psyq_SetFogNearFar(&g,I(0),I(1),I(2));
        } else if(!strcmp(name,"TransMatrix")) {
            ret=sh2o_psyq_TransMatrix(&g,ap[0],ap[1]),rkind=1;
        } else if(!strcmp(name,"TransposeMatrix")) {
            ret=sh2o_psyq_TransposeMatrix(&g,ap[0],ap[1]),rkind=1;
        } else if(!strcmp(name,"CompMatrix")) {
            ret=sh2o_psyq_CompMatrix(&g,ap[0],ap[1],ap[2]),rkind=1;
        } else if(!strcmp(name,"ApplyMatrix")) {
            ret=sh2o_psyq_ApplyMatrix(&g,ap[0],ap[1],ap[2]),rkind=1;
        } else if(!strcmp(name,"ApplyMatrixLV")) {
            ret=sh2o_psyq_ApplyMatrixLV(&g,ap[0],ap[1],ap[2]),rkind=1;
        } else if(!strcmp(name,"RotTrans")) {
            sh2o_psyq_RotTrans(&g,ap[0],ap[1],ap[2]);
        } else if(!strcmp(name,"PushMatrix")) {
            rv=(uint32_t)sh2o_psyq_PushMatrix(&g,ap[0]),rkind=2;
        } else if(!strcmp(name,"PopMatrix")) {
            rv=(uint32_t)sh2o_psyq_PopMatrix(&g,ap[0]),rkind=2;
        } else if(!strcmp(name,"ratan2")) {
            rv=(uint32_t)sh2o_psyq_ratan2(&g,I(0),I(1)),rkind=2;
        } else {
            fprintf(stderr,"unknown routine %s\n",name);
            return 2;
        }
        if(brk_calls==1) {
            printf("brk%lx",(unsigned long)brk_code);
        } else if(brk_calls) {
            printf("brk?");
        } else if(rkind==0) {
            printf("-");
        } else if(rkind==2) {
            printf("%lx",(unsigned long)rv);
        } else {
            for(i=0;i<n&&ptr(&reg[i])!=ret;i++)
                ;
            if(i<n) {
                printf("r%u",i);
            } else {
                printf("r?");
            }
        }
        for(i=0;i<n;i++) {
            pack(&reg[i],rw);
            for(k=0;k<nwords(reg[i].kind);k++)
                printf(" %lx",(unsigned long)rw[k]);
        }
        for(k=0;k<64;k++)
            printf(" %lx",(unsigned long)sh2o_gte_read(&g,k));
        printf("\n");
    }
    return 0;
}

/* the hook's calls over the case set (seeded); the ratan2 known answers install their own hook */
static uint32_t seeded_breaks(void) {
    uint32_t s[2]={SH2O_PSYQ_SEED,0x811C9DC5u};
    brk_calls=0;
    sh2o_psyq_break=on_break;
    (void)sh2o_psyq_test_digest_part(0,SH2O_PSYQ_CASES,s);
    sh2o_psyq_break=0;
    return brk_calls;
}

static int matrix_stack_check(void) {
    static const struct { int32_t top; int push,pop; } rejects[]={
        {641,-1,-2},{639,-2,-2},{-1,-2,-1},{1,-2,-2}
    };
    sh2o_gte_t g={0},want;
    sh2o_psyq_mstack_t s={0},saved;
    for(unsigned i=0;i<8;i++)
        g.r[32+i]=0x12348000u+i;
    want=g;
    for(unsigned i=0;i<sizeof rejects/sizeof rejects[0];i++) {
        s.top=rejects[i].top;
        saved=s;
        if(sh2o_psyq_PushMatrix(&g,&s)!=rejects[i].push || memcmp(g.r,want.r,sizeof g.r) ||
            s.top!=saved.top || memcmp(s.w,saved.w,sizeof s.w) ||
            sh2o_psyq_PopMatrix(&g,&s)!=rejects[i].pop || memcmp(g.r,want.r,sizeof g.r) ||
            s.top!=saved.top || memcmp(s.w,saved.w,sizeof s.w)) {
            fprintf(stderr,"psyq stack: guard/state mismatch at top %ld\n",(long)saved.top);
            return 1;
        }
    }
    s.top=608;
    if(sh2o_psyq_PushMatrix(&g,&s) || s.top!=640 || s.w[156]!=0xffff8004u ||
        s.w[155]!=want.r[35] || s.w[157]!=want.r[37] || s.w[151]!=0) {
        fprintf(stderr,"psyq stack: signed push mismatch\n");
        return 1;
    }
    want.r[36]=0xffff8004u;
    memset(&g.r[32],0,8*sizeof g.r[0]);
    if(sh2o_psyq_PopMatrix(&g,&s) || s.top!=608 || memcmp(g.r,want.r,sizeof g.r)) {
        fprintf(stderr,"psyq stack: round-trip register mismatch\n");
        return 1;
    }
    s.top=640;
    s.w[156]=want.r[36]=0xfedc8001u;
    saved=s;
    if(sh2o_psyq_PopMatrix(&g,&s) || s.top!=608 || memcmp(s.w,saved.w,sizeof s.w) ||
        memcmp(g.r,want.r,sizeof g.r)) {
        fprintf(stderr,"psyq stack: raw pop register mismatch: word 36 expected %08lx got %08lx\n",
            (unsigned long)want.r[36],(unsigned long)g.r[36]);
        return 1;
    }
    return 0;
}

int main(int argc,char **argv) {
    uint32_t digest,breaks;
    int rc;
    if(argc==2&&strcmp(argv[1],"--driver")==0)
        return driver();
    if(argc!=1&&!(argc==2&&strcmp(argv[1],"--plant-break")==0)&&!(argc==3&&strcmp(argv[1],"--mutant")==0)) {
        fprintf(stderr,"usage: %s [--mutant <name> | --plant-break | --driver]\n",argv[0]);
        return EXIT_FAILURE;
    }
    breaks=seeded_breaks();
    printf("psyq: %lu break hook calls over the seeded cases\n",(unsigned long)breaks);
    if(argc==2)
        return breaks>0?0:EXIT_FAILURE;
    if(argc==1&&breaks!=0)
        return EXIT_FAILURE;
    rc=sh2o_psyq_test_suite();
    digest=sh2o_psyq_test_digest();
    if(argc==3) {
        printf("mutant %s: suite returned 0x%X, digest 0x%08lX\n",argv[2],(unsigned)rc,(unsigned long)digest);
        return rc!=0?0:EXIT_FAILURE;
    }
    printf("psyq: suite returned 0x%X, digest 0x%08lX (expected 0x%08lX)\n",(unsigned)rc,(unsigned long)digest,
        (unsigned long)SH2O_DIGEST_PSYQ);
    return rc==0&&digest==SH2O_DIGEST_PSYQ&&!matrix_stack_check()?0:EXIT_FAILURE;
}
