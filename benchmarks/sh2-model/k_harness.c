/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the k family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: k_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant vector]
 * Each ELF holds the library, benchmarks/k-model/bridge.c and
 * tests/suite/sh2o_k_test_suite.c. For each ELF:
 * 1. Every vector of tests/k_oracle.h runs through the public entry, the C
 *    reference (_c) and, in the native ELF, the MAC.W kernels _w (SR.S-blind)
 *    and _w2 (checks SR.S), each by its bridge wrapper, with SR.S = 0. The result bytes are compared with the oracle before any
 *    figure is printed. --plant flips one bit of a copy of the expected result
 *    of one vector, to show that the comparison fails.
 * 2. The suite's digests are compared with the pins of sh2o_k_test.h. In the
 *    native ELF, each of the six test mutants must give a digest that is not
 *    its pin. A digest runs as a chain of calls of its resumable _part
 *    function, K_CHUNK vectors each, so that no call reaches the 100,000,000
 *    instruction limit of SH2ZAM's invoke; the figures are the chain's sums,
 *    and the harness prints the most instructions of any one call.
 * 3. Native only: the entry, _w and _w2 wrappers of the four MAC ops run
 *    again over their vectors with SR.S = 1. Any difference on the entry or
 *    _w2 fails the run. The _w kernels assume SR.S = 0 (their contract), so
 *    for them the harness only prints how many results differ. Then, per op,
 *    the mean MODELLED cycles of _w and _w2 at SR.S = 0 on the same vectors
 *    and their difference (the cost of the SR.S check). */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/k_oracle.h"
#include "../../tests/suite/sh2o_k_test.h"

#define K_PATHS 4
#define K_MAC_OPS 4 /* dot3b, mvb3, gpf3, gpl3: the ops with a _w path */
#define K_IO_IN 0u
#define K_IO_OUT 128u
#define K_IO_DIGEST 192u
#define K_CHUNK 8192u /* vectors per _part call */
_Static_assert(K_IN_MAX<=K_IO_OUT-K_IO_IN,"a k op's inputs do not fit the bridge io");
_Static_assert(K_OUT_MAX<=K_IO_DIGEST-K_IO_OUT,"a k op's results do not fit the bridge io");

static const char *const k_variants[2]={"native","portable"};
static const char *const k_paths[K_PATHS]={"entry","c","w","w2"};
static const char *const k_digest_symbols[3]={"_sh2o_k_test_dot3b_part","_sh2o_k_test_mvb3_part",
    "_sh2o_k_test_gp3_part"};

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t min_instructions,max_instructions,sum_cycles,sum_instructions,count;
} k_stats_t;

static k_stats_t k_stats[2][K_OP_COUNT][K_PATHS];
static u32 k_io;
static uint64_t k_call_max; /* the most instructions of one _part call */

typedef struct {
    uint64_t instructions,cycles;
} k_total_t;

static int k_has_path(unsigned variant,unsigned path,unsigned op) {
    return path<2||(variant==0&&op<K_MAC_OPS);
}

/* The bridge wrapper of an op on a path, as the symbol that nm gives. */
static const char *k_symbol(unsigned path,unsigned op) {
    static char name[64];
    snprintf(name,sizeof(name),"_sh2o_model_k_%s%s",k_op_names[op],path==0?"":path==1?"_c":path==2?"_w":"_w2");
    return name;
}

static u32 k_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static u16 k_get_u16(const uint8_t *bytes) {
    return (u16)(bytes[0]|(bytes[1]<<8));
}

static void k_put_u32(uint8_t *bytes,u32 value) {
    for(unsigned byte=0;byte<4;byte++)
        bytes[byte]=(uint8_t)(value>>(8*byte));
}

static void k_print_bytes(const char *label,const uint8_t *bytes,unsigned size) {
    fprintf(stderr,"  %-8s",label);
    for(unsigned i=0;i<size;i++)
        fprintf(stderr," %02x",bytes[i]);
    fprintf(stderr,"\n");
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/k-model/bridge.c). */
static void k_load(unsigned variant,const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    k_io=sym("_sh2o_model_k_io");
    u32 table=sym("_sh2o_model_bridge"),entry=0;
    for(unsigned i=0;i<3;i++,entry++) {
        if(rl(&cpu,table+4*entry)!=sym(k_digest_symbols[i])) {
            fprintf(stderr,"k: %s: bridge entry %u is not %s\n",elf,entry,k_digest_symbols[i]);
            exit(2);
        }
    }
    for(unsigned path=0;path<K_PATHS;path++) {
        for(unsigned op=0;op<K_OP_COUNT;op++) {
            if(!k_has_path(variant,path,op))
                continue;
            const char *name=k_symbol(path,op);
            if(rl(&cpu,table+4*entry)!=sym(name)) {
                fprintf(stderr,"k: %s: bridge entry %u is %08x, symbol %s is %08x\n",
                    elf,entry,rl(&cpu,table+4*entry),name,sym(name));
                exit(2);
            }
            entry++;
        }
    }
}

