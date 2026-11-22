/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the gte family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: gte_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant vector | --plant-digest | --plant-entry]
 * Each ELF holds the library, benchmarks/gte-model/bridge.c and
 * tests/suite/sh2o_gte_test_suite.c. For each ELF:
 * 1. Every vector of tests/gte_oracle.h runs through the library functions
 *    _sh2o_gte_write, _sh2o_gte_command and _sh2o_gte_read, as tests/test_gte.c
 *    does: the 61 words in sh2o_gte_order (read from the ELF) on a zeroed
 *    image in the bridge buffer, then the command (cmd) or one more write
 *    (write), then the 64 reads. The result bytes are compared with the oracle
 *    before any figure is printed. --plant flips one bit of a copy of the
 *    expected result of one vector, to show that the comparison fails.
 * 2. The suite's digest over the 3,840 cases is computed in the ELF, as a
 *    chain of calls of sh2o_gte_test_digest_part, GTE_CHUNK cases each, so
 *    that no call reaches the 100,000,000 instruction limit of SH2ZAM's
 *    invoke. Each call also gives the known answers of its range; all 14 must
 *    hold, and the digest must be the pin of sh2o_gte_test.h. The harness
 *    prints the most instructions of one call. --plant-digest compares the
 *    digest with the pin XOR 1, to show that the comparison fails. A failure
 *    stops the run.
 * 3. Each suite case runs again with SR.S = 1, from the state that the
 *    SR.S = 0 chain has before that case, and each oracle cmd vector runs
 *    again with SR.S = 1. The library checks SR.S, so any difference fails
 *    the run.
 * 4. The fixed-word entries _sh2o_gte_nrtps and _sh2o_gte_nrtpt run on the
 *    input image of each RTPS and RTPT cmd vector; their 64 read words must
 *    equal those of _sh2o_gte_command with their words (0x4A180001,
 *    0x4A280030) on the same image, at SR.S = 0 and at SR.S = 1, and the
 *    vector's result where its cmd has sf 1 and lm 0. --plant-entry compares
 *    with a wrong word (sf 0: 0x4A100001, lm 1: 0x4A280430), to show that the
 *    comparison fails. The entry _sh2o_gte_dpct takes the cmd word: it runs
 *    with the word of each DPCT cmd vector and must equal _sh2o_gte_command
 *    with that word and the vector's result, at SR.S = 0 and at SR.S = 1;
 *    --plant-entry compares with the word's sf flipped. Any difference stops
 *    the run.
 * 5. The MVMVA kernel path, at SR.S = 0: each oracle cmd vector with cmd
 *    12h, sf 1, mx < 3, cv != 2 and each T inside the range test of
 *    sh2oi_mac3 (src/sh2o_gte.c). One _sh2o_gte_mvmva call (whole) on its
 *    input image: the 64 reads must equal the vector's result. Then one
 *    _sh2o_model_gte_mac3 call (kernel: sh2o_k_mvb3) on the same matrix
 *    words (the image from R base), V and T: each out[i] must equal
 *    (int64)T[i] << 12 + M[i].V, computed here from the image words, and
 *    out[i] >> 12 must equal the vector's MAC(i+1). Any difference stops
 *    the run.
 * The figures are the cycles of one _sh2o_gte_command call, for each opcode,
 * of one entry call (rows nRTPS, nRTPT, eDPCT) over the vectors of step 4,
 * and of the whole and kernel calls of step 5. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/gte_oracle.h"
#include "../../tests/suite/sh2o_gte_test.h"

#define GTE_IO_IMAGE 0u    /* one sh2o_gte_t: 64 words */
#define GTE_IO_DIGEST 256u /* the digest state s[2] */
#define GTE_IO_V16 264u    /* step 5: the kernel's v[3] (int16) */
#define GTE_IO_T 272u      /* step 5: its t[3] */
#define GTE_IO_OUT 288u    /* step 5: its out[3] (int64) */
#define GTE_CHUNK 256u     /* cases per digest call */
#define GTE_LIMIT UINT64_C(100000000)

