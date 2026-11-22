/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the dpct32 family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: dpct32_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant-known k]
 * Each ELF holds the library and benchmarks/dpct32-model/bridge.c. The
 * dpct32 family has no oracle file of its own: its known answers are the DPCT
 * (op 2Ah) cmd vectors of tests/gte_oracle.h. For each ELF, at SR.S = 0 and
 * at SR.S = 1, each DPCT vector's 64 registers are written through
 * _sh2o_gte_write in the write order of tests/suite/sh2o_gte_test_suite.c on a
 * zeroed image in the bridge buffer; then _sh2o_gte_dpct (the C path) or
 * _sh2o_gte_dpct32 (the kernel) runs with the vector's word, and the 64 words
 * of _sh2o_gte_read must equal the vector's result. All comparisons come
 * before any figure; one difference stops the run (exit 1). --plant-known k
 * flips bit 0 of a copy of the first result word of DPCT vector k, to show
 * that the comparison fails.
 * The figures are the cycles of one call of each function at SR.S = 0, over
 * the same vectors. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/gte_oracle.h"

#define DPCT32_IO_IMAGE 0u /* one sh2o_gte_t: 64 words */
#define DPCT32_ORDER_COUNT 61u

static const char *const dpct32_variants[2]={"native","portable"};
static const char *const dpct32_table_symbols[4]={"_sh2o_gte_write","_sh2o_gte_read","_sh2o_gte_dpct",
    "_sh2o_gte_dpct32"};
/* The two functions timed: [0] the C path, [1] the kernel. */
static const char *const dpct32_paths[2]={"C","dpct32"};
static const char *const dpct32_path_symbols[2]={"_sh2o_gte_dpct","_sh2o_gte_dpct32"};
/* The write order of tests/suite/sh2o_gte_test_suite.c (sh2o_gte_order): every register but 15, 29 and 31, 28
 * before IR1-3. */
static const unsigned char dpct32_order[DPCT32_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,
    21,22,23,24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,
    62,63};

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t min_instructions,max_instructions,sum_cycles,sum_instructions,count;
} dpct32_stats_t;

static dpct32_stats_t dpct32_stats[2][2]; /* [variant][path] */
static u32 dpct32_io;

static u32 dpct32_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/dpct32-model/bridge.c). */
static void dpct32_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    dpct32_io=sym("_sh2o_model_dpct32_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<4;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(dpct32_table_symbols[entry])) {
            fprintf(stderr,"dpct32: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),dpct32_table_symbols[entry],sym(dpct32_table_symbols[entry]));
            exit(2);
        }
    }
}

static void dpct32_stats_add(dpct32_stats_t *s,const struct cost *c) {
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

/* Every DPCT vector through both paths, at SR.S 0 and 1. Returns 1 at the first difference; *vectors: the DPCT
 * vectors. */
static int dpct32_verify(unsigned variant,long plant_known,unsigned *vectors) {
    u32 g=dpct32_io+DPCT32_IO_IMAGE;
    for(unsigned sr_s=0;sr_s<2;sr_s++) {
        u32 sr=sr_s?2u:0u;
        unsigned k=0;
        for(size_t i=0;i<sizeof gte_vectors/sizeof gte_vectors[0];i++) {
            const gte_vector_t *vector=&gte_vectors[i];
            u32 cmd=dpct32_get_u32(vector->in+256),want[64];
            if(vector->op!=GTE_OP_CMD||(cmd&0x3Fu)!=0x2Au)
                continue;
            for(unsigned r=0;r<64;r++)
                want[r]=dpct32_get_u32(vector->out+4+4*r);
            if(plant_known>=0&&(unsigned long)plant_known==k)
                want[0]^=1u;
            for(unsigned path=0;path<2;path++) {
                for(unsigned r=0;r<64;r++)
                    wl(&cpu,g+4*r,0);
                for(unsigned j=0;j<DPCT32_ORDER_COUNT;j++)
                    invoke_sr("_sh2o_gte_write",g,dpct32_order[j],dpct32_get_u32(vector->in+4*dpct32_order[j]),0,sr);
                struct cost c=invoke_sr(dpct32_path_symbols[path],g,cmd,0,0,sr);
                for(unsigned r=0;r<64;r++) {
                    u32 got=invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result;
                    if(got==want[r])
                        continue;
                    fprintf(stderr,"dpct32: %s: SR.S %u: %s: the known answer of DPCT vector %u (oracle vector %zu, "
                        "word %08x) does not hold: register %u expected %08x, got %08x\n",dpct32_variants[variant],
                        sr_s,dpct32_paths[path],k,i,cmd,r,want[r],got);
                    return 1;
                }
                if(!sr_s)
                    dpct32_stats_add(&dpct32_stats[variant][path],&c);
            }
            k++;
        }
        *vectors=k;
    }
    return 0;
}

int main(int argc,char **argv) {
    long plant_known=-1;
    unsigned dpct_vectors=0;
    for(size_t i=0;i<sizeof gte_vectors/sizeof gte_vectors[0];i++)
        if(gte_vectors[i].op==GTE_OP_CMD&&(dpct32_get_u32(gte_vectors[i].in+256)&0x3Fu)==0x2Au)
            dpct_vectors++;
    if(argc==12&&!strcmp(argv[10],"--plant-known")) {
        char *end;
        plant_known=strtol(argv[11],&end,10);
        if(*end||plant_known<0||plant_known>=(long)dpct_vectors) {
            fprintf(stderr,"dpct32: --plant-known needs a DPCT vector from 0 to %u\n",dpct_vectors-1);
            return 2;
        }
    } else if(argc!=10) {
        fprintf(stderr,"usage: dpct32_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant-known k]\n");
        return 2;
    }
    if(dpct_vectors==0) {
        fprintf(stderr,"dpct32: tests/gte_oracle.h has no DPCT vector\n");
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
        unsigned vectors;
        dpct32_load(argv[1+2*variant],argv[2+2*variant]);
        if(dpct32_verify(variant,plant_known,&vectors))
            return 1;
        printf("dpct32: %s: %u DPCT vectors exact through sh2o_gte_dpct and sh2o_gte_dpct32, SR.S 0 and SR.S 1 (%s)\n",
            dpct32_variants[variant],vectors,argv[1+2*variant]);
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. One call of sh2o_gte_dpct (C) and of sh2o_gte_dpct32 (dpct32) with the vector's\n");
    printf("word, at SR.S 0, over the DPCT cmd vectors of tests/gte_oracle.h.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-7s %7s %7s %9s %7s %7s %9s %7s\n","variant","path","cyc min","cyc max","cyc mean","ins min",
        "ins max","ins mean","vectors");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned path=0;path<2;path++) {
            const dpct32_stats_t *s=&dpct32_stats[variant][path];
            printf("%-9s %-7s %7u %7u %9.2f %7" PRIu64 " %7" PRIu64 " %9.2f %7" PRIu64 "\n",dpct32_variants[variant],
                dpct32_paths[path],s->min_cycles,s->max_cycles,(double)s->sum_cycles/(double)s->count,
                s->min_instructions,s->max_instructions,(double)s->sum_instructions/(double)s->count,s->count);
        }
    }
    return 0;
}
