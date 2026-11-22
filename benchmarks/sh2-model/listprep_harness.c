/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the listprep family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: listprep_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant-known k]
 * Each ELF holds the library and benchmarks/listprep-model/bridge.c. The
 * listprep family has no oracle file of its own: its known answers are the
 * vectors of tests/list_oracle.h. For each ELF:
 * 1. Each vector's 61 words are written through _sh2o_gte_write in the write
 *    order of tests/test_list.c on a zeroed image, then
 *    _sh2o_gte_mvmva_list_prepared runs on its n items (the fifth argument,
 *    out, on the stack at R15 of the call). The 16 out rows and the 64 words
 *    of _sh2o_gte_read must equal the vector's result. Two runs: SR.S 0 and
 *    SR.S 1 at the writes, the call and the reads. The count of vectors and
 *    of vectors in the gate (n > 0, sf 1, v 0, mx < 3, cv != 2, each t inside
 *    |t| < 7FF40000h; computed here) are pins. --plant-known k flips bit 0 of
 *    a copy of the first result byte of vector k, to show that the comparison
 *    fails.
 * 2. The random lists of tests/test_list_prepared.c as a chain of
 *    _sh2o_model_listprep_check_part calls, at SR.S 0 and at SR.S 1: no run
 *    may differ from sh2o_gte_mvmva_list; the runs in the gate and the swapped
 *    inputs caught are pins (native SR.S 1: the fallback, the same counts).
 * All comparisons come before any figure; one difference stops the run
 * (exit 1).
 * The figures (step 3), at SR.S 0: the whole call of _sh2o_gte_mvmva_list
 * (today) and of _sh2o_gte_mvmva_list_prepared (arm C, its load included) for
 * cmd 4A486012h (RT, V0, no translation, sf 1) at n = 1..8 over
 * LP_TIME_IMAGES images and items drawn here (each in the gate), and over the
 * oracle vectors with n > 0 (all, and those in the gate); per item = whole / n.
 * Each pair of calls starts from the same image; the out rows and the 64
 * words of the two must be equal. The load row: one _sh2o_gte_prepared_load
 * call with parts RT|NONE (the parts of 4A486012h) on each timing image; its
 * ok[2] must be 1. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/list_oracle.h"

#define LP_IO_G0 0u       /* image G0 */
#define LP_IO_G1 256u     /* image G1 */
#define LP_IO_ITEMS 512u  /* v[16][2] */
#define LP_IO_ROWS0 640u  /* out[16][6] of G0's list */
#define LP_IO_ROWS1 1024u /* out[16][6] of G1's list */
#define LP_IO_SHADOW 1408u
#define LP_IO_STATE 1600u /* the check state s[5] */
#define LP_STACK (RAM_BASE+RAM_SIZE-64) /* R15 at the start of each invoke: the fifth argument */
#define LP_MAX 16u
#define LP_ORDER_COUNT 61u
#define LP_VECTOR_PIN 88u
#define LP_GATE_PIN 10u      /* oracle vectors in the gate */
#define LP_RUNS 1024u
#define LP_SEED UINT32_C(0x53504B32)
#define LP_ACCEPT_PIN 587u   /* random runs in the gate */
#define LP_CAUGHT_PIN 587u   /* of them, swapped items caught */
#define LP_CHUNK 128u
#define LP_LIMIT UINT64_C(100000000)
#define LP_TIME_CMD UINT32_C(0x4A486012)
#define LP_TIME_PARTS 33u    /* RT | NONE */
#define LP_TIME_IMAGES 32u
#define LP_TIME_SEED UINT32_C(0x4C503031)

static const char *const lp_variants[2]={"native","portable"};
static const char *const lp_table_symbols[6]={"_sh2o_gte_write","_sh2o_gte_read","_sh2o_gte_mvmva_list",
    "_sh2o_gte_mvmva_list_prepared","_sh2o_gte_prepared_load","_sh2o_model_listprep_check_part"};