static const char *const gte_variants[2]={"native","portable"};
static const char *const gte_table_symbols[9]={"_sh2o_gte_test_digest_part","_sh2o_gte_write","_sh2o_gte_command",
    "_sh2o_gte_read","_sh2o_gte_order","_sh2o_gte_nrtps","_sh2o_gte_nrtpt","_sh2o_gte_dpct","_sh2o_model_gte_mac3"};
/* The entries: [0] nRTPS (RTPS, cmd 01h), [1] nRTPT (RTPT, cmd 30h), fixed words; [2] eDPCT (DPCT, cmd 2Ah), which
 * takes the cmd word (no entry in the word tables: it runs with the vector's word, planted with sf flipped). */
static const char *const gte_entry_names[3]={"nRTPS","nRTPT","eDPCT"};
static const char *const gte_entry_symbols[3]={"_sh2o_gte_nrtps","_sh2o_gte_nrtpt","_sh2o_gte_dpct"};
static const u32 gte_entry_words[2]={UINT32_C(0x4A180001),UINT32_C(0x4A280030)};
static const u32 gte_entry_planted[2]={UINT32_C(0x4A100001),UINT32_C(0x4A280430)}; /* sf 0, lm 1 */

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t min_instructions,max_instructions,sum_cycles,sum_instructions,count;
} gte_stats_t;

static gte_stats_t gte_stats[2][64]; /* cycles of _sh2o_gte_command, by opcode (cmd bits 0-5) */
static gte_stats_t gte_entry_stats[2][3]; /* cycles of one entry call: [variant][0 nRTPS, 1 nRTPT, 2 eDPCT] */
static gte_stats_t gte_mac3_stats[2][2]; /* step 5: [variant][0 whole _sh2o_gte_mvmva, 1 kernel _sh2o_model_gte_mac3] */
static u32 gte_io;
static unsigned gte_order[SH2O_GTE_ORDER_COUNT];

static u32 gte_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static void gte_put_u32(uint8_t *bytes,u32 value) {
    for(unsigned byte=0;byte<4;byte++)
        bytes[byte]=(uint8_t)(value>>(8*byte));
}

static const char *gte_op_name(unsigned op) {
    switch(op) {
    case 0x01: return "RTPS";
    case 0x06: return "NCLIP";
    case 0x0C: return "OP (not modelled)";
    case 0x10: return "DPCS";
    case 0x12: return "MVMVA";
    case 0x13: return "NCDS";
    case 0x16: return "NCDT";
    case 0x1B: return "NCCS";
    case 0x1E: return "NCS";
    case 0x20: return "NCT";
    case 0x28: return "SQR";
    case 0x2A: return "DPCT";
    case 0x2D: return "AVSZ3";
    case 0x30: return "RTPT";
    case 0x3D: return "GPF";
    case 0x3E: return "GPL";
    default: return "-";
    }
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/gte-model/bridge.c) and
 * reads the write order from the ELF. */
static void gte_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    gte_io=sym("_sh2o_model_gte_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<9;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(gte_table_symbols[entry])) {
            fprintf(stderr,"gte: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),gte_table_symbols[entry],sym(gte_table_symbols[entry]));
            exit(2);
        }
    }
    u32 order=sym("_sh2o_gte_order");
    for(unsigned k=0;k<SH2O_GTE_ORDER_COUNT;k++) {
        gte_order[k]=rb(&cpu,order+k);
        if(gte_order[k]>63) {
            fprintf(stderr,"gte: %s: sh2o_gte_order[%u] is %u\n",elf,k,gte_order[k]);
            exit(2);
        }
    }
}

/* Loads a vector's input image: the 61 words in sh2o_gte_order on a zeroed image, with SR = sr. */
static void gte_image(const uint8_t *in,u32 sr) {
    u32 g=gte_io+GTE_IO_IMAGE;
    for(unsigned r=0;r<64;r++)
        wl(&cpu,g+4*r,0);
    for(unsigned k=0;k<SH2O_GTE_ORDER_COUNT;k++)
        invoke_sr("_sh2o_gte_write",g,gte_order[k],gte_get_u32(in+4*gte_order[k]),0,sr);
}