/* Reads the three int64 results of a wrapper (big-endian in RAM) as the
 * oracle's little-endian lo:hi bytes. */
static void k_read_i64(u32 addr,unsigned count,uint8_t *out) {
    for(unsigned i=0;i<count;i++) {
        k_put_u32(out+8*i,rl(&cpu,addr+8*i+4));
        k_put_u32(out+8*i+4,rl(&cpu,addr+8*i));
    }
}

/* Writes the inputs of one vector into the I/O buffer (big-endian), calls the
 * wrapper with SR = sr and gives the result as the oracle's bytes. */
static struct cost k_call(unsigned path,const k_vector_t *vector,uint8_t *out,u32 sr) {
    const uint8_t *in=vector->in;
    u32 io=k_io+K_IO_IN,result=k_io+K_IO_OUT;
    const char *name=k_symbol(path,vector->op);
    struct cost c;
    for(unsigned i=0;i<6;i++)
        wl(&cpu,result+4*i,UINT32_C(0xa5a5a5a5));
    switch(vector->op) {
    case K_OP_DOT3B:
        for(unsigned i=0;i<3;i++) {
            ww(&cpu,io+2*i,k_get_u16(in+2*i));
            ww(&cpu,io+8+2*i,k_get_u16(in+6+2*i));
        }
        c=invoke_sr(name,io,io+8,k_get_u32(in+12),result,sr);
        k_read_i64(result,1,out);
        break;
    case K_OP_MVB3:
        for(unsigned i=0;i<5;i++)
            wl(&cpu,io+4*i,k_get_u32(in+4*i));
        for(unsigned i=0;i<3;i++) {
            ww(&cpu,io+20+2*i,k_get_u16(in+20+2*i));
            wl(&cpu,io+28+4*i,k_get_u32(in+26+4*i));
        }
        c=invoke_sr(name,io,io+20,io+28,result,sr);
        k_read_i64(result,3,out);
        break;
    case K_OP_GPF3:
        for(unsigned i=0;i<4;i++)
            wl(&cpu,io+4*i,k_get_u32(in+4*i));
        c=invoke_sr(name,io,result,0,0,sr);
        k_read_i64(result,3,out);
        break;
    case K_OP_GPL3:
        for(unsigned i=0;i<7;i++)
            wl(&cpu,io+4*i,k_get_u32(in+4*i));
        c=invoke_sr(name,io,io+16,in[28],result,sr);
        k_read_i64(result,3,out);
        break;
    case K_OP_CLZ32:
        c=invoke_sr(name,k_get_u32(in),0,0,0,sr);
        out[0]=(uint8_t)c.result;
        break;
    case K_OP_UNR_RECIP:
        c=invoke_sr(name,k_get_u32(in),0,0,0,sr);
        k_put_u32(out,c.result);
        break;
    case K_OP_UNR_DIV:
        c=invoke_sr(name,k_get_u32(in),k_get_u32(in+4),0,0,sr);
        k_put_u32(out,c.result);
        break;
    default:
        fprintf(stderr,"k: vector has unknown op %u\n",vector->op);
        exit(2);
    }
    return c;
}