/* The write order of tests/test_list.c (the gte suite's order). */
static const unsigned char lp_order[LP_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,
    23,24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,
    63};

typedef struct {
    double sum_cycles,sum_instructions;
    uint64_t count;
} lp_stats_t;

/* [variant][n 1..8][0 today, 1 arm C] whole calls; [variant][0 all, 1 in the gate][0 today, 1 arm C] per item */
static lp_stats_t lp_n_stats[2][9][2];
static lp_stats_t lp_oracle_stats[2][2][2];
static lp_stats_t lp_load_stats[2];
static u32 lp_io;

static u32 lp_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static void lp_put_u32(uint8_t *bytes,u32 value) {
    for(unsigned byte=0;byte<4;byte++)
        bytes[byte]=(uint8_t)(value>>(8*byte));
}

static u32 lp_lcg(u32 *x) {
    *x=*x*1664525u+1013904223u;
    return *x;
}

static void lp_add(lp_stats_t *s,double cycles,double instructions) {
    s->sum_cycles+=cycles;
    s->sum_instructions+=instructions;
    s->count++;
}

static double lp_mean(const lp_stats_t *s) {
    return s->sum_cycles/(double)s->count;
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/listprep-model/bridge.c). */
static void lp_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    lp_io=sym("_sh2o_model_listprep_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<6;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(lp_table_symbols[entry])) {
            fprintf(stderr,"listprep: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),lp_table_symbols[entry],sym(lp_table_symbols[entry]));
            exit(2);
        }
    }
}

/* Writes the input image of one vector at g with SR = sr, its items, zeroed rows; gives its n. */
static unsigned lp_image(const list_vector_t *vector,u32 g,u32 rows,u32 sr) {
    const uint8_t *in=vector->in;
    u32 items=lp_io+LP_IO_ITEMS;
    unsigned n=(unsigned)lp_get_u32(in+260);
    if(vector->op!=LIST_OP_LIST||n>LP_MAX) {
        fprintf(stderr,"listprep: vector has unknown op %u or n %u\n",vector->op,n);
        exit(2);
    }
    for(unsigned r=0;r<64;r++)
        wl(&cpu,g+4*r,0);
    for(unsigned k=0;k<LP_ORDER_COUNT;k++)
        invoke_sr("_sh2o_gte_write",g,lp_order[k],lp_get_u32(in+4*lp_order[k]),0,sr);
    for(unsigned k=0;k<2*LP_MAX;k++)
        wl(&cpu,items+4*k,lp_get_u32(in+264+4*k));
    for(unsigned k=0;k<6*LP_MAX;k++)
        wl(&cpu,rows+4*k,0);
    return n;
}

/* One list call (name) on image g with the items in the buffer, n items, rows at the stack; SR = sr. */
static struct cost lp_call(const char *name,u32 g,u32 cmd,unsigned n,u32 rows,u32 sr) {
    wl(&cpu,LP_STACK,rows);
    return invoke_sr(name,g,cmd,lp_io+LP_IO_ITEMS,n,sr);
}

/* 1 when cmd on the image at g is in the gate (without SR.S) */
static int lp_gate(u32 g,u32 cmd) {
    unsigned mx=cmd>>17&3u,cv=cmd>>13&3u;
    if(!(cmd&0x80000u)||(cmd>>15&3u)||mx==3u||cv==2u)
        return 0;
    if(cv==3u)
        return 1;
    for(unsigned i=0;i<3;i++)
        if(!(rl(&cpu,g+4*((cv?45u:37u)+i))+0x7FF3FFFFu<0xFFE7FFFFu))
            return 0;
    return 1;
}