static void gte_stats_add(gte_stats_t *s,const struct cost *c) {
    if(s->count==0||c->cycles<s->min_cycles)
        s->min_cycles=c->cycles;
    if(s->count==0||c->cycles>s->max_cycles)
        s->max_cycles=c->cycles;
    if(s->count==0||c->instructions<s->min_instructions)
        s->min_instructions=c->instructions;
    if(s->count==0||c->instructions>s->max_instructions)
        s->max_instructions=c->instructions;
    s->sum_cycles+=c->cycles;
    s->sum_instructions+=c->instructions;
    s->count++;
}

/* Runs one vector with SR = sr and gives its result as the oracle's bytes. *cost: the command call (cmd only). */
static void gte_run(const gte_vector_t *vector,uint8_t *out,u32 sr,struct cost *cost) {
    const uint8_t *in=vector->in;
    u32 g=gte_io+GTE_IO_IMAGE;
    gte_image(in,sr);
    switch(vector->op) {
    case GTE_OP_CMD:
        *cost=invoke_sr("_sh2o_gte_command",g,gte_get_u32(in+256),0,0,sr);
        gte_put_u32(out,cost->result);
        for(unsigned r=0;r<64;r++)
            gte_put_u32(out+4+4*r,invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result);
        break;
    case GTE_OP_WRITE: {
        u32 reg=gte_get_u32(in+256);
        invoke_sr("_sh2o_gte_write",g,reg,gte_get_u32(in+260),0,sr);
        gte_put_u32(out+256,invoke_sr("_sh2o_gte_read",g,reg,0,0,sr).result);
        for(unsigned r=0;r<64;r++)
            gte_put_u32(out+4*r,invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result);
        break;
    }
    default:
        fprintf(stderr,"gte: vector has unknown op %u\n",vector->op);
        exit(2);
    }
}

/* Runs every vector on one ELF. Returns 1 at the first difference. */
static int gte_verify(unsigned variant,long plant,size_t *cmds) {
    *cmds=0;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        uint8_t expected[GTE_OUT_MAX],out[GTE_OUT_MAX];
        struct cost c;
        memcpy(expected,vector->out,sizeof(expected));
        if(plant>=0&&(size_t)plant==i)
            expected[0]^=1;
        gte_run(vector,out,0,&c);
        if(vector->op==GTE_OP_CMD) {
            gte_stats_add(&gte_stats[variant][gte_get_u32(vector->in+256)&0x3Fu],&c);
            (*cmds)++;
        }
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            if(out[byte]==expected[byte])
                continue;
            fprintf(stderr,"gte: %s: first difference in vector %zu (%s, word %08x), result byte %u: "
                "expected %02x, got %02x\n",gte_variants[variant],i,gte_op_names[vector->op],
                gte_get_u32(vector->in+256),byte,expected[byte],out[byte]);
            return 1;
        }
    }
    return 0;
}