/* Runs every vector on every path of one ELF. Returns 1 at the first difference. */
static int k_verify(unsigned variant,long plant) {
    for(size_t i=0;i<K_VECTOR_COUNT;i++) {
        const k_vector_t *vector=&k_vectors[i];
        uint8_t expected[K_OUT_MAX],out[K_OUT_MAX];
        memcpy(expected,vector->out,sizeof(expected));
        if(plant>=0&&(size_t)plant==i)
            expected[0]^=1;
        if(vector->op>=K_OP_COUNT) {
            fprintf(stderr,"k: vector has unknown op %u\n",vector->op);
            exit(2);
        }
        for(unsigned path=0;path<K_PATHS;path++) {
            if(!k_has_path(variant,path,vector->op))
                continue;
            struct cost c=k_call(path,vector,out,0);
            k_stats_t *s=&k_stats[variant][vector->op][path];
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
                fprintf(stderr,"k: %s: first difference in vector %zu (%s, %s), result byte %u\n",
                    k_variants[variant],i,k_op_names[vector->op],k_paths[path],byte);
                k_print_bytes("in",vector->in,vector->in_size);
                k_print_bytes("expected",expected,vector->out_size);
                k_print_bytes("got",out,vector->out_size);
                return 1;
            }
        }
    }
    return 0;
}

/* One digest as a chain of _part calls over vectors 0..n-1: symbol, which
 * (dot3b has none), and words state words seeded {SH2O_VEC_SEED, 811C9DC5h...}
 * in the I/O buffer. The state stays in the buffer from call to call. */
static k_total_t k_chain(const char *symbol,int which,u32 n,unsigned words) {
    u32 s=k_io+K_IO_DIGEST;
    k_total_t t={0,0};
    wl(&cpu,s,SH2O_VEC_SEED);
    for(unsigned i=1;i<words;i++)
        wl(&cpu,s+4*i,UINT32_C(0x811C9DC5));
    for(u32 first=0;first<n;first+=K_CHUNK) {
        u32 count=n-first<K_CHUNK?n-first:K_CHUNK;
        struct cost c=which<0?invoke(symbol,first,count,s,0):invoke(symbol,(u32)which,first,count,s);
        t.instructions+=c.instructions;
        t.cycles+=c.cycles;
        if(c.instructions>k_call_max)
            k_call_max=c.instructions;
    }
    return t;
}

/* One digest. want: the pin; mutant: the digest must not be the pin.
 * Returns 1 when the check fails. */
static int k_check(unsigned variant,const char *what,u32 got,u32 want,int mutant,k_total_t c) {
    int bad=mutant?got==want:got!=want;
    printf("k: %s: %-22s %08x %s %08x  %10" PRIu64 " instructions %10" PRIu64 " cycles%s\n",k_variants[variant],
        what,got,mutant?"pin":"want",want,c.instructions,c.cycles,bad?"  FAIL":"");
    if(bad)
        fprintf(stderr,"k: %s: %s: %s\n",k_variants[variant],what,
            mutant?"the mutant gives the pin: it is not caught":"the digest is not the pin");
    return bad;
}

/* The suite's digests against the pins of sh2o_k_test.h, and the native mutants. */
static int k_digests(unsigned variant) {
    unsigned whichs=variant==0?5u:2u;
    u32 h=k_io+K_IO_DIGEST;
    char what[64];
    int bad=0;
    k_total_t c=k_chain("_sh2o_k_test_dot3b_part",-1,SH2O_VEC_COUNT,2);
    bad|=k_check(variant,"dot3b",rl(&cpu,h+4),SH2O_DIGEST_DOT3B,0,c);
    for(unsigned which=0;which<whichs;which++) {
        c=k_chain("_sh2o_k_test_mvb3_part",(int)which,SH2O_VEC_COUNT,2);
        snprintf(what,sizeof(what),"mvb3 which %u",which);
        bad|=k_check(variant,what,rl(&cpu,h+4),SH2O_DIGEST_MVB3,which>=3,c);
    }
    for(unsigned which=0;which<whichs;which++) {
        c=k_chain("_sh2o_k_test_gp3_part",(int)which,SH2O_VEC_COUNT,3);
        snprintf(what,sizeof(what),"gpf3 which %u",which);
        bad|=k_check(variant,what,rl(&cpu,h+4),SH2O_DIGEST_GPF3,which>=3,c);
        snprintf(what,sizeof(what),"gpl3 which %u",which);
        bad|=k_check(variant,what,rl(&cpu,h+8),SH2O_DIGEST_GPL3,which>=3,c);
    }
    c=k_chain("_sh2o_k_test_gp3_part",0,SH2O_VEC_EDGE_TR0,3);
    bad|=k_check(variant,"gpf3 which 0 edge",rl(&cpu,h+4),SH2O_DIGEST_GPF3_EDGE,0,c);
    bad|=k_check(variant,"gpl3 which 0 edge",rl(&cpu,h+8),SH2O_DIGEST_GPL3_EDGE,0,c);
    printf("k: %s: most instructions of one _part call (%u vectors): %" PRIu64 "\n",k_variants[variant],K_CHUNK,
        k_call_max);
    k_call_max=0;
    return bad;
}

