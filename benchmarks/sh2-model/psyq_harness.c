/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the psyq family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: psyq_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant-digest | --plant-known k]
 * Each ELF holds the library, benchmarks/psyq-model/bridge.c and
 * tests/suite/sh2o_psyq_test_suite.c. The psyq family has no oracle file:
 * the suite's digest and known answers are the checks. For each ELF:
 * 1. The suite's digest over the 1,216 cases is computed in the ELF, as a
 *    chain of calls of sh2o_psyq_test_digest_part, PSYQ_CHUNK cases each, so
 *    that no call reaches the 100,000,000 instruction limit of SH2ZAM's
 *    invoke. The last call also runs the 6 ratan2 known answers; its return
 *    must be 0, and the digest must be the pin of sh2o_psyq_test.h. The
 *    harness prints the most instructions of one call. --plant-digest
 *    compares the digest with the pin XOR 1; --plant-known k flips bit 0 of
 *    the expected value (want) of known answer k in the ELF's RAM before the
 *    run. Each shows that its comparison fails. A failure stops the run.
 * 2. Each case runs again with the body called directly: the ELF draws the
 *    case (sh2o_psyq_test_case_draw), gives the body's four argument words
 *    (sh2o_model_psyq_args), the harness calls _sh2o_psyq_<body> and maps its
 *    result to the return word as sh2o_psyq_call does, and the ELF hashes the
 *    case (sh2o_psyq_test_case_hash). This chain must also give the pin, so
 *    the timed calls are the suite's cases.
 * 3. Each suite case runs again with SR.S = 1, from the state that the
 *    SR.S = 0 chain of one-case calls has before that case. The library
 *    checks SR.S, so any hash that differs fails the run.
 * The figures are the cycles of one body call, for each body, over its 64
 * suite cases. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/suite/sh2o_psyq_test.h"

#define PSYQ_IO_DIGEST 0u /* the digest state s[2] */
#define PSYQ_IO_ARGS 8u   /* the four argument words */
#define PSYQ_IO_CASE 24u  /* one sh2o_psyq_test_case_t */
#define PSYQ_CHUNK 256u   /* cases per digest call */
#define PSYQ_LIMIT UINT64_C(100000000)
#define PSYQ_KNOWN 6u     /* ratan2 known answers: {y, x, want, brk}, 16 bytes each */

static const char *const psyq_variants[2]={"native","portable"};
static const char *const psyq_table_symbols[4]={"_sh2o_psyq_test_digest_part","_sh2o_psyq_test_case_draw",
    "_sh2o_psyq_test_case_hash","_sh2o_model_psyq_args"};

/* The bodies in the suite's case order. ret: 'v' void (return word 0), 'i' int (R0), 'p' pointer (arg: the index of
 * the argument word it must equal; the return word is that index, else 0xFFFFFFFF). */
typedef struct {
    const char *name;
    char ret;
    unsigned arg;
} psyq_body_t;

static const psyq_body_t psyq_bodies[SH2O_PSYQ_BODIES]={
    {"ApplyMatrix",'p',3},{"ApplyMatrixLV",'p',3},{"CompMatrix",'p',3},{"InitGeom",'v',0},{"PopMatrix",'i',0},
    {"PushMatrix",'i',0},{"RotTrans",'v',0},{"SetBackColor",'v',0},{"SetColorMatrix",'v',0},{"SetFarColor",'v',0},
    {"SetFogNearFar",'v',0},{"SetGeomOffset",'v',0},{"SetGeomScreen",'v',0},{"SetLightMatrix",'v',0},
    {"SetRotMatrix",'v',0},{"SetTransMatrix",'v',0},{"TransMatrix",'p',1},{"TransposeMatrix",'p',2},{"ratan2",'i',0}};

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t sum_cycles,sum_instructions,count;
} psyq_stats_t;

static psyq_stats_t psyq_stats[2][SH2O_PSYQ_BODIES];
static u32 psyq_io;
static char psyq_symbol[SH2O_PSYQ_BODIES][64];

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/psyq-model/bridge.c)
 * and, for --plant-known k, flips bit 0 of want of known answer k. */