/* The suite's digest as a chain of _part calls, and its known answers. Returns 1 when a check fails. */
static int gte_digest(unsigned variant,int plant_digest) {
    u32 s=gte_io+GTE_IO_DIGEST,want=SH2O_DIGEST_GTE^(plant_digest?1u:0u);
    uint64_t most=0,instructions=0,cycles=0;
    int bad=0;
    wl(&cpu,s,SH2O_GTE_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    for(u32 first=0;first<SH2O_GTE_CASES;first+=GTE_CHUNK) {
        u32 count=SH2O_GTE_CASES-first<GTE_CHUNK?SH2O_GTE_CASES-first:GTE_CHUNK;
        struct cost c=invoke("_sh2o_gte_test_digest_part",first,count,s,0);
        instructions+=c.instructions;
        cycles+=c.cycles;
        if(c.instructions>most)
            most=c.instructions;
        if(c.result!=0) {
            fprintf(stderr,"gte: %s: cases %u to %u: known answer result %03x\n",gte_variants[variant],first,
                first+count-1,c.result);
            bad=1;
        }
    }
    u32 got=rl(&cpu,s+4);
    printf("gte: %s: digest %08x want %08x, %u cases  %10" PRIu64 " instructions %10" PRIu64 " cycles\n",
        gte_variants[variant],got,want,SH2O_GTE_CASES,instructions,cycles);
    if(!bad)
        printf("gte: %s: known answers: 14 of 14 hold\n",gte_variants[variant]);
    printf("gte: %s: most instructions of one _part call (%u cases): %" PRIu64 "\n",gte_variants[variant],GTE_CHUNK,
        most);
    if(most>=GTE_LIMIT) {
        fprintf(stderr,"gte: %s: a _part call is at the instruction limit\n",gte_variants[variant]);
        bad=1;
    }
    if(got!=want) {
        fprintf(stderr,"gte: %s: the digest is not the pin\n",gte_variants[variant]);
        bad=1;
    }
    return bad;
}

/* SR.S = 1. The library checks SR.S, so any result that differs fails.
 * Returns 1 on a difference, or when the SR.S = 0 chain of one-case calls does
 * not give the pin (a fault of this check, not of SR.S). */
static int gte_sr_s(unsigned variant) {
    u32 s=gte_io+GTE_IO_DIGEST;
    unsigned differ=0;
    size_t total=0,cmd_differ=0;
    wl(&cpu,s,SH2O_GTE_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    for(u32 i=0;i<SH2O_GTE_CASES;i++) {
        u32 x=rl(&cpu,s),h=rl(&cpu,s+4);
        invoke("_sh2o_gte_test_digest_part",i,1,s,0);
        u32 x0=rl(&cpu,s),h0=rl(&cpu,s+4);
        wl(&cpu,s,x);
        wl(&cpu,s+4,h);
        invoke_sr("_sh2o_gte_test_digest_part",i,1,s,0,2);
        if(rl(&cpu,s+4)!=h0)
            differ++;
        wl(&cpu,s,x0);
        wl(&cpu,s+4,h0);
    }
    if(rl(&cpu,s+4)!=SH2O_DIGEST_GTE) {
        fprintf(stderr,"gte: %s: the chain of one-case calls gives %08x, not the pin\n",gte_variants[variant],
            rl(&cpu,s+4));
        return 1;
    }
    printf("gte: %s: SR.S 1: suite: %u of %u differ%s\n",gte_variants[variant],differ,SH2O_GTE_CASES,
        differ?"  FAIL":"");
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        uint8_t out[GTE_OUT_MAX];
        struct cost c;
        if(vector->op!=GTE_OP_CMD)
            continue;
        gte_run(vector,out,2,&c);
        total++;
        if(memcmp(out,vector->out,vector->out_size))
            cmd_differ++;
    }
    printf("gte: %s: SR.S 1: oracle cmd: %zu of %zu differ%s\n",gte_variants[variant],cmd_differ,total,
        cmd_differ?"  FAIL":"");
    return differ||cmd_differ;
}

/* The entries against _sh2o_gte_command with their words (step 4 of the header). The SR.S = 0 calls give the
 * timing. Returns 1 on any difference. */
static int gte_entry(unsigned variant,int plant_entry) {
    size_t vectors[3]={0,0,0},differ[2][3]={{0,0,0},{0,0,0}},results[3]={0,0,0},
        result_differ[2][3]={{0,0,0},{0,0,0}};
    int bad=0;
    u32 g=gte_io+GTE_IO_IMAGE;
    for(unsigned sr_s=0;sr_s<2;sr_s++) {
        u32 sr=sr_s?2u:0u;
        for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
            const gte_vector_t *vector=&gte_vectors[i];
            u32 cmd=gte_get_u32(vector->in+256),got[64],want[64];
            unsigned e;
            if(vector->op!=GTE_OP_CMD)
                continue;
            if((cmd&0x3Fu)==0x01u)
                e=0;
            else if((cmd&0x3Fu)==0x30u)
                e=1;
            else if((cmd&0x3Fu)==0x2Au)
                e=2;
            else
                continue;
            u32 word=e==2?(plant_entry?cmd^0x80000u:cmd):plant_entry?gte_entry_planted[e]:gte_entry_words[e];
            gte_image(vector->in,sr);
            struct cost c=invoke_sr(gte_entry_symbols[e],g,e==2?cmd:0,0,0,sr);
            for(unsigned r=0;r<64;r++)
                got[r]=invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result;
            gte_image(vector->in,sr);
            invoke_sr("_sh2o_gte_command",g,word,0,0,sr);
            for(unsigned r=0;r<64;r++)
                want[r]=invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result;
            if(!sr_s) {
                gte_stats_add(&gte_entry_stats[variant][e],&c);
                vectors[e]++;
            }
            for(unsigned r=0;r<64;r++) {
                if(got[r]==want[r])
                    continue;
                if(!bad)
                    fprintf(stderr,"gte: %s: SR.S %u: %s differs from the command %08x in vector %zu (word %08x), "
                        "register %u: command %08x, entry %08x\n",gte_variants[variant],sr_s,gte_entry_names[e],word,
                        i,cmd,r,want[r],got[r]);
                differ[sr_s][e]++;
                bad=1;
                break;
            }
            if(e!=2&&(cmd&0x80400u)!=0x80000u)
                continue;
            if(!sr_s)
                results[e]++;
            for(unsigned r=0;r<64;r++) {
                if(got[r]==gte_get_u32(vector->out+4+4*r))
                    continue;
                if(!bad)
                    fprintf(stderr,"gte: %s: SR.S %u: %s differs from the result of vector %zu (word %08x), "
                        "register %u\n",gte_variants[variant],sr_s,gte_entry_names[e],i,cmd,r);
                result_differ[sr_s][e]++;
                bad=1;
                break;
            }
        }
    }
    for(unsigned e=0;e<3;e++) {
        char word[40];
        if(e==2)
            snprintf(word,sizeof(word),"%s",plant_entry?"with the vector's word, sf flipped":"with the vector's word");
        else
            snprintf(word,sizeof(word),"%08x",plant_entry?gte_entry_planted[e]:gte_entry_words[e]);
        printf("gte: %s: entry %s == command %s: SR.S 0: %zu of %zu differ, SR.S 1: %zu of %zu differ; "
            "== vector result (%s): SR.S 0: %zu of %zu differ, SR.S 1: %zu of %zu differ%s\n",
            gte_variants[variant],gte_entry_names[e],word,differ[0][e],vectors[e],differ[1][e],vectors[e],
            e==2?"all":"sf 1, lm 0",result_differ[0][e],results[e],result_differ[1][e],results[e],
            differ[0][e]||differ[1][e]||result_differ[0][e]||result_differ[1][e]?"  FAIL":"");
    }
    return bad;
}