/* SR.S = 1: MAC.W then saturates the sum to 32 bits in MACL. The entry and
 * the _w2 kernels check SR.S, so any difference fails; the _w kernels assume
 * SR.S = 0, so for them this only counts. Then the cost of the check: the
 * mean cycles of _w2 less those of _w, both at SR.S = 0 on the same vectors.
 * Returns 1 when an entry or _w2 result differs. */
static int k_sr_s(void) {
    static const unsigned paths[3]={0,2,3};
    int bad=0;
    for(unsigned op=0;op<K_MAC_OPS;op++) {
        for(unsigned p=0;p<3;p++) {
            unsigned path=paths[p];
            size_t total=0,differ=0;
            for(size_t i=0;i<K_VECTOR_COUNT;i++) {
                const k_vector_t *vector=&k_vectors[i];
                uint8_t out[K_OUT_MAX];
                if(vector->op!=op)
                    continue;
                k_call(path,vector,out,2);
                total++;
                if(memcmp(out,vector->out,vector->out_size)) {
                    if(differ==0&&path!=2) {
                        fprintf(stderr,"k: native: SR.S 1: first difference in vector %zu (%s, %s)\n",i,
                            k_op_names[op],k_paths[path]);
                        k_print_bytes("in",vector->in,vector->in_size);
                        k_print_bytes("expected",vector->out,vector->out_size);
                        k_print_bytes("got",out,vector->out_size);
                    }
                    differ++;
                }
            }
            printf("k: native: SR.S 1: %-5s %-5s %zu of %zu differ%s\n",k_op_names[op],k_paths[path],differ,
                total,path==2?" (count only: SR.S-blind)":differ?"  FAIL":"");
            if(path!=2&&differ)
                bad=1;
        }
    }
    for(unsigned op=0;op<K_MAC_OPS;op++) {
        const k_stats_t *w=&k_stats[0][op][2],*w2=&k_stats[0][op][3];
        double mw=(double)w->sum_cycles/(double)w->count,mw2=(double)w2->sum_cycles/(double)w2->count;
        printf("k: native: SR.S 0: %-5s MODELLED cycles mean _w %.2f _w2 %.2f, SR.S check %+.2f (%" PRIu64
            " vectors)\n",k_op_names[op],mw,mw2,mw2-mw,w->count);
    }
    return bad;
}

int main(int argc,char **argv) {
    long plant=-1;
    if(argc==12&&!strcmp(argv[10],"--plant")) {
        char *end;
        plant=strtol(argv[11],&end,10);
        if(*end||plant<0||(size_t)plant>=K_VECTOR_COUNT) {
            fprintf(stderr,"k: --plant needs a vector from 0 to %zu\n",K_VECTOR_COUNT-1);
            return 2;
        }
    } else if(argc!=10) {
        fprintf(stderr,"usage: k_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
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
    int bad=0;
    for(unsigned variant=0;variant<2;variant++) {
        k_load(variant,argv[1+2*variant],argv[2+2*variant]);
        if(k_verify(variant,plant))
            return 1;
        printf("k: %s: %zu vectors verified on each path (%s)\n",k_variants[variant],K_VECTOR_COUNT,
            argv[1+2*variant]);
        bad|=k_digests(variant);
        if(variant==0)
            bad|=k_sr_s();
    }
    if(bad)
        return 1;
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-10s %-6s %7s %7s %9s %7s %7s %9s %7s\n","variant","op","path","cyc min","cyc max",
        "cyc mean","ins min","ins max","ins mean","vectors");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned op=0;op<K_OP_COUNT;op++) {
            for(unsigned path=0;path<K_PATHS;path++) {
                const k_stats_t *s=&k_stats[variant][op][path];
                if(!k_has_path(variant,path,op))
                    continue;
                printf("%-9s %-10s %-6s %7u %7u %9.2f %7" PRIu64 " %7" PRIu64 " %9.2f %7" PRIu64 "\n",
                    k_variants[variant],k_op_names[op],k_paths[path],s->min_cycles,s->max_cycles,
                    (double)s->sum_cycles/(double)s->count,s->min_instructions,s->max_instructions,
                    (double)s->sum_instructions/(double)s->count,s->count);
            }
        }
    }
    return 0;
}
