/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the zam family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: zam_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant-digest | --plant-known k]
 * Each ELF holds the library and SH2ZAM built with SH2O_ZAM=1,
 * benchmarks/zam-model/bridge.c and tests/suite/sh2o_zam_test_suite.c. The
 * zam family has no oracle file: the suite's pins are the checks. For each
 * ELF:
 * 1. The suite over all SH2O_ZAM_ITEMS items is computed in the ELF: one
 *    _sh2o_zam_test_seed call, then a chain of _sh2o_zam_test_digest_part
 *    calls over consecutive ranges of each set, ZAM_CHUNK items each, so that
 *    no call reaches the 100,000,000 instruction limit of SH2ZAM's invoke.
 *    rc must be 0; the counts, the digest and the delegate's counters
 *    (sh2o_zam_delegated, sh2o_zam_refused: the mac3 calls) must be the pins
 *    of sh2o_zam_test.h. The harness prints the most instructions of one
 *    call. --plant-digest compares the digest with the pin XOR 1;
 *    --plant-known k flips bit 0 of the expected result (want) of edge row k
 *    of set (C) (the local symbol _sh2o_zam_edges) in the ELF's RAM before
 *    the run. Each shows that its comparison fails. A failure stops the run.
 * 2. The zam-3, zam-4 and zam-5 trials run in the ELF (zam-4 as a chain of
 *    _sh2o_zam_fog_trial_part calls); each count must be its pin and no trial
 *    may have a mismatch. zam-2 (a measurement) does not run.
 * 3. Native only: each suite item runs again with SR.S = 1, from the state
 *    that the SR.S = 0 chain of one-item calls has before that item (the tally
 *    and the delegate's counters). The harness prints how many end states
 *    differ, split by the path of the SR.S = 0 run: delegated (it raised
 *    sh2o_zam_delegated) or core (it did not). A core difference fails the
 *    run; a delegated one is only counted.
 * 4. For each vector of set (A): the ELF writes its image and its three
 *    commands (_sh2o_zam_test_vector_image). Each command runs through
 *    _sh2o_gte_command on a copy with sh2o_zam_bypass 0 (the delegate). When
 *    the delegate took the case, the command runs again on a second copy
 *    with sh2o_zam_bypass 1 (the GTE core's own kernel), and the 64
 *    _sh2o_gte_read words of the two copies must be equal. Native only: the
 *    delegate's call runs again with SR.S = 1 on a third copy; its result and
 *    64 words must be the SR.S = 0 run's.
 * 5. The zam-3 to zam-5 trials' case draws run as a chain in the ELF
 *    (_sh2o_zam_transpose_case, _sh2o_zam_fog_case, _sh2o_zam_list_case, each
 *    from its seed). Each case runs the delegate entry
 *    (_sh2o_zam_TransposeMatrix apart and aliased, _sh2o_zam_SetFogNearFar,
 *    _sh2o_zam_mvmva_list with sh2o_zam_bypass 0) and, when it accepted, the
 *    core entry on the same input (_sh2o_psyq_TransposeMatrix,
 *    _sh2o_psyq_SetFogNearFar, _sh2o_gte_mvmva_list with sh2o_zam_bypass 1);
 *    the outputs must be equal (the return, both matrices; the 64
 *    _sh2o_gte_read words; those and the n out rows) before the cycles count,
 *    and a refused SetFogNearFar must leave the image unchanged. The counts of
 *    the chain must be the trials' pins with 0 mismatches. The list's fifth
 *    argument goes on the stack, at @r15. Native only: each accepted
 *    delegate call runs again with SR.S = 1 from the same input; its result
 *    and outputs must be the SR.S = 0 run's. The harness prints the count of
 *    differing calls per op (with step 4's); one fails the run.
 * The figures are the cycles of one _sh2o_gte_command call (the call that the
 * gte family times) for MVMVA, RTPS and RTPT on the delegated set (A) cases,
 * and of one call of each entry of step 5 on its accepted cases (the list also
 * per item: cycles / n), delegate against core. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/suite/sh2o_zam_test.h"

#define ZAM_IO_TALLY 0u    /* sh2o_zam_tally: nine words */
#define ZAM_IO_IMAGE 64u   /* the image of a set (A) vector */
#define ZAM_IO_A 320u      /* the copy for the delegate */
#define ZAM_IO_B 576u      /* the copy for the core */
#define ZAM_IO_CMD 832u    /* cmd[3] */
#define ZAM_IO_TRIAL 848u  /* sh2o_zam_trial: five words */
#define ZAM_IO_LTRIAL 872u /* sh2o_zam_ltrial: seven words */
#define ZAM_IO_FOG_X 904u  /* zam-4's generator */
#define ZAM_IO_TR_SRC 1024u  /* step 5: zam-3's source matrix */
#define ZAM_IO_TR_M1 1056u   /* zam-3's m1 */
#define ZAM_IO_TR_A0 1088u   /* the delegate's m0 and m1 */
#define ZAM_IO_TR_A1 1120u
#define ZAM_IO_TR_B0 1152u   /* the core's m0 and m1 */
#define ZAM_IO_TR_B1 1184u
#define ZAM_IO_ABH 1216u     /* zam-4's (a, b, h) */
#define ZAM_IO_TR_X 1228u    /* the chain's generators: zam-3, zam-4, zam-5 */
#define ZAM_IO_CFOG_X 1232u
#define ZAM_IO_LIST_X 1236u
#define ZAM_IO_LIST_CMD 1240u
#define ZAM_IO_LIST_V 1280u  /* v[32][2] */
#define ZAM_IO_LIST_OA 1536u /* out[32][6] of the delegate */
#define ZAM_IO_LIST_OB 2304u /* out[32][6] of the core */
#define ZAM_MATRIX_WORDS 8u
#define ZAM_ARG5 (RAM_BASE+RAM_SIZE-64) /* @r15 at entry: a fifth argument */
#define ZAM_TALLY_WORDS 9u
#define ZAM_CHUNK 512u     /* suite items per digest call */
#define ZAM_FOG_CHUNK 2048u /* zam-4 items per call */
#define ZAM_EDGE_SIZE 32u  /* one sh2o_zam_edge: m0[3], v[3], tr, want */
#define ZAM_EDGES 11u
#define ZAM_LIMIT UINT64_C(100000000)

static const char *const zam_variants[2]={"native","portable"};
#define ZAM_OPS 7u
#define ZAM_SR_OPS 6u /* the ops of the SR.S 1 counts: zam_ops[0..5] */
static const char *const zam_ops[ZAM_OPS]={"MVMVA","RTPS","RTPT","TransposeMatrix","SetFogNearFar","list call",
    "list item"};
static const char *const zam_paths[2]={"delegate","core"};
#define ZAM_TABLE 20u
static const char *const zam_table_symbols[ZAM_TABLE]={"_sh2o_zam_test_seed","_sh2o_zam_test_digest_part",
    "_sh2o_zam_test_vector_image","_sh2o_zam_transpose_trial","_sh2o_zam_fog_trial_part","_sh2o_zam_list_trial",
    "_sh2o_zam_transpose_case","_sh2o_zam_fog_case","_sh2o_zam_list_case","_sh2o_zam_TransposeMatrix",
    "_sh2o_psyq_TransposeMatrix","_sh2o_zam_SetFogNearFar","_sh2o_psyq_SetFogNearFar","_sh2o_zam_mvmva_list",
    "_sh2o_gte_mvmva_list","_sh2o_gte_command","_sh2o_gte_read","_sh2o_zam_bypass","_sh2o_zam_delegated",
    "_sh2o_zam_refused"};

typedef struct {
    double min_cycles,max_cycles,min_instructions,max_instructions,sum_cycles,sum_instructions;
    uint64_t count;
} zam_stats_t;

static zam_stats_t zam_stats[2][ZAM_OPS][2]; /* [variant][zam_ops][delegate, core] */
static unsigned zam_sr_calls[ZAM_SR_OPS],zam_sr_differ[ZAM_SR_OPS],zam_sr_refused[ZAM_SR_OPS]; /* native, SR.S 1 */
static u32 zam_io,zam_bypass,zam_delegated,zam_refused;

static void zam_add(zam_stats_t *s,double cycles,double instructions) {
    if(s->count==0||cycles<s->min_cycles)
        s->min_cycles=cycles;
    if(s->count==0||cycles>s->max_cycles)
        s->max_cycles=cycles;
    if(s->count==0||instructions<s->min_instructions)
        s->min_instructions=instructions;
    if(s->count==0||instructions>s->max_instructions)
        s->max_instructions=instructions;
    s->sum_cycles+=cycles;
    s->sum_instructions+=instructions;
    s->count++;
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/zam-model/bridge.c) and
 * plants the known answer when asked. */
static void zam_load(const char *elf,const char *symbols_path,long plant_known) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    zam_io=sym("_sh2o_model_zam_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<ZAM_TABLE;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(zam_table_symbols[entry])) {
            fprintf(stderr,"zam: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),zam_table_symbols[entry],sym(zam_table_symbols[entry]));
            exit(2);
        }
    }
    zam_bypass=sym("_sh2o_zam_bypass");
    zam_delegated=sym("_sh2o_zam_delegated");
    zam_refused=sym("_sh2o_zam_refused");
    if(plant_known>=0) {
        u32 want=sym("_sh2o_zam_edges")+ZAM_EDGE_SIZE*(u32)plant_known+28u;
        wl(&cpu,want,rl(&cpu,want)^1u);
    }
}

/* The suite as a chain of _part calls over each set. Returns 1 when a check fails. */
static int zam_digest(unsigned variant,int plant_digest) {
    static const u32 set_first[4]={0,SH2O_VEC_COUNT,SH2O_VEC_COUNT+SH2O_ZAM_SEEDED,SH2O_ZAM_ITEMS};
    u32 t=zam_io+ZAM_IO_TALLY,want=SH2O_ZAM_PIN_DIGEST^(plant_digest?1u:0u);
    uint64_t most=0,instructions=0,cycles=0;
    int bad=0;
    invoke("_sh2o_zam_test_seed",t,0,0,0);
    for(unsigned set=0;set<3;set++) {
        for(u32 first=set_first[set];first<set_first[set+1];first+=ZAM_CHUNK) {
            u32 count=set_first[set+1]-first<ZAM_CHUNK?set_first[set+1]-first:ZAM_CHUNK;
            struct cost c=invoke("_sh2o_zam_test_digest_part",first,count,t,0);
            instructions+=c.instructions;
            cycles+=c.cycles;
            if(c.instructions>most)
                most=c.instructions;
        }
    }
    u32 cases=rl(&cpu,t),delegated=rl(&cpu,t+4),refused=rl(&cpu,t+8),h=rl(&cpu,t+12),rc=rl(&cpu,t+16);
    u32 d3=rl(&cpu,zam_delegated),r3=rl(&cpu,zam_refused);
    printf("zam: %s: %u cases, %u delegated, %u refused (mac3 calls: %u delegated, %u refused), digest %08x want "
        "%08x, rc %x  %10" PRIu64 " instructions %10" PRIu64 " cycles\n",zam_variants[variant],cases,delegated,refused,
        d3,r3,h,want,rc,instructions,cycles);
    printf("zam: %s: most instructions of one _part call (%u items): %" PRIu64 "\n",zam_variants[variant],ZAM_CHUNK,
        most);
    if(rc!=0) {
        if(rc>>12==3u&&(rc&0xF00u)!=0xF00u)
            fprintf(stderr,"zam: %s: rc %x: the known answer of edge row %u does not hold\n",zam_variants[variant],rc,
                (rc>>4)&0xFFu);
        else
            fprintf(stderr,"zam: %s: rc %x: a check of the suite fails\n",zam_variants[variant],rc);
        bad=1;
    }
    if(most>=ZAM_LIMIT) {
        fprintf(stderr,"zam: %s: a _part call is at the instruction limit\n",zam_variants[variant]);
        bad=1;
    }
    if(h!=want) {
        fprintf(stderr,"zam: %s: the digest is not the pin\n",zam_variants[variant]);
        bad=1;
    }
    if(cases!=SH2O_ZAM_PIN_CASES||delegated!=SH2O_ZAM_PIN_DELEGATED||refused!=SH2O_ZAM_PIN_REFUSED||
        d3!=SH2O_ZAM_PIN_MAC3_DELEGATED||r3!=SH2O_ZAM_PIN_MAC3_REFUSED) {
        fprintf(stderr,"zam: %s: the counts are not the pins (%u %u %u, mac3 %u %u)\n",zam_variants[variant],
            SH2O_ZAM_PIN_CASES,SH2O_ZAM_PIN_DELEGATED,SH2O_ZAM_PIN_REFUSED,SH2O_ZAM_PIN_MAC3_DELEGATED,
            SH2O_ZAM_PIN_MAC3_REFUSED);
        bad=1;
    }
    return bad;
}

static void zam_zero(u32 address,unsigned words) {
    for(unsigned k=0;k<words;k++)
        wl(&cpu,address+4*k,0);
}

/* The zam-3 to zam-5 trials in the ELF. Returns 1 when a check fails. */
static int zam_trials(unsigned variant) {
    u32 tr=zam_io+ZAM_IO_TRIAL,lt=zam_io+ZAM_IO_LTRIAL,x=zam_io+ZAM_IO_FOG_X;
    uint64_t most=0;
    int bad=0;
    zam_zero(tr,5);
    struct cost c=invoke("_sh2o_zam_transpose_trial",tr,0,0,0);
    u32 cases=rl(&cpu,tr),delegated=rl(&cpu,tr+4),refused=rl(&cpu,tr+8),mismatches=rl(&cpu,tr+12);
    printf("zam: %s: zam-3 TransposeMatrix: %u cases, %u delegated, %u mismatches  %10" PRIu64 " instructions\n",
        zam_variants[variant],cases,delegated,mismatches,c.instructions);
    if(cases!=SH2O_ZAM_PIN_TR_CASES||delegated!=SH2O_ZAM_PIN_TR_DELEGATED||refused!=0||mismatches!=0) {
        fprintf(stderr,"zam: %s: zam-3: the counts are not the pins\n",zam_variants[variant]);
        bad=1;
    }
    zam_zero(tr,5);
    wl(&cpu,x,SH2O_ZAM_FOG_SEED);
    for(u32 first=0;first<SH2O_ZAM_FOG_ITEMS;first+=ZAM_FOG_CHUNK) {
        u32 count=SH2O_ZAM_FOG_ITEMS-first<ZAM_FOG_CHUNK?SH2O_ZAM_FOG_ITEMS-first:ZAM_FOG_CHUNK;
        c=invoke("_sh2o_zam_fog_trial_part",first,count,tr,x);
        if(c.instructions>most)
            most=c.instructions;
    }
    cases=rl(&cpu,tr),delegated=rl(&cpu,tr+4),refused=rl(&cpu,tr+8),mismatches=rl(&cpu,tr+12);
    printf("zam: %s: zam-4 SetFogNearFar: %u cases, %u delegated, %u refused, %u mismatches; most instructions of "
        "one _part call (%u items): %" PRIu64 "\n",zam_variants[variant],cases,delegated,refused,mismatches,
        ZAM_FOG_CHUNK,most);
    if(cases!=SH2O_ZAM_PIN_FOG_CASES||delegated!=SH2O_ZAM_PIN_FOG_DELEGATED||refused!=SH2O_ZAM_PIN_FOG_REFUSED||
        mismatches!=0) {
        fprintf(stderr,"zam: %s: zam-4: the counts are not the pins\n",zam_variants[variant]);
        bad=1;
    }
    zam_zero(lt,7);
    c=invoke("_sh2o_zam_list_trial",lt,0,0,0);
    u32 l[7];
    for(unsigned k=0;k<7;k++)
        l[k]=rl(&cpu,lt+4*k);
    printf("zam: %s: zam-5 mvmva_list: %u runs, %u items, %u delegated, %u refused, %u mismatches, planted caught %u "
        "of %u  %10" PRIu64 " instructions\n",zam_variants[variant],l[0],l[1],l[2],l[3],l[4],l[6],l[5],
        c.instructions);
    if(l[0]!=SH2O_ZAM_PIN_LIST_RUNS||l[1]!=SH2O_ZAM_PIN_LIST_ITEMS||l[2]!=SH2O_ZAM_PIN_LIST_DELEGATED||
        l[3]!=SH2O_ZAM_PIN_LIST_REFUSED||l[4]!=0||l[5]!=SH2O_ZAM_PIN_LIST_PLANTED||l[6]!=SH2O_ZAM_PIN_LIST_CAUGHT) {
        fprintf(stderr,"zam: %s: zam-5: the counts are not the pins\n",zam_variants[variant]);
        bad=1;
    }
    return bad;
}

/* The state that one suite item changes: the tally, the delegate's counters and its bypass switch. */
static void zam_state_get(u32 s[ZAM_TALLY_WORDS+3]) {
    u32 t=zam_io+ZAM_IO_TALLY;
    for(unsigned k=0;k<ZAM_TALLY_WORDS;k++)
        s[k]=rl(&cpu,t+4*k);
    s[ZAM_TALLY_WORDS]=rl(&cpu,zam_delegated);
    s[ZAM_TALLY_WORDS+1]=rl(&cpu,zam_refused);
    s[ZAM_TALLY_WORDS+2]=rl(&cpu,zam_bypass);
}

static void zam_state_put(const u32 s[ZAM_TALLY_WORDS+3]) {
    u32 t=zam_io+ZAM_IO_TALLY;
    for(unsigned k=0;k<ZAM_TALLY_WORDS;k++)
        wl(&cpu,t+4*k,s[k]);
    wl(&cpu,zam_delegated,s[ZAM_TALLY_WORDS]);
    wl(&cpu,zam_refused,s[ZAM_TALLY_WORDS+1]);
    wl(&cpu,zam_bypass,s[ZAM_TALLY_WORDS+2]);
}

/* SR.S = 1 in the native ELF. The GTE core checks SR.S, so an item whose
 * SR.S = 0 run did not delegate must end in the same state; the delegated
 * items are only counted. Returns 1 on a core difference, or when the SR.S = 0
 * chain of one-item calls does not give the pin (a fault of this check, not of
 * SR.S). */
static int zam_sr_s(void) {
    u32 t=zam_io+ZAM_IO_TALLY,start[ZAM_TALLY_WORDS+3],end0[ZAM_TALLY_WORDS+3],end1[ZAM_TALLY_WORDS+3];
    unsigned differ[2]={0,0},items[2]={0,0}; /* [0: core, 1: delegated] */
    invoke("_sh2o_zam_test_seed",t,0,0,0);
    for(u32 i=0;i<SH2O_ZAM_ITEMS;i++) {
        zam_state_get(start);
        invoke("_sh2o_zam_test_digest_part",i,1,t,0);
        zam_state_get(end0);
        zam_state_put(start);
        invoke_sr("_sh2o_zam_test_digest_part",i,1,t,0,2);
        zam_state_get(end1);
        unsigned path=end0[ZAM_TALLY_WORDS]!=start[ZAM_TALLY_WORDS];
        items[path]++;
        if(memcmp(end0,end1,sizeof end0))
            differ[path]++;
        zam_state_put(end0);
    }
    if(rl(&cpu,t+12)!=SH2O_ZAM_PIN_DIGEST||rl(&cpu,t+16)!=0) {
        fprintf(stderr,"zam: native: the chain of one-item calls gives digest %08x rc %x, not the pin\n",
            rl(&cpu,t+12),rl(&cpu,t+16));
        return 1;
    }
    printf("zam: native: SR.S 1: core: %u of %u items end in another state%s\n",differ[0],items[0],
        differ[0]?"  FAIL":"");
    printf("zam: native: SR.S 1: delegated: %u of %u items end in another state (count only)\n",differ[1],
        items[1]);
    return differ[0]!=0;
}

static void zam_copy(u32 to,u32 from) {
    for(unsigned k=0;k<64;k++)
        wl(&cpu,to+4*k,rl(&cpu,from+4*k));
}

/* The first of the 64 _sh2o_gte_read words where images a and b differ, or -1. */
static int zam_gte_diff(u32 a,u32 b) {
    for(unsigned r=0;r<64;r++)
        if(invoke("_sh2o_gte_read",a,r,0,0).result!=invoke("_sh2o_gte_read",b,r,0,0).result)
            return (int)r;
    return -1;
}

static int zam_same(u32 a,u32 b,unsigned words) {
    for(unsigned k=0;k<words;k++)
        if(rl(&cpu,a+4*k)!=rl(&cpu,b+4*k))
            return 0;
    return 1;
}

static void zam_copy_words(u32 to,u32 from,unsigned words) {
    for(unsigned k=0;k<words;k++)
        wl(&cpu,to+4*k,rl(&cpu,from+4*k));
}

/* Delegate against core on each set (A) command, timed. Returns 1 at the first difference. */
static int zam_pairs(unsigned variant) {
    u32 t=zam_io+ZAM_IO_TALLY,img=zam_io+ZAM_IO_IMAGE,a=zam_io+ZAM_IO_A,b=zam_io+ZAM_IO_B,cmds=zam_io+ZAM_IO_CMD;
    invoke("_sh2o_zam_test_seed",t,0,0,0);
    for(u32 i=0;i<SH2O_VEC_COUNT;i++) {
        invoke("_sh2o_zam_test_vector_image",i,t,img,cmds);
        for(unsigned op=0;op<3;op++) {
            u32 cmd=rl(&cpu,cmds+4*op),d0=rl(&cpu,zam_delegated);
            zam_copy(a,img);
            zam_copy(b,img);
            wl(&cpu,zam_bypass,0);
            struct cost ca=invoke("_sh2o_gte_command",a,cmd,0,0);
            if(rl(&cpu,zam_delegated)==d0)
                continue;
            wl(&cpu,zam_bypass,1);
            struct cost cb=invoke("_sh2o_gte_command",b,cmd,0,0);
            wl(&cpu,zam_bypass,0);
            if(ca.result!=0||cb.result!=0) {
                fprintf(stderr,"zam: %s: vector %u, cmd %08x: the command returns %x (delegate), %x (core)\n",
                    zam_variants[variant],i,cmd,ca.result,cb.result);
                return 1;
            }
            for(unsigned r=0;r<64;r++) {
                u32 wa=invoke("_sh2o_gte_read",a,r,0,0).result,wb=invoke("_sh2o_gte_read",b,r,0,0).result;
                if(wa!=wb) {
                    fprintf(stderr,"zam: %s: vector %u, cmd %08x: word %u is %08x (delegate), %08x (core)\n",
                        zam_variants[variant],i,cmd,r,wa,wb);
                    return 1;
                }
            }
            zam_add(&zam_stats[variant][op][0],(double)ca.cycles,(double)ca.instructions);
            zam_add(&zam_stats[variant][op][1],(double)cb.cycles,(double)cb.instructions);
            if(variant==0) {
                zam_copy(b,img);
                struct cost cs=invoke_sr("_sh2o_gte_command",b,cmd,0,0,2);
                zam_sr_calls[op]++;
                zam_sr_differ[op]+=cs.result!=ca.result||zam_gte_diff(a,b)>=0;
            }
        }
    }
    printf("zam: %s: set (A): %" PRIu64 " MVMVA, %" PRIu64 " RTPS, %" PRIu64 " RTPT delegated cases, delegate and "
        "core equal\n",zam_variants[variant],zam_stats[variant][0][0].count,zam_stats[variant][1][0].count,
        zam_stats[variant][2][0].count);
    return 0;
}

/* Step 5: the trials' cases as a chain of case draws; delegate against core on each accepted case, timed, and
 * (native) the delegate again with SR.S = 1. Returns 1 when a check fails. */
static int zam_calls(unsigned variant) {
    u32 src=zam_io+ZAM_IO_TR_SRC,m1=zam_io+ZAM_IO_TR_M1,a0=zam_io+ZAM_IO_TR_A0,a1=zam_io+ZAM_IO_TR_A1;
    u32 b0=zam_io+ZAM_IO_TR_B0,b1=zam_io+ZAM_IO_TR_B1,img=zam_io+ZAM_IO_IMAGE,a=zam_io+ZAM_IO_A,b=zam_io+ZAM_IO_B;
    u32 abh=zam_io+ZAM_IO_ABH,v=zam_io+ZAM_IO_LIST_V,oa=zam_io+ZAM_IO_LIST_OA,ob=zam_io+ZAM_IO_LIST_OB;
    u32 x=zam_io+ZAM_IO_TR_X,fx=zam_io+ZAM_IO_CFOG_X,lx=zam_io+ZAM_IO_LIST_X,cmds=zam_io+ZAM_IO_LIST_CMD;
    unsigned cases=0,delegated=0,refused=0,mismatches=0;
    int bad=0;
    /* zam-3: per source, apart (m1 the destination) and aliased (m0 the destination) */
    wl(&cpu,x,SH2O_ZAM_TR_SEED);
    for(u32 k=0;k<SH2O_ZAM_TR_SOURCES;k++) {
        invoke("_sh2o_zam_transpose_case",k,x,src,m1);
        for(unsigned alias=0;alias<2;alias++) {
            u32 da=alias?a0:a1,db=alias?b0:b1;
            zam_copy_words(a0,src,ZAM_MATRIX_WORDS);
            zam_copy_words(a1,m1,ZAM_MATRIX_WORDS);
            zam_copy_words(b0,src,ZAM_MATRIX_WORDS);
            zam_copy_words(b1,m1,ZAM_MATRIX_WORDS);
            struct cost ca=invoke("_sh2o_zam_TransposeMatrix",a0,da,0,0);
            struct cost cb=invoke("_sh2o_psyq_TransposeMatrix",0,b0,db,0);
            cases++;
            delegated+=ca.result==da;
            if(ca.result!=da||cb.result!=db||!zam_same(a0,b0,ZAM_MATRIX_WORDS)||!zam_same(a1,b1,ZAM_MATRIX_WORDS)) {
                mismatches++;
                continue;
            }
            zam_add(&zam_stats[variant][3][0],(double)ca.cycles,(double)ca.instructions);
            zam_add(&zam_stats[variant][3][1],(double)cb.cycles,(double)cb.instructions);
            if(variant==0) {
                zam_copy_words(b0,src,ZAM_MATRIX_WORDS);
                zam_copy_words(b1,m1,ZAM_MATRIX_WORDS);
                struct cost cs=invoke_sr("_sh2o_zam_TransposeMatrix",b0,db,0,0,2);
                zam_sr_calls[3]++;
                zam_sr_refused[3]+=cs.result!=db;
                zam_sr_differ[3]+=cs.result!=db||!zam_same(a0,b0,ZAM_MATRIX_WORDS)||
                    !zam_same(a1,b1,ZAM_MATRIX_WORDS);
            }
        }
    }
    printf("zam: %s: zam-3 chain: %u calls, %u delegated, %u mismatches\n",zam_variants[variant],cases,delegated,
        mismatches);
    if(cases!=SH2O_ZAM_PIN_TR_CASES||delegated!=SH2O_ZAM_PIN_TR_DELEGATED||mismatches!=0) {
        fprintf(stderr,"zam: %s: zam-3 chain: the counts are not the pins (%u %u, 0 mismatches)\n",
            zam_variants[variant],SH2O_ZAM_PIN_TR_CASES,SH2O_ZAM_PIN_TR_DELEGATED);
        bad=1;
    }
    /* zam-4 */
    cases=delegated=refused=mismatches=0;
    wl(&cpu,fx,SH2O_ZAM_FOG_SEED);
    for(u32 k=0;k<SH2O_ZAM_FOG_ITEMS;k++) {
        invoke("_sh2o_zam_fog_case",k,fx,img,abh);
        u32 fa=rl(&cpu,abh),fb=rl(&cpu,abh+4),fh=rl(&cpu,abh+8);
        zam_copy(a,img);
        struct cost ca=invoke("_sh2o_zam_SetFogNearFar",a,fa,fb,fh);
        cases++;
        if(ca.result==0) {
            refused++;
            mismatches+=!zam_same(a,img,64); /* a refusal wrote */
            continue;
        }
        delegated++;
        zam_copy(b,img);
        struct cost cb=invoke("_sh2o_psyq_SetFogNearFar",b,fa,fb,fh);
        if(zam_gte_diff(a,b)>=0) {
            mismatches++;
            continue;
        }
        zam_add(&zam_stats[variant][4][0],(double)ca.cycles,(double)ca.instructions);
        zam_add(&zam_stats[variant][4][1],(double)cb.cycles,(double)cb.instructions);
        if(variant==0) {
            zam_copy(b,img);
            struct cost cs=invoke_sr("_sh2o_zam_SetFogNearFar",b,fa,fb,fh,2);
            zam_sr_calls[4]++;
            zam_sr_refused[4]+=cs.result==0;
            zam_sr_differ[4]+=cs.result!=ca.result||zam_gte_diff(a,b)>=0;
        }
    }
    printf("zam: %s: zam-4 chain: %u calls, %u delegated, %u refused, %u mismatches\n",zam_variants[variant],cases,
        delegated,refused,mismatches);
    if(cases!=SH2O_ZAM_PIN_FOG_CASES||delegated!=SH2O_ZAM_PIN_FOG_DELEGATED||refused!=SH2O_ZAM_PIN_FOG_REFUSED||
        mismatches!=0) {
        fprintf(stderr,"zam: %s: zam-4 chain: the counts are not the pins (%u %u %u, 0 mismatches)\n",
            zam_variants[variant],SH2O_ZAM_PIN_FOG_CASES,SH2O_ZAM_PIN_FOG_DELEGATED,SH2O_ZAM_PIN_FOG_REFUSED);
        bad=1;
    }
    /* zam-5: the core is the list with the GTE core's own kernel (bypass 1) */
    cases=delegated=refused=mismatches=0;
    unsigned items=0;
    wl(&cpu,lx,SH2O_ZAM_LIST_SEED);
    for(u32 i=0;i<SH2O_ZAM_PIN_LIST_RUNS;i++) {
        wl(&cpu,ZAM_ARG5,v);
        u32 n=invoke("_sh2o_zam_list_case",i,lx,img,cmds).result,cmd=rl(&cpu,cmds);
        cases++;
        items+=n;
        zam_copy(a,img);
        wl(&cpu,zam_bypass,0);
        wl(&cpu,ZAM_ARG5,oa);
        struct cost ca=invoke("_sh2o_zam_mvmva_list",a,cmd,v,n);
        if(ca.result==0) {
            refused++;
            continue;
        }
        delegated++;
        zam_copy(b,img);
        wl(&cpu,zam_bypass,1);
        wl(&cpu,ZAM_ARG5,ob);
        struct cost cb=invoke("_sh2o_gte_mvmva_list",b,cmd,v,n);
        wl(&cpu,zam_bypass,0);
        if(zam_gte_diff(a,b)>=0||!zam_same(oa,ob,6*n)) {
            mismatches++;
            continue;
        }
        zam_add(&zam_stats[variant][5][0],(double)ca.cycles,(double)ca.instructions);
        zam_add(&zam_stats[variant][5][1],(double)cb.cycles,(double)cb.instructions);
        zam_add(&zam_stats[variant][6][0],(double)ca.cycles/n,(double)ca.instructions/n);
        zam_add(&zam_stats[variant][6][1],(double)cb.cycles/n,(double)cb.instructions/n);
        if(variant==0) {
            zam_copy(b,img);
            zam_zero(ob,6*n);
            wl(&cpu,ZAM_ARG5,ob);
            struct cost cs=invoke_sr("_sh2o_zam_mvmva_list",b,cmd,v,n,2);
            zam_sr_calls[5]++;
            zam_sr_refused[5]+=cs.result==0;
            zam_sr_differ[5]+=cs.result!=ca.result||zam_gte_diff(a,b)>=0||!zam_same(oa,ob,6*n);
        }
    }
    printf("zam: %s: zam-5 chain: %u runs, %u items, %u delegated, %u refused, %u mismatches\n",
        zam_variants[variant],cases,items,delegated,refused,mismatches);
    if(items!=SH2O_ZAM_PIN_LIST_ITEMS||delegated!=SH2O_ZAM_PIN_LIST_DELEGATED||refused!=SH2O_ZAM_PIN_LIST_REFUSED||
        mismatches!=0) {
        fprintf(stderr,"zam: %s: zam-5 chain: the counts are not the pins (%u items, %u %u, 0 mismatches)\n",
            zam_variants[variant],SH2O_ZAM_PIN_LIST_ITEMS,SH2O_ZAM_PIN_LIST_DELEGATED,SH2O_ZAM_PIN_LIST_REFUSED);
        bad=1;
    }
    if(variant==0) {
        unsigned differ=0;
        for(unsigned op=0;op<ZAM_SR_OPS;op++) {
            printf("zam: native: SR.S 1: %s: %u of %u delegate calls differ from SR.S 0 (%u refused)%s\n",zam_ops[op],
                zam_sr_differ[op],zam_sr_calls[op],zam_sr_refused[op],zam_sr_differ[op]?"  FAIL":"");
            differ+=zam_sr_differ[op];
        }
        if(differ)
            bad=1;
    }
    return bad;
}

int main(int argc,char **argv) {
    long plant_known=-1;
    int plant_digest=0;
    if(argc==12&&!strcmp(argv[10],"--plant-known")) {
        char *end;
        plant_known=strtol(argv[11],&end,10);
        if(*end||plant_known<0||plant_known>=(long)ZAM_EDGES) {
            fprintf(stderr,"zam: --plant-known needs an edge row from 0 to %u\n",ZAM_EDGES-1);
            return 2;
        }
    } else if(argc==11&&!strcmp(argv[10],"--plant-digest")) {
        plant_digest=1;
    } else if(argc!=10) {
        fprintf(stderr,"usage: zam_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant-digest | --plant-known k]\n");
        return 2;
    }
    cpu.model=SHMT_SH2;
    cpu.MappedMemoryReadByte=rb;
    cpu.MappedMemoryReadWord=rw;
    cpu.MappedMemoryReadLong=rl;
    cpu.MappedMemoryWriteByte=wb;
    cpu.MappedMemoryWriteWord=ww;
    cpu.MappedMemoryWriteLong=wl;
    for(unsigned i=0;i<65536;i++)
        cpu.opcodes[i]=decode(SHMT_SH2,(u16)i);
    for(unsigned i=0;i<256;i++)
        cpu.fetchlist[i]=fetch;
    for(unsigned variant=0;variant<2;variant++) {
        zam_load(argv[1+2*variant],argv[2+2*variant],plant_known);
        if(zam_digest(variant,plant_digest))
            return 1;
        printf("zam: %s: the suite gives every pin (%s)\n",zam_variants[variant],argv[1+2*variant]);
        if(zam_trials(variant))
            return 1;
        if(variant==0&&zam_sr_s())
            return 1;
        if(zam_pairs(variant))
            return 1;
        if(zam_calls(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. One sh2o_gte_command call on the delegated set (A) cases of the zam suite, with the\n");
    printf("delegate (sh2o_zam_bypass 0) and with the GTE core's own kernel (sh2o_zam_bypass 1); one call of\n");
    printf("TransposeMatrix (sh2o_zam_ against sh2o_psyq_), SetFogNearFar (the same) and the MVMVA list (sh2o_zam_\n");
    printf("against sh2o_gte_mvmva_list with sh2o_zam_bypass 1; list item = cycles / n) on the zam-3 to zam-5 cases the\n");
    printf("delegate accepts.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-15s %-9s %9s %9s %9s %9s %9s %9s %7s\n","variant","op","path","cyc min","cyc max","cyc mean",
        "ins min","ins max","ins mean","count");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned op=0;op<ZAM_OPS;op++) {
            for(unsigned path=0;path<2;path++) {
                const zam_stats_t *s=&zam_stats[variant][op][path];
                if(s->count==0) {
                    printf("%-9s %-15s %-9s no delegated case\n",zam_variants[variant],zam_ops[op],zam_paths[path]);
                    continue;
                }
                printf("%-9s %-15s %-9s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %7" PRIu64 "\n",zam_variants[variant],
                    zam_ops[op],zam_paths[path],s->min_cycles,s->max_cycles,s->sum_cycles/(double)s->count,
                    s->min_instructions,s->max_instructions,s->sum_instructions/(double)s->count,s->count);
            }
        }
    }
    return 0;
}