/* The MVMVA kernel path: cmd 12h, sf 1, mx < 3, cv != 2 and each t inside the range test of sh2oi_mac3
 * (src/sh2o_gte.c). Gives 0 for any other word; else 1, with the matrix's first word r, V and T as
 * sh2o_gte_mvmva takes them from the image at g, and want[i] = (int64)T[i] << 12 + M[i].V. */
static int gte_mac3_input(u32 g,u32 cmd,unsigned *r,int16_t v[3],int32_t t[3],int64_t want[3]) {
    unsigned mx=cmd>>17&3u,vx=cmd>>15&3u,cv=cmd>>13&3u;
    int16_t m[9];
    if((cmd&0x3Fu)!=0x12u||!(cmd&0x80000u)||mx==3||cv==2)
        return 0;
    *r=32u+8u*mx;
    for(unsigned i=0;i<3;i++) {
        t[i]=cv==3?0:(int32_t)rl(&cpu,g+4*((cv?45u:37u)+i));
        if((uint32_t)t[i]+0x7FF3FFFFu>=0xFFE7FFFFu)
            return 0;
    }
    if(vx==3) {
        for(unsigned i=0;i<3;i++)
            v[i]=(int16_t)rl(&cpu,g+4*(9+i));
    } else {
        u32 xy=rl(&cpu,g+8*vx);
        v[0]=(int16_t)xy;
        v[1]=(int16_t)(xy>>16);
        v[2]=(int16_t)rl(&cpu,g+8*vx+4);
    }
    for(unsigned k=0;k<9;k++)
        m[k]=(int16_t)(rl(&cpu,g+4*(*r+k/2))>>(16*(k&1)));
    for(unsigned i=0;i<3;i++)
        want[i]=(int64_t)t[i]*4096+(int64_t)m[3*i]*v[0]+(int64_t)m[3*i+1]*v[1]+(int64_t)m[3*i+2]*v[2];
    return 1;
}

