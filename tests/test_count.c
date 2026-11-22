/* Test of the call counters (sh2o/count.h), SH2O_COUNT=ON only.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/count.h"
#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/list.h"
#include "sh2o/psyq.h"
#include <stdio.h>
#include <string.h>

/* A fixed call sequence through each route: the 14 per-op entries, the 2 fixed-word entries, DPCT, the prepared
 * MVMVA (its kernel path and its fallback) and its load called directly,
 * sh2o_gte_command (two modelled ops and two ops with no entry), two lists (3 items and none), three prepared lists
 * (3 items on the kernel path, 3 on the fallback, none), each of the 19 psyq bodies, sh2o_gte_write to each
 * write group. After each stage every counter must equal its expected value (all others 0). Then every name, the
 * reset to all-zero, and get/name out of range. rc: 0 when all pass, else 0x10 + the first stage that fails (1 direct,
 * 2 command, 3 list, 4 psyq, 5 writes, 6 names, 7 reset, 8 range).
 * SH2O_COUNT_PLANT=1 (CMakeLists.txt count_plant): the library does not count list items; the test passes only when
 * rc is the pin 0x13 (the list stage catches it). */
#ifndef SH2O_COUNT_PLANT
#define SH2O_COUNT_PLANT 0
#endif
#define COUNT_PLANT_RC 0x13

static uint32_t want[SH2O_COUNT_N];

/* 0 when every counter equals want; else prints each that differs */
static int check(const char *stage) {
    int bad=0;
    for(unsigned i=0;i<SH2O_COUNT_N;i++) {
        uint32_t got=sh2o_count_get(i);
        if(got!=want[i]) {
            fprintf(stderr,"count: %s: %s is %lu, not %lu\n",stage,sh2o_count_name(i),(unsigned long)got,
                (unsigned long)want[i]);
            bad=1;
        }
    }
    return bad;
}

typedef void (*entry_fn)(sh2o_gte_t *g,uint32_t cmd);