/* Step 1 at SR = sr. Returns 1 at the first difference or when a count is not its pin. */
static int lp_exact(unsigned variant,u32 sr,long plant_known) {
    u32 g=lp_io+LP_IO_G0,rows=lp_io+LP_IO_ROWS0;
    unsigned gated=0,k=0;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++,k++) {
        const list_vector_t *vector=&list_vectors[i];
        uint8_t expected[LIST_OUT_MAX],out[LIST_OUT_MAX];
        u32 cmd=lp_get_u32(vector->in+256);
        memcpy(expected,vector->out,sizeof(expected));
        if(plant_known>=0&&(size_t)plant_known==i)
            expected[0]^=1;
        unsigned n=lp_image(vector,g,rows,sr);
        gated+=(unsigned)(n>0&&lp_gate(g,cmd));
        lp_call("_sh2o_gte_mvmva_list_prepared",g,cmd,n,rows,sr);
        for(unsigned w=0;w<6*LP_MAX;w++)
            lp_put_u32(out+4*w,rl(&cpu,rows+4*w));
        for(unsigned r=0;r<64;r++)
            lp_put_u32(out+384+4*r,invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result);
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            if(out[byte]==expected[byte])
                continue;
            fprintf(stderr,"listprep: %s: SR.S %u: the known answer of vector %zu (cmd %08x, n %u) does not hold: "
                "result byte %u expected %02x, got %02x\n",lp_variants[variant],sr>>1,i,cmd,n,byte,expected[byte],
                out[byte]);
            return 1;
        }
    }
    printf("listprep: %s: SR.S %u: %u list vectors exact, %u in the gate (pins %u, %u)\n",lp_variants[variant],
        sr>>1,k,gated,LP_VECTOR_PIN,LP_GATE_PIN);
    if(k!=LP_VECTOR_PIN||gated!=LP_GATE_PIN) {
        fprintf(stderr,"listprep: %s: SR.S %u: the counts are not the pins\n",lp_variants[variant],sr>>1);
        return 1;
    }
    return 0;
}

/* Step 2 at SR = sr. Returns 1 when a run differs or a count is not its pin. */
static int lp_check(unsigned variant,u32 sr) {
    u32 s=lp_io+LP_IO_STATE;
    uint64_t most=0;
    wl(&cpu,s,LP_SEED);
    for(unsigned i=1;i<5;i++)
        wl(&cpu,s+4*i,0);
    for(u32 done=0;done<LP_RUNS;done+=LP_CHUNK) {
        struct cost c=invoke_sr("_sh2o_model_listprep_check_part",LP_CHUNK,s,0,0,sr);
        if(c.instructions>most)
            most=c.instructions;
    }
    u32 runs=rl(&cpu,s+4),accepted=rl(&cpu,s+8),differ=rl(&cpu,s+12),caught=rl(&cpu,s+16);
    printf("listprep: %s: SR.S %u: random: %u runs, %u differ, %u in the gate (pin %u), swapped items caught %u "
        "(pin %u), most instructions of one _part call (%u runs): %" PRIu64 "\n",lp_variants[variant],sr>>1,runs,
        differ,accepted,LP_ACCEPT_PIN,caught,LP_CAUGHT_PIN,LP_CHUNK,most);
    if(most>=LP_LIMIT) {
        fprintf(stderr,"listprep: %s: a random _part call is at the instruction limit\n",lp_variants[variant]);
        return 1;
    }
    if(differ) {
        fprintf(stderr,"listprep: %s: SR.S %u: random: %u runs differ from sh2o_gte_mvmva_list\n",
            lp_variants[variant],sr>>1,differ);
        return 1;
    }
    if(runs!=LP_RUNS||accepted!=LP_ACCEPT_PIN||caught!=LP_CAUGHT_PIN) {
        fprintf(stderr,"listprep: %s: SR.S %u: random: the counts are not the pins\n",lp_variants[variant],sr>>1);
        return 1;
    }
    return 0;
}