/* Step 5: the whole and kernel calls of each kernel-path cmd vector, at SR.S = 0. */
static int gte_mac3(unsigned variant) {
    u32 g=gte_io+GTE_IO_IMAGE,vp=gte_io+GTE_IO_V16,tp=gte_io+GTE_IO_T,op=gte_io+GTE_IO_OUT;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        u32 cmd=gte_get_u32(vector->in+256);
        unsigned r;
        int16_t v[3];
        int32_t t[3];
        int64_t want[3];
        if(vector->op!=GTE_OP_CMD||(cmd&0x3Fu)!=0x12u)
            continue;
        gte_image(vector->in,0);
        if(!gte_mac3_input(g,cmd,&r,v,t,want))
            continue;
        struct cost c=invoke_sr("_sh2o_gte_mvmva",g,cmd,0,0,0);
        for(unsigned reg=0;reg<64;reg++) {
            u32 got=invoke_sr("_sh2o_gte_read",g,reg,0,0,0).result,expect=gte_get_u32(vector->out+4+4*reg);
            if(got==expect)
                continue;
            fprintf(stderr,"gte: %s: MVMVA kernel path: whole differs from the result of vector %zu (word %08x), "
                "register %u: result %08x, whole %08x\n",gte_variants[variant],i,cmd,reg,expect,got);
            return 1;
        }
        gte_stats_add(&gte_mac3_stats[variant][0],&c);
        for(unsigned k=0;k<3;k++) {
            ww(&cpu,vp+2*k,(u16)v[k]);
            wl(&cpu,tp+4*k,(u32)t[k]);
            wl(&cpu,op+8*k,0);
            wl(&cpu,op+8*k+4,0);
        }
        c=invoke_sr("_sh2o_model_gte_mac3",g+4*r,vp,tp,op,0);
        for(unsigned k=0;k<3;k++) {
            int64_t got=(int64_t)((uint64_t)rl(&cpu,op+8*k)<<32|rl(&cpu,op+8*k+4));
            u32 mac=gte_get_u32(vector->out+4+4*(25+k));
            if(got==want[k]&&(u32)(int32_t)(got>>12)==mac)
                continue;
            fprintf(stderr,"gte: %s: MVMVA kernel path: kernel differs in vector %zu (word %08x), out[%u]: "
                "%016" PRIx64 ", T*1000h + M*V %016" PRIx64 ", MAC%u %08x\n",gte_variants[variant],i,cmd,k,
                (uint64_t)got,(uint64_t)want[k],k+1,mac);
            return 1;
        }
        gte_stats_add(&gte_mac3_stats[variant][1],&c);
    }
    if(gte_mac3_stats[variant][0].count==0) {
        fprintf(stderr,"gte: %s: MVMVA kernel path: no oracle vector takes it\n",gte_variants[variant]);
        return 1;
    }
    printf("gte: %s: MVMVA kernel path, SR.S 0: %" PRIu64 " vectors: whole == vector result, kernel == T*1000h + M*V "
        "and MAC1-3: 0 differ\n",gte_variants[variant],gte_mac3_stats[variant][0].count);
    return 0;
}

static void gte_row(unsigned variant,unsigned op,const char *name,const gte_stats_t *s) {
    printf("%-9s %02Xh  %-17s %7u %7u %9.2f %7" PRIu64 " %7" PRIu64 " %9.2f %7" PRIu64 "\n",
        gte_variants[variant],op,name,s->min_cycles,s->max_cycles,
        (double)s->sum_cycles/(double)s->count,s->min_instructions,s->max_instructions,
        (double)s->sum_instructions/(double)s->count,s->count);
}

