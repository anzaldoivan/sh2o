/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the fixed family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: fixed_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant vector]
 * Each ELF holds the library and benchmarks/fixed-model/bridge.c. Every
 * vector of tests/fixed_oracle.h runs on the library symbol of each ELF and
 * its result bytes are compared with the oracle before any figure is
 * printed. --plant flips one bit of a copy of the expected result of one
 * vector, to show that the comparison fails. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/fixed_oracle.h"

static const char *const fx_variants[2]={"native","portable"};
static const char *const fx_symbols[FIXED_OP_COUNT]={"_sh2o_mul_q12","_sh2o_lim_ir"};

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t min_instructions,max_instructions,sum_cycles,sum_instructions,count;
} fx_stats_t;

static fx_stats_t fx_stats[2][FIXED_OP_COUNT];

static u32 fx_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static void fx_print_bytes(const char *label,const uint8_t *bytes,unsigned size) {
    fprintf(stderr,"  %-8s",label);
    for(unsigned i=0;i<size;i++)
        fprintf(stderr," %02x",bytes[i]);
    fprintf(stderr,"\n");
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses. */
static void fx_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned op=0;op<FIXED_OP_COUNT;op++) {
        u32 entry=rl(&cpu,table+4*op);
        if(entry!=sym(fx_symbols[op])) {
            fprintf(stderr,"fixed: %s: bridge entry %u is %08x, symbol %s is %08x\n",
                elf,op,entry,fx_symbols[op],sym(fx_symbols[op]));
            exit(2);
        }
    }
}

/* Runs every vector on one ELF. Returns 1 at the first difference. */
static int fx_verify(unsigned variant,long plant) {
    for(size_t i=0;i<FIXED_VECTOR_COUNT;i++) {
        const fixed_vector_t *vector=&fixed_vectors[i];
        uint8_t expected[FIXED_OUT_MAX],out[FIXED_OUT_MAX];
        memcpy(expected,vector->out,sizeof(expected));
        if(plant>=0&&(size_t)plant==i)
            expected[0]^=1;
        if(vector->op>=FIXED_OP_COUNT) {
            fprintf(stderr,"fixed: vector has unknown op %u\n",vector->op);
            exit(2);
        }
        u32 a=fx_get_u32(vector->in),b;
        if(vector->op==FIXED_OP_MUL_Q12) {
            b=fx_get_u32(vector->in+4);
        } else {
            if(vector->in[4]>1) {
                fprintf(stderr,"fixed: vector %zu has lm byte %u, not 0 or 1\n",i,vector->in[4]);
                exit(2);
            }
            b=vector->in[4];
        }
        struct cost c=invoke(fx_symbols[vector->op],a,b,0,0);
        for(unsigned byte=0;byte<4;byte++)
            out[byte]=(uint8_t)(c.result>>(8*byte));
        fx_stats_t *s=&fx_stats[variant][vector->op];
        if(s->count==0||c.cycles<s->min_cycles)
            s->min_cycles=c.cycles;
        if(s->count==0||c.cycles>s->max_cycles)
            s->max_cycles=c.cycles;
        if(s->count==0||c.instructions<s->min_instructions)
            s->min_instructions=c.instructions;
        if(s->count==0||c.instructions>s->max_instructions)
            s->max_instructions=c.instructions;
        s->sum_cycles+=c.cycles;
        s->sum_instructions+=c.instructions;
        s->count++;
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            if(out[byte]==expected[byte])
                continue;
            fprintf(stderr,"fixed: %s: first difference in vector %zu (%s), result byte %u\n",
                fx_variants[variant],i,fixed_op_names[vector->op],byte);
            fx_print_bytes("in",vector->in,vector->in_size);
            fx_print_bytes("expected",expected,vector->out_size);
            fx_print_bytes("got",out,vector->out_size);
            return 1;
        }
    }
    return 0;
}

int main(int argc,char **argv) {
    long plant=-1;
    if(argc==12&&!strcmp(argv[10],"--plant")) {
        char *end;
        plant=strtol(argv[11],&end,10);
        if(*end||plant<0||(size_t)plant>=FIXED_VECTOR_COUNT) {
            fprintf(stderr,"fixed: --plant needs a vector from 0 to %zu\n",FIXED_VECTOR_COUNT-1);
            return 2;
        }
    } else if(argc!=10) {
        fprintf(stderr,"usage: fixed_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant vector]\n");
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
        fx_load(argv[1+2*variant],argv[2+2*variant]);
        if(fx_verify(variant,plant))
            return 1;
        printf("fixed: %s: %zu vectors verified (%s)\n",fx_variants[variant],FIXED_VECTOR_COUNT,argv[1+2*variant]);
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-8s %7s %7s %9s %7s %7s %9s %7s\n","variant","op","cyc min","cyc max","cyc mean",
        "ins min","ins max","ins mean","vectors");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned op=0;op<FIXED_OP_COUNT;op++) {
            const fx_stats_t *s=&fx_stats[variant][op];
            printf("%-9s %-8s %7u %7u %9.2f %7" PRIu64 " %7" PRIu64 " %9.2f %7" PRIu64 "\n",
                fx_variants[variant],fixed_op_names[op],s->min_cycles,s->max_cycles,
                (double)s->sum_cycles/(double)s->count,s->min_instructions,s->max_instructions,
                (double)s->sum_instructions/(double)s->count,s->count);
        }
    }
    return 0;
}