int main(void) {
    static const entry_fn entries[14]={sh2o_gte_mvmva,sh2o_gte_nclip,sh2o_gte_avsz3,sh2o_gte_sqr,sh2o_gte_gpf,
        sh2o_gte_gpl,sh2o_gte_dpcs,sh2o_gte_rtps,sh2o_gte_rtpt,sh2o_gte_ncs,sh2o_gte_nct,sh2o_gte_ncds,
        sh2o_gte_ncdt,sh2o_gte_nccs};
    static const uint32_t ops[14]={0x12,0x06,0x2D,0x28,0x3D,0x3E,0x10,0x01,0x30,0x1E,0x20,0x13,0x16,0x1B};
    static const char *const names[SH2O_COUNT_N-64]={"sh2o_gte_command","sh2o_gte_mvmva","sh2o_gte_nclip",
        "sh2o_gte_avsz3","sh2o_gte_sqr","sh2o_gte_gpf","sh2o_gte_gpl","sh2o_gte_dpcs","sh2o_gte_rtps",
        "sh2o_gte_rtpt","sh2o_gte_ncs","sh2o_gte_nct","sh2o_gte_ncds","sh2o_gte_ncdt","sh2o_gte_nccs",
        "sh2o_gte_nrtps","sh2o_gte_nrtpt","sh2o_gte_dpct","sh2o_gte_mvmva_prepared","sh2o_gte_prepared_load","sh2o_psyq_SetRotMatrix","sh2o_psyq_SetLightMatrix","sh2o_psyq_SetColorMatrix","sh2o_psyq_SetTransMatrix",
        "sh2o_psyq_SetBackColor","sh2o_psyq_SetFarColor","sh2o_psyq_SetGeomOffset","sh2o_psyq_SetGeomScreen",
        "sh2o_psyq_InitGeom","sh2o_psyq_SetFogNearFar","sh2o_psyq_TransMatrix","sh2o_psyq_TransposeMatrix",
        "sh2o_psyq_CompMatrix","sh2o_psyq_ApplyMatrix","sh2o_psyq_ApplyMatrixLV","sh2o_psyq_RotTrans",
        "sh2o_psyq_PushMatrix","sh2o_psyq_PopMatrix","sh2o_psyq_ratan2","sh2o_gte_mvmva_list","list_items","sh2o_gte_mvmva_list_prepared",
        "write_rt","write_llm","write_lcm","write_tr","write_bk","write_fc","write_other"};
    static sh2o_gte_t g;
    static sh2o_gte_prepared_t prep;
    static sh2o_psyq_mstack_t stack;
    sh2o_psyq_matrix_t m0,m1,m2;
    sh2o_psyq_vector_t v,v1;
    sh2o_psyq_svector_t sv;
    uint32_t items[3][2]={{1,2},{3,4},{5,6}},out[3][6];
    int32_t flag;
    int rc=0;
    _Static_assert(SH2O_COUNT_GTE_COMMAND==64&&SH2O_COUNT_N==64+49,"count.h layout");
    memset(&m0,0,sizeof m0);
    m0.m[0][0]=m0.m[1][1]=m0.m[2][2]=4096;
    m1=m0;
    memset(&v,0,sizeof v);
    memset(&sv,0,sizeof sv);
    sh2o_count_reset();

    /* 1: each per-op entry directly: its op and its name */
    for(unsigned k=0;k<14;k++) {
        entries[k](&g,ops[k]);
        want[SH2O_COUNT_OP+ops[k]]++;
        want[SH2O_COUNT_GTE_MVMVA+k]++;
    }
    /* the fixed-word entries: their own name and op, not the name sh2o_gte_rtps or sh2o_gte_rtpt */
    sh2o_gte_nrtps(&g);
    sh2o_gte_nrtpt(&g);
    want[SH2O_COUNT_OP+0x01]++;
    want[SH2O_COUNT_OP+0x30]++;
    want[SH2O_COUNT_GTE_NRTPS]++;
    want[SH2O_COUNT_GTE_NRTPT]++;
    sh2o_gte_dpct(&g,0x2A);
    want[SH2O_COUNT_OP+0x2A]++;
    want[SH2O_COUNT_GTE_DPCT]++;
    /* the prepared MVMVA: its name and op 12h on the kernel path and on its fallback (not the name sh2o_gte_mvmva);
     * the load: its name */
    sh2o_gte_prepared_load(&prep,&g,SH2O_GTE_PREPARED_ALL);
    if(sh2o_gte_mvmva_prepared(&g,&prep,0x0080012u)!=1||sh2o_gte_mvmva_prepared(&g,&prep,0x0000012u)!=0)
        rc=0x11;
    want[SH2O_COUNT_GTE_PREPARED_LOAD]++;
    want[SH2O_COUNT_GTE_MVMVA_PREPARED]+=2;
    want[SH2O_COUNT_OP+0x12]+=2;
    if(!rc&&check("direct"))
        rc=0x11;

    /* 2: sh2o_gte_command: its name and the op, never the entry's name; -1 ops counted too */
    if(!rc&&(sh2o_gte_command(&g,0x0486012u)!=0||sh2o_gte_command(&g,0x06)!=0||sh2o_gte_command(&g,0x00)!=-1||
        sh2o_gte_command(&g,0x3F)!=-1))
        rc=0x12;
    want[SH2O_COUNT_GTE_COMMAND]+=4;
    want[SH2O_COUNT_OP+0x12]++;
    want[SH2O_COUNT_OP+0x06]++;
    want[SH2O_COUNT_OP+0x00]++;
    want[SH2O_COUNT_OP+0x3F]++;
    if(!rc&&check("command"))
        rc=0x12;

    /* 3: lists: calls, items, and op 12h per item (not the name sh2o_gte_mvmva) */
    sh2o_gte_mvmva_list(&g,0x0486012u,(const uint32_t (*)[2])items,3,out);
    sh2o_gte_mvmva_list(&g,0x0486012u,(const uint32_t (*)[2])items,0,out);
    want[SH2O_COUNT_LIST_CALLS]+=2;
    want[SH2O_COUNT_LIST_ITEMS]+=3;
    want[SH2O_COUNT_OP+0x12]+=3;
    /* the prepared lists: their name, n items and op 12h per item on the kernel path (sf 1) and on the fallback
     * (sf 0); not the name sh2o_gte_mvmva_list nor sh2o_gte_prepared_load */
    sh2o_gte_mvmva_list_prepared(&g,0x0080012u,(const uint32_t (*)[2])items,3,out);
    sh2o_gte_mvmva_list_prepared(&g,0x0000012u,(const uint32_t (*)[2])items,3,out);
    sh2o_gte_mvmva_list_prepared(&g,0x0080012u,(const uint32_t (*)[2])items,0,out);
    want[SH2O_COUNT_LIST_PREPARED_CALLS]+=3;
    want[SH2O_COUNT_LIST_ITEMS]+=6;
    want[SH2O_COUNT_OP+0x12]+=6;
    if(!rc&&check("list"))
        rc=0x13;

    /* 4: each psyq body: its name, its writes and its commands' ops (not sh2o_gte_command) */
    sh2o_psyq_SetRotMatrix(&g,&m0); /* RT stored in place, counted as writes */
    want[SH2O_COUNT_WRITE_RT]+=5;
    sh2o_psyq_SetLightMatrix(&g,&m0);
    want[SH2O_COUNT_WRITE_LLM]+=5;
    sh2o_psyq_SetColorMatrix(&g,&m0);
    want[SH2O_COUNT_WRITE_LCM]+=5;
    sh2o_psyq_SetTransMatrix(&g,&m0);
    want[SH2O_COUNT_WRITE_TR]+=3;
    sh2o_psyq_SetBackColor(&g,1,2,3);
    want[SH2O_COUNT_WRITE_BK]+=3;
    sh2o_psyq_SetFarColor(&g,1,2,3);
    want[SH2O_COUNT_WRITE_FC]+=3;
    sh2o_psyq_SetGeomOffset(&g,160,120);
    want[SH2O_COUNT_WRITE_OTHER]+=2;
    sh2o_psyq_SetGeomScreen(&g,512);
    want[SH2O_COUNT_WRITE_OTHER]+=1;
    sh2o_psyq_InitGeom(&g);
    want[SH2O_COUNT_WRITE_OTHER]+=7;
    sh2o_psyq_SetFogNearFar(&g,1000,2000,1000); /* DQA, DQB */
    want[SH2O_COUNT_WRITE_OTHER]+=2;
    (void)sh2o_psyq_TransMatrix(&g,&m2,&v);
    (void)sh2o_psyq_TransposeMatrix(&g,&m0,&m2);
    (void)sh2o_psyq_CompMatrix(&g,&m0,&m1,&m2); /* RT, 4 x (VXY0, VZ0, MVMVA) */
    want[SH2O_COUNT_WRITE_RT]+=5;
    want[SH2O_COUNT_WRITE_OTHER]+=8;
    want[SH2O_COUNT_OP+0x12]+=4;
    (void)sh2o_psyq_ApplyMatrix(&g,&m0,&sv,&v1); /* RT and V0 stored in place, MVMVA */
    want[SH2O_COUNT_WRITE_RT]+=5;
    want[SH2O_COUNT_WRITE_OTHER]+=2;
    want[SH2O_COUNT_OP+0x12]+=1;
    (void)sh2o_psyq_ApplyMatrixLV(&g,&m0,&v,&v1); /* RT, 2 x (IR1-3, MVMVA) */
    want[SH2O_COUNT_WRITE_RT]+=5;
    want[SH2O_COUNT_WRITE_OTHER]+=6;
    want[SH2O_COUNT_OP+0x12]+=2;
    sh2o_psyq_RotTrans(&g,&sv,&v1,&flag); /* VXY0, VZ0, MVMVA */
    want[SH2O_COUNT_WRITE_OTHER]+=2;
    want[SH2O_COUNT_OP+0x12]+=1;
    if(!rc&&(sh2o_psyq_PushMatrix(&g,&stack)!=0||sh2o_psyq_PopMatrix(&g,&stack)!=0)) /* pop: RT, TR */
        rc=0x14;
    want[SH2O_COUNT_WRITE_RT]+=5;
    want[SH2O_COUNT_WRITE_TR]+=3;
    (void)sh2o_psyq_ratan2(&g,1,1);
    for(unsigned i=SH2O_COUNT_PSYQ_SETROTMATRIX;i<=SH2O_COUNT_PSYQ_RATAN2;i++)
        want[i]++;
    if(!rc&&check("psyq"))
        rc=0x14;

    /* 5: sh2o_gte_write to each group (any register outside the six, 64 too), then a command from outside again */
    static const unsigned regs[9]={32,36,40,48,37,45,53,0,64};
    static const unsigned groups[9]={SH2O_COUNT_WRITE_RT,SH2O_COUNT_WRITE_RT,SH2O_COUNT_WRITE_LLM,
        SH2O_COUNT_WRITE_LCM,SH2O_COUNT_WRITE_TR,SH2O_COUNT_WRITE_BK,SH2O_COUNT_WRITE_FC,SH2O_COUNT_WRITE_OTHER,
        SH2O_COUNT_WRITE_OTHER};
    for(unsigned k=0;k<9;k++) {
        sh2o_gte_write(&g,regs[k],0);
        want[groups[k]]++;
    }
    (void)sh2o_gte_command(&g,0x12);
    want[SH2O_COUNT_GTE_COMMAND]++;
    want[SH2O_COUNT_OP+0x12]++;
    if(!rc&&check("writes"))
        rc=0x15;

    /* 6: every name */
    for(unsigned i=0;i<SH2O_COUNT_N&&!rc;i++) {
        char op[8];
        const char *name=sh2o_count_name(i);
        const char *expect=names[i<64?0:i-64];
        if(i<64) {
            snprintf(op,sizeof op,"op_%02x",i);
            expect=op;
        }
        if(!name||strcmp(name,expect)!=0) {
            fprintf(stderr,"count: name %u is %s, not %s\n",i,name?name:"NULL",expect);
            rc=0x16;
        }
    }

    /* 7: reset */
    sh2o_count_reset();
    memset(want,0,sizeof want);
    if(!rc&&check("reset"))
        rc=0x17;

    /* 8: out of range */
    if(!rc&&(sh2o_count_get(SH2O_COUNT_N)!=0||sh2o_count_get(~0u)!=0||sh2o_count_name(SH2O_COUNT_N)||
        sh2o_count_name(~0u)))
        rc=0x18;

    printf("count: %u counters, rc 0x%X\n",(unsigned)SH2O_COUNT_N,(unsigned)rc);
    if(SH2O_COUNT_PLANT) {
        if(rc!=COUNT_PLANT_RC) {
            fprintf(stderr,"count: FAIL: the plant gives rc 0x%X, not 0x%X\n",(unsigned)rc,COUNT_PLANT_RC);
            return 1;
        }
        printf("count: plant caught, rc 0x%X\n",(unsigned)rc);
        return 0;
    }
    return rc;
}