int main(int argc,char **argv) {
    long plant=-1;
    int plant_digest=0,plant_entry=0;
    if(argc==12&&!strcmp(argv[10],"--plant")) {
        char *end;
        plant=strtol(argv[11],&end,10);
        if(*end||plant<0||(size_t)plant>=GTE_VECTOR_COUNT) {
            fprintf(stderr,"gte: --plant needs a vector from 0 to %zu\n",GTE_VECTOR_COUNT-1);
            return 2;
        }
    } else if(argc==11&&!strcmp(argv[10],"--plant-digest")) {
        plant_digest=1;
    } else if(argc==11&&!strcmp(argv[10],"--plant-entry")) {
        plant_entry=1;
    } else if(argc!=10) {
        fprintf(stderr,"usage: gte_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant vector | --plant-digest | --plant-entry]\n");
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
        size_t cmds;
        gte_load(argv[1+2*variant],argv[2+2*variant]);
        if(gte_verify(variant,plant,&cmds))
            return 1;
        printf("gte: %s: %zu vectors exact (%zu cmd, %zu write) (%s)\n",gte_variants[variant],GTE_VECTOR_COUNT,cmds,
            GTE_VECTOR_COUNT-cmds,argv[1+2*variant]);
        if(gte_digest(variant,plant_digest))
            return 1;
        if(gte_sr_s(variant))
            return 1;
        if(gte_entry(variant,plant_entry))
            return 1;
        if(gte_mac3(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. One call of sh2o_gte_command, over the oracle cmd vectors.\n");
    printf("Rows nRTPS, nRTPT: one call of sh2o_gte_nrtps, sh2o_gte_nrtpt, over the RTPS, RTPT cmd vectors.\n");
    printf("Row eDPCT: one call of sh2o_gte_dpct with the vector's word, over the DPCT cmd vectors.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-4s %-17s %7s %7s %9s %7s %7s %9s %7s\n","variant","op","name","cyc min","cyc max","cyc mean",
        "ins min","ins max","ins mean","vectors");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned op=0;op<64;op++) {
            const gte_stats_t *s=&gte_stats[variant][op];
            if(s->count==0)
                continue;
            gte_row(variant,op,gte_op_name(op),s);
            if(op==0x01u&&gte_entry_stats[variant][0].count)
                gte_row(variant,op,gte_entry_names[0],&gte_entry_stats[variant][0]);
            if(op==0x30u&&gte_entry_stats[variant][1].count)
                gte_row(variant,op,gte_entry_names[1],&gte_entry_stats[variant][1]);
            if(op==0x2Au&&gte_entry_stats[variant][2].count)
                gte_row(variant,op,gte_entry_names[2],&gte_entry_stats[variant][2]);
        }
    }
    printf("\nMVMVA kernel path, SR.S 0: the oracle cmd vectors with cmd 12h, sf 1, mx < 3, cv != 2 and T inside the\n");
    printf("kernel's range. Row whole: one sh2o_gte_mvmva call. Row kernel: one sh2o_model_gte_mac3 call on the same\n");
    printf("inputs (sh2o_k_mvb3 and the wrapper's call). Overhead: whole mean - kernel mean.\n");
    printf("\n%-9s %-9s %7s %7s %9s %7s\n","variant","call","cyc min","cyc max","cyc mean","inputs");
    for(unsigned variant=0;variant<2;variant++) {
        const gte_stats_t *w=&gte_mac3_stats[variant][0],*k=&gte_mac3_stats[variant][1];
        double whole=(double)w->sum_cycles/(double)w->count,kernel=(double)k->sum_cycles/(double)k->count;
        printf("%-9s %-9s %7u %7u %9.2f %7" PRIu64 "\n",gte_variants[variant],"whole",w->min_cycles,w->max_cycles,
            whole,w->count);
        printf("%-9s %-9s %7u %7u %9.2f %7" PRIu64 "\n",gte_variants[variant],"kernel",k->min_cycles,k->max_cycles,
            kernel,k->count);
        printf("%-9s %-9s %25.2f  %.1f%% of whole\n",gte_variants[variant],"overhead",whole-kernel,
            100.0*(whole-kernel)/whole);
    }
    return 0;
}