/* 1 when the out rows (n) and the 64 words of G0 and G1 are equal; else 0, with the first word that differs. */
static int lp_same(unsigned n,unsigned *word,u32 *a,u32 *b) {
    for(unsigned w=0;w<6*n;w++) {
        *a=rl(&cpu,lp_io+LP_IO_ROWS0+4*w);
        *b=rl(&cpu,lp_io+LP_IO_ROWS1+4*w);
        if(*a!=*b) {
            *word=w;
            return 0;
        }
    }
    for(unsigned r=0;r<64;r++) {
        *a=rl(&cpu,lp_io+LP_IO_G0+4*r);
        *b=rl(&cpu,lp_io+LP_IO_G1+4*r);
        if(*a!=*b) {
            *word=6*LP_MAX+r;
            return 0;
        }
    }
    return 1;
}

/* Step 3 at SR.S 0. Returns 1 at the first difference. */
static int lp_timing(unsigned variant) {
    u32 g0=lp_io+LP_IO_G0,g1=lp_io+LP_IO_G1,r0=lp_io+LP_IO_ROWS0,r1=lp_io+LP_IO_ROWS1,p=lp_io+LP_IO_SHADOW,
        items=lp_io+LP_IO_ITEMS,a,b;
    unsigned word;
    /* cmd 4A486012h at n = 1..8 */
    for(unsigned n=1;n<=8;n++) {
        u32 x=LP_TIME_SEED;
        for(unsigned img=0;img<LP_TIME_IMAGES;img++) {
            struct cost c;
            for(unsigned r=0;r<64;r++) {
                u32 w=lp_lcg(&x);
                wl(&cpu,g0+4*r,w);
                wl(&cpu,g1+4*r,w);
            }
            for(unsigned k=0;k<2*LP_MAX;k++)
                wl(&cpu,items+4*k,k<2*n?lp_lcg(&x):0u);
            for(unsigned k=0;k<6*LP_MAX;k++) {
                wl(&cpu,r0+4*k,0);
                wl(&cpu,r1+4*k,0);
            }
            if(!lp_gate(g0,LP_TIME_CMD)) {
                fprintf(stderr,"listprep: %s: timing: image %u is not in the gate\n",lp_variants[variant],img);
                return 1;
            }
            c=lp_call("_sh2o_gte_mvmva_list",g0,LP_TIME_CMD,n,r0,0);
            lp_add(&lp_n_stats[variant][n][0],(double)c.cycles,(double)c.instructions);
            c=lp_call("_sh2o_gte_mvmva_list_prepared",g1,LP_TIME_CMD,n,r1,0);
            lp_add(&lp_n_stats[variant][n][1],(double)c.cycles,(double)c.instructions);
            if(!lp_same(n,&word,&a,&b)) {
                fprintf(stderr,"listprep: %s: timing: n %u, image %u: word %u: today %08x, arm C %08x\n",
                    lp_variants[variant],n,img,word,a,b);
                return 1;
            }
            if(n==1) {
                /* the load of the parts the call loads, on the image (MVMVA leaves the matrix words) */
                c=invoke_sr("_sh2o_gte_prepared_load",p,g0,LP_TIME_PARTS,0,0);
                if(rb(&cpu,p+180+2)!=1u) {
                    fprintf(stderr,"listprep: %s: timing: load: ok[2] is not 1\n",lp_variants[variant]);
                    return 1;
                }
                lp_add(&lp_load_stats[variant],(double)c.cycles,(double)c.instructions);
            }
        }
    }
    /* the oracle vectors with n > 0 */
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        u32 cmd=lp_get_u32(vector->in+256);
        struct cost c0,c1;
        unsigned n=lp_image(vector,g0,r0,0);
        if(n==0)
            continue;
        int gate=lp_gate(g0,cmd);
        c0=lp_call("_sh2o_gte_mvmva_list",g0,cmd,n,r0,0);
        (void)lp_image(vector,g1,r1,0);
        c1=lp_call("_sh2o_gte_mvmva_list_prepared",g1,cmd,n,r1,0);
        if(!lp_same(n,&word,&a,&b)) {
            fprintf(stderr,"listprep: %s: timing: vector %zu: word %u: today %08x, arm C %08x\n",lp_variants[variant],
                i,word,a,b);
            return 1;
        }
        for(unsigned set=0;set<2;set++) {
            if(set&&!gate)
                continue;
            lp_add(&lp_oracle_stats[variant][set][0],(double)c0.cycles/n,(double)c0.instructions/n);
            lp_add(&lp_oracle_stats[variant][set][1],(double)c1.cycles/n,(double)c1.instructions/n);
        }
    }
    printf("listprep: %s: timing, SR.S 0: n 1..8 x %u images and %" PRIu64 " oracle vectors: today == arm C: 0 "
        "differ\n",lp_variants[variant],LP_TIME_IMAGES,lp_oracle_stats[variant][0][0].count);
    return 0;
}