static void psyq_load(const char *elf,const char *symbols_path,long plant_known) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    psyq_io=sym("_sh2o_model_psyq_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<4+SH2O_PSYQ_BODIES;entry++) {
        const char *name=entry<4?psyq_table_symbols[entry]:psyq_symbol[entry-4];
        if(rl(&cpu,table+4*entry)!=sym(name)) {
            fprintf(stderr,"psyq: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),name,sym(name));
            exit(2);
        }
    }
    if(plant_known>=0) {
        u32 want=sym("_sh2o_pka_ratan2")+16u*(u32)plant_known+8u;
        wl(&cpu,want,rl(&cpu,want)^1u);
    }
}

/* The suite's digest as a chain of _part calls, and its known answers. Returns 1 when a check fails. */
static int psyq_digest(unsigned variant,int plant_digest) {
    u32 s=psyq_io+PSYQ_IO_DIGEST,want=SH2O_DIGEST_PSYQ^(plant_digest?1u:0u);
    uint64_t most=0,instructions=0,cycles=0;
    int bad=0;
    wl(&cpu,s,SH2O_PSYQ_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    for(u32 first=0;first<SH2O_PSYQ_CASES;first+=PSYQ_CHUNK) {
        u32 count=SH2O_PSYQ_CASES-first<PSYQ_CHUNK?SH2O_PSYQ_CASES-first:PSYQ_CHUNK;
        struct cost c=invoke("_sh2o_psyq_test_digest_part",first,count,s,0);
        instructions+=c.instructions;
        cycles+=c.cycles;
        if(c.instructions>most)
            most=c.instructions;
        if(c.result!=0) {
            fprintf(stderr,"psyq: %s: cases %u to %u: known answer result %03x\n",psyq_variants[variant],first,
                first+count-1,c.result);
            bad=1;
        }
    }
    u32 got=rl(&cpu,s+4);
    printf("psyq: %s: digest %08x want %08x, %u cases  %10" PRIu64 " instructions %10" PRIu64 " cycles\n",
        psyq_variants[variant],got,want,SH2O_PSYQ_CASES,instructions,cycles);
    if(!bad)
        printf("psyq: %s: known answers: %u of %u hold\n",psyq_variants[variant],PSYQ_KNOWN,PSYQ_KNOWN);
    printf("psyq: %s: most instructions of one _part call (%u cases): %" PRIu64 "\n",psyq_variants[variant],
        PSYQ_CHUNK,most);
    if(most>=PSYQ_LIMIT) {
        fprintf(stderr,"psyq: %s: a _part call is at the instruction limit\n",psyq_variants[variant]);
        bad=1;
    }
    if(got!=want) {
        fprintf(stderr,"psyq: %s: the digest is not the pin\n",psyq_variants[variant]);
        bad=1;
    }
    return bad;
}

/* Each case with its body called directly; records the cost of each body call. Returns 1 when the chain does not
 * give the pin. */
static int psyq_cases(unsigned variant) {
    u32 s=psyq_io+PSYQ_IO_DIGEST,args=psyq_io+PSYQ_IO_ARGS,c=psyq_io+PSYQ_IO_CASE;
    wl(&cpu,s,SH2O_PSYQ_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    for(u32 i=0;i<SH2O_PSYQ_CASES;i++) {
        u32 a[4],ret;
        invoke("_sh2o_psyq_test_case_draw",i,s,c,0);
        invoke("_sh2o_model_psyq_args",c,args,0,0);
        u32 body=rl(&cpu,c);
        if(body!=i>>SH2O_PSYQ_PER_LOG2) {
            fprintf(stderr,"psyq: %s: case %u has body %u\n",psyq_variants[variant],i,body);
            exit(2);
        }
        for(unsigned k=0;k<4;k++)
            a[k]=rl(&cpu,args+4*k);
        const psyq_body_t *b=&psyq_bodies[body];
        struct cost cost=invoke(psyq_symbol[body],a[0],a[1],a[2],a[3]);
        if(b->ret=='v')
            ret=0;
        else if(b->ret=='i')
            ret=cost.result;
        else
            ret=cost.result==a[b->arg]?b->arg:UINT32_C(0xFFFFFFFF);
        invoke("_sh2o_psyq_test_case_hash",c,ret,s,0);
        psyq_stats_t *st=&psyq_stats[variant][body];
        if(st->count==0||cost.cycles<st->min_cycles)
            st->min_cycles=cost.cycles;
        if(st->count==0||cost.cycles>st->max_cycles)
            st->max_cycles=cost.cycles;
        st->sum_cycles+=cost.cycles;
        st->sum_instructions+=cost.instructions;
        st->count++;
    }
    if(rl(&cpu,s+4)!=SH2O_DIGEST_PSYQ) {
        fprintf(stderr,"psyq: %s: the chain of direct body calls gives %08x, not the pin\n",psyq_variants[variant],
            rl(&cpu,s+4));
        return 1;
    }
    printf("psyq: %s: direct body calls: %u cases give the pin %08x\n",psyq_variants[variant],SH2O_PSYQ_CASES,
        SH2O_DIGEST_PSYQ);
    return 0;
}

/* SR.S = 1. The library checks SR.S, so any result that differs fails.
 * Returns 1 on a difference, or when the SR.S = 0 chain of one-case calls does
 * not give the pin (a fault of this check, not of SR.S). */
static int psyq_sr_s(unsigned variant) {
    u32 s=psyq_io+PSYQ_IO_DIGEST;
    unsigned differ=0;
    wl(&cpu,s,SH2O_PSYQ_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    for(u32 i=0;i<SH2O_PSYQ_CASES;i++) {
        u32 x=rl(&cpu,s),h=rl(&cpu,s+4);
        invoke("_sh2o_psyq_test_digest_part",i,1,s,0);
        u32 x0=rl(&cpu,s),h0=rl(&cpu,s+4);
        wl(&cpu,s,x);
        wl(&cpu,s+4,h);
        invoke_sr("_sh2o_psyq_test_digest_part",i,1,s,0,2);
        if(rl(&cpu,s+4)!=h0)
            differ++;
        wl(&cpu,s,x0);
        wl(&cpu,s+4,h0);
    }
    if(rl(&cpu,s+4)!=SH2O_DIGEST_PSYQ) {
        fprintf(stderr,"psyq: %s: the chain of one-case calls gives %08x, not the pin\n",psyq_variants[variant],
            rl(&cpu,s+4));
        return 1;
    }
    printf("psyq: %s: SR.S 1: %u of %u differ%s\n",psyq_variants[variant],differ,SH2O_PSYQ_CASES,
        differ?"  FAIL":"");
    return differ!=0;
}

int main(int argc,char **argv) {
    long plant_known=-1;
    int plant_digest=0;
    if(argc==12&&!strcmp(argv[10],"--plant-known")) {
        char *end;
        plant_known=strtol(argv[11],&end,10);
        if(*end||plant_known<0||plant_known>=(long)PSYQ_KNOWN) {
            fprintf(stderr,"psyq: --plant-known needs a known answer from 0 to %u\n",PSYQ_KNOWN-1);
            return 2;
        }
    } else if(argc==11&&!strcmp(argv[10],"--plant-digest")) {
        plant_digest=1;
    } else if(argc!=10) {
        fprintf(stderr,"usage: psyq_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant-digest | --plant-known k]\n");
        return 2;
    }
    for(unsigned b=0;b<SH2O_PSYQ_BODIES;b++)
        snprintf(psyq_symbol[b],sizeof psyq_symbol[b],"_sh2o_psyq_%s",psyq_bodies[b].name);
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
        psyq_load(argv[1+2*variant],argv[2+2*variant],plant_known);
        printf("psyq: %s: bridge table checked (%s)\n",psyq_variants[variant],argv[1+2*variant]);
        if(psyq_digest(variant,plant_digest))
            return 1;
        if(psyq_cases(variant))
            return 1;
        if(psyq_sr_s(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. One call of each body, over its %u suite cases.\n",SH2O_PSYQ_PER);
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-16s %7s %7s %9s %9s %6s\n","variant","body","cyc min","cyc max","cyc mean","ins mean","cases");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned b=0;b<SH2O_PSYQ_BODIES;b++) {
            const psyq_stats_t *st=&psyq_stats[variant][b];
            printf("%-9s %-16s %7u %7u %9.2f %9.2f %6" PRIu64 "\n",psyq_variants[variant],psyq_bodies[b].name,
                st->min_cycles,st->max_cycles,(double)st->sum_cycles/(double)st->count,
                (double)st->sum_instructions/(double)st->count,st->count);
        }
    }
    return 0;
}