int main(int argc,char **argv) {
    long plant_known=-1;
    if(argc==12&&!strcmp(argv[10],"--plant-known")) {
        char *end;
        plant_known=strtol(argv[11],&end,10);
        if(*end||plant_known<0||plant_known>=(long)LP_VECTOR_PIN) {
            fprintf(stderr,"listprep: --plant-known needs a vector from 0 to %u\n",LP_VECTOR_PIN-1);
            return 2;
        }
    } else if(argc!=10) {
        fprintf(stderr,"usage: listprep_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant-known k]\n");
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
        lp_load(argv[1+2*variant],argv[2+2*variant]);
        printf("listprep: %s: %s\n",lp_variants[variant],argv[1+2*variant]);
        if(lp_exact(variant,0,plant_known)||lp_exact(variant,2,plant_known))
            return 1;
        if(lp_check(variant,0)||lp_check(variant,2))
            return 1;
        if(lp_timing(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. SR.S 0. Whole: one list call over its n items; today: sh2o_gte_mvmva_list; arm C:\n");
    printf("sh2o_gte_mvmva_list_prepared, its load included. Per item: whole / n. cmd %08x over %u images\n",
        LP_TIME_CMD,LP_TIME_IMAGES);
    printf("and items drawn from seed %08x (each in the gate); the oracle rows over the vectors of\n",LP_TIME_SEED);
    printf("tests/list_oracle.h with n > 0 (all, and those in the gate), mean of each vector's cycles / n.\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    for(unsigned variant=0;variant<2;variant++) {
        const lp_stats_t *ld=&lp_load_stats[variant];
        unsigned even=0;
        printf("\n%-9s %-14s %10s %10s %10s %10s %10s\n",lp_variants[variant],"n","today","arm C","today/it",
            "arm C/it","C ins");
        for(unsigned n=1;n<=8;n++) {
            const lp_stats_t *s=lp_n_stats[variant][n];
            double t=lp_mean(&s[0]),c=lp_mean(&s[1]);
            if(!even&&c<t)
                even=n;
            printf("%-9s %-14u %10.2f %10.2f %10.2f %10.2f %10.2f\n",lp_variants[variant],n,t,c,t/n,c/n,
                s[1].sum_instructions/(double)s[1].count);
        }
        for(unsigned set=0;set<2;set++) {
            const lp_stats_t *s=lp_oracle_stats[variant][set];
            char label[32];
            snprintf(label,sizeof(label),set?"oracle gate %" PRIu64:"oracle %" PRIu64,s[0].count);
            printf("%-9s %-14s %10s %10s %10.2f %10.2f %10s\n",lp_variants[variant],label,"","",lp_mean(&s[0]),
                lp_mean(&s[1]),"");
        }
        printf("%-9s load RT|NONE: %.2f cycles (%.2f instructions), %" PRIu64 " images\n",lp_variants[variant],
            lp_mean(ld),ld->sum_instructions/(double)ld->count,ld->count);
        if(even)
            printf("%-9s break-even n: %u (the first n where arm C's whole call is under today's)\n",
                lp_variants[variant],even);
        else
            printf("%-9s break-even n: none in 1..8\n",lp_variants[variant]);
    }
    return 0;
}
