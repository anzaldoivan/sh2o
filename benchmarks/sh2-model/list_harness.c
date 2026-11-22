/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the list family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: list_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant vector | --plant-count]
 * Each ELF holds the library, benchmarks/list-model/bridge.c and
 * tests/suite/sh2o_list_test_suite.c. For each ELF:
 * 1. Every vector of tests/list_oracle.h runs through the library functions
 *    _sh2o_gte_write, _sh2o_gte_mvmva_list and _sh2o_gte_read, as
 *    tests/test_list.c does: the 61 words in the bridge's write order on a
 *    zeroed image in the bridge buffer, then the list of cmd on its n items,
 *    then the 16 out rows and the 64 reads. The fifth argument of the list
 *    (out) goes on the stack, at R15 of the call. The result bytes are
 *    compared with the oracle before any figure is printed. --plant flips one
 *    bit of a copy of the expected result of one vector, to show that the
 *    comparison fails.
 * 2. For each vector, the singles: from the same image, each item is stored
 *    in VXY0 and VZ0 (as src/sh2o_list.c does), then one _sh2o_gte_mvmva
 *    call. The out rows and the 64 reads of the singles must equal the
 *    vector's result.
 * 3. The suite counts are computed in the ELF (one _sh2o_list_test_counts
 *    call) and must be the pins of sh2o_list_test.h. The harness prints the
 *    instructions of the call. --plant-count compares the counts with the pins
 *    where the item count is XOR 1, to show that the comparison fails. A
 *    failure stops the run.
 * 4. Each oracle vector runs again with SR.S = 1. The library checks SR.S,
 *    so any result that differs fails the run.
 * 5. The MVMVA kernel path, at SR.S = 0: each oracle vector with cmd 12h,
 *    sf 1, mx < 3, cv != 2 and each T inside the range test of sh2oi_mac3
 *    (src/sh2o_gte.c). For each item, as in step 2: one _sh2o_gte_mvmva call
 *    (whole), whose out row (and, after the last item, the 64 reads) must
 *    equal the vector's result; then one _sh2o_model_list_mac3 call (kernel:
 *    sh2o_k_mvb3) on the same matrix words (the image from R base), V and T:
 *    each out[i] must equal (int64)T[i] << 12 + M[i].V, computed here from
 *    the image words, and out[i] >> 12 must equal the item's MAC(i+1). Any
 *    difference stops the run.
 * The figures are the cycles of one list item (the cycles of one
 * _sh2o_gte_mvmva_list call over its n items), of one _sh2o_gte_mvmva call,
 * and of the whole and kernel calls of step 5. */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/list_oracle.h"
#include "../../tests/suite/sh2o_list_test.h"

#define LIST_IO_IMAGE 0u    /* one sh2o_gte_t: 64 words */
#define LIST_IO_ITEMS 256u  /* v[16][2] */
#define LIST_IO_ROWS 384u   /* out[16][6] */
#define LIST_IO_COUNTS 768u /* the suite counts c[5] */
#define LIST_IO_V16 792u    /* step 5: the kernel's v[3] (int16) */
#define LIST_IO_T 800u      /* step 5: its t[3] */
#define LIST_IO_OUT 816u    /* step 5: its out[3] (int64) */
#define LIST_STACK (RAM_BASE+RAM_SIZE-64) /* R15 at the start of each invoke: the fifth argument */
#define LIST_LIMIT UINT64_C(100000000)

static const char *const list_variants[2]={"native","portable"};
static const char *const list_table_symbols[7]={"_sh2o_gte_write","_sh2o_gte_read","_sh2o_gte_mvmva",
    "_sh2o_gte_mvmva_list","_sh2o_list_test_counts","_sh2o_model_list_order","_sh2o_model_list_mac3"};
static const uint32_t list_pins[5]=SH2O_LIST_COUNTS;

typedef struct {
    double min_cycles,max_cycles,min_instructions,max_instructions,sum_cycles,sum_instructions;
    uint64_t count;
} list_stats_t;

static list_stats_t list_stats[2][2]; /* [variant][0: one list item, 1: one single _sh2o_gte_mvmva call] */
static list_stats_t list_mac3_stats[2][2]; /* step 5: [variant][0 whole _sh2o_gte_mvmva, 1 kernel _sh2o_model_list_mac3] */
static u32 list_io;
static unsigned list_order[SH2O_GTE_ORDER_COUNT];

static u32 list_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static void list_put_u32(uint8_t *bytes,u32 value) {
    for(unsigned byte=0;byte<4;byte++)
        bytes[byte]=(uint8_t)(value>>(8*byte));
}

static void list_add(list_stats_t *s,double cycles,double instructions) {
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
 * Then it checks the bridge table (layout: benchmarks/list-model/bridge.c) and
 * reads the write order from the ELF. */
static void list_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    list_io=sym("_sh2o_model_list_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<7;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(list_table_symbols[entry])) {
            fprintf(stderr,"list: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),list_table_symbols[entry],sym(list_table_symbols[entry]));
            exit(2);
        }
    }
    u32 order=sym("_sh2o_model_list_order");
    for(unsigned k=0;k<SH2O_GTE_ORDER_COUNT;k++) {
        list_order[k]=rb(&cpu,order+k);
        if(list_order[k]>63) {
            fprintf(stderr,"list: %s: sh2o_model_list_order[%u] is %u\n",elf,k,list_order[k]);
            exit(2);
        }
    }
}

/* Writes the input image of one vector with SR = sr and gives its n. */
static unsigned list_image(const list_vector_t *vector,u32 sr) {
    const uint8_t *in=vector->in;
    u32 g=list_io+LIST_IO_IMAGE;
    unsigned n=(unsigned)list_get_u32(in+260);
    if(vector->op!=LIST_OP_LIST||n>SH2O_LIST_MAX) {
        fprintf(stderr,"list: vector has unknown op %u or n %u\n",vector->op,n);
        exit(2);
    }
    for(unsigned r=0;r<64;r++)
        wl(&cpu,g+4*r,0);
    for(unsigned k=0;k<SH2O_GTE_ORDER_COUNT;k++)
        invoke_sr("_sh2o_gte_write",g,list_order[k],list_get_u32(in+4*list_order[k]),0,sr);
    return n;
}

/* Runs one vector with SR = sr and gives its result as the oracle's bytes. *cost: the list call. */
static unsigned list_run(const list_vector_t *vector,uint8_t *out,u32 sr,struct cost *cost) {
    const uint8_t *in=vector->in;
    u32 g=list_io+LIST_IO_IMAGE,items=list_io+LIST_IO_ITEMS,rows=list_io+LIST_IO_ROWS;
    unsigned n=list_image(vector,sr);
    for(unsigned k=0;k<2*SH2O_LIST_MAX;k++)
        wl(&cpu,items+4*k,list_get_u32(in+264+4*k));
    for(unsigned k=0;k<6*SH2O_LIST_MAX;k++)
        wl(&cpu,rows+4*k,0);
    wl(&cpu,LIST_STACK,rows);
    *cost=invoke_sr("_sh2o_gte_mvmva_list",g,list_get_u32(in+256),items,n,sr);
    for(unsigned k=0;k<6*SH2O_LIST_MAX;k++)
        list_put_u32(out+4*k,rl(&cpu,rows+4*k));
    for(unsigned r=0;r<64;r++)
        list_put_u32(out+384+4*r,invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result);
    return n;
}

/* The singles of one vector: gives their result as the oracle's bytes, and adds each call to the figures. */
static void list_singles(unsigned variant,const list_vector_t *vector,uint8_t *out) {
    static const unsigned out_regs[6]={9,10,11,25,26,27};
    const uint8_t *in=vector->in;
    u32 g=list_io+LIST_IO_IMAGE,cmd=list_get_u32(in+256);
    unsigned n=list_image(vector,0);
    memset(out,0,LIST_OUT_MAX);
    for(unsigned k=0;k<n;k++) {
        wl(&cpu,g,list_get_u32(in+264+8*k));
        wl(&cpu,g+4,list_get_u32(in+268+8*k));
        struct cost c=invoke("_sh2o_gte_mvmva",g,cmd,0,0);
        list_add(&list_stats[variant][1],(double)c.cycles,(double)c.instructions);
        for(unsigned col=0;col<6;col++)
            list_put_u32(out+24*k+4*col,invoke("_sh2o_gte_read",g,out_regs[col],0,0).result);
    }
    for(unsigned r=0;r<64;r++)
        list_put_u32(out+384+4*r,invoke("_sh2o_gte_read",g,r,0,0).result);
}

/* Runs every vector and its singles on one ELF. Returns 1 at the first difference. */
static int list_verify(unsigned variant,long plant) {
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        uint8_t expected[LIST_OUT_MAX],out[LIST_OUT_MAX];
        struct cost c;
        memcpy(expected,vector->out,sizeof(expected));
        if(plant>=0&&(size_t)plant==i)
            expected[0]^=1;
        unsigned n=list_run(vector,out,0,&c);
        if(n>0)
            list_add(&list_stats[variant][0],(double)c.cycles/n,(double)c.instructions/n);
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            if(out[byte]==expected[byte])
                continue;
            fprintf(stderr,"list: %s: first difference in vector %zu (cmd %08x, n %u), result byte %u: "
                "expected %02x, got %02x\n",list_variants[variant],i,list_get_u32(vector->in+256),n,byte,
                expected[byte],out[byte]);
            return 1;
        }
    }
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        uint8_t out[LIST_OUT_MAX];
        list_singles(variant,vector,out);
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            if(out[byte]==vector->out[byte])
                continue;
            fprintf(stderr,"list: %s: the singles of vector %zu differ from the list, result byte %u: "
                "expected %02x, got %02x\n",list_variants[variant],i,byte,vector->out[byte],out[byte]);
            return 1;
        }
    }
    return 0;
}

/* The suite counts in the ELF against the pins. Returns 1 when a check fails. */
static int list_counts(unsigned variant,int plant_count) {
    u32 c=list_io+LIST_IO_COUNTS,got[5],want[5];
    int bad=0;
    for(unsigned k=0;k<5;k++)
        wl(&cpu,c+4*k,0);
    struct cost cost=invoke("_sh2o_list_test_counts",c,0,0,0);
    for(unsigned k=0;k<5;k++) {
        got[k]=rl(&cpu,c+4*k);
        want[k]=list_pins[k]^(plant_count&&k==1?1u:0u);
    }
    printf("list: %s: suite: %u runs, %u items, %u prefixes differ, reorder caught %u of %u  %10" PRIu64
        " instructions %10u cycles\n",list_variants[variant],got[0],got[1],got[2],got[3],got[4],cost.instructions,
        cost.cycles);
    if(cost.instructions>=LIST_LIMIT) {
        fprintf(stderr,"list: %s: the counts call is at the instruction limit\n",list_variants[variant]);
        bad=1;
    }
    if(memcmp(got,want,sizeof got)) {
        fprintf(stderr,"list: %s: the counts are not the pins\n",list_variants[variant]);
        bad=1;
    }
    return bad;
}

/* SR.S = 1. The library checks SR.S, so any result that differs fails. Returns 1 on a difference. */
static int list_sr_s(unsigned variant) {
    size_t differ=0;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        uint8_t out[LIST_OUT_MAX];
        struct cost c;
        list_run(vector,out,2,&c);
        if(memcmp(out,vector->out,vector->out_size))
            differ++;
    }
    printf("list: %s: SR.S 1: %zu of %zu differ%s\n",list_variants[variant],differ,(size_t)LIST_VECTOR_COUNT,
        differ?"  FAIL":"");
    return differ!=0;
}

/* The MVMVA kernel path: cmd 12h, sf 1, mx < 3, cv != 2 and each t inside the range test of sh2oi_mac3
 * (src/sh2o_gte.c). Gives 0 for any other word; else 1, with the matrix's first word r, V and T as
 * sh2o_gte_mvmva takes them from the image at g, and want[i] = (int64)T[i] << 12 + M[i].V. */
static int list_mac3_input(u32 g,u32 cmd,unsigned *r,int16_t v[3],int32_t t[3],int64_t want[3]) {
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

/* Step 5: the whole and kernel calls of each item of each kernel-path vector, at SR.S = 0. The items run in turn
 * on one image, as in list_singles; T does not change between items, so the path is the vector's. */
static int list_mac3(unsigned variant) {
    static const unsigned out_regs[6]={9,10,11,25,26,27};
    u32 g=list_io+LIST_IO_IMAGE,vp=list_io+LIST_IO_V16,tp=list_io+LIST_IO_T,op=list_io+LIST_IO_OUT;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        const uint8_t *in=vector->in;
        u32 cmd=list_get_u32(in+256);
        unsigned n=list_image(vector,0),r;
        int16_t v[3];
        int32_t t[3];
        int64_t want[3];
        if(!list_mac3_input(g,cmd,&r,v,t,want))
            continue;
        for(unsigned item=0;item<n;item++) {
            wl(&cpu,g,list_get_u32(in+264+8*item));
            wl(&cpu,g+4,list_get_u32(in+268+8*item));
            if(!list_mac3_input(g,cmd,&r,v,t,want)) {
                fprintf(stderr,"list: %s: MVMVA kernel path: vector %zu item %u leaves the path\n",
                    list_variants[variant],i,item);
                return 1;
            }
            struct cost c=invoke_sr("_sh2o_gte_mvmva",g,cmd,0,0,0);
            for(unsigned col=0;col<6;col++) {
                u32 got=invoke_sr("_sh2o_gte_read",g,out_regs[col],0,0,0).result,
                    expect=list_get_u32(vector->out+24*item+4*col);
                if(got==expect)
                    continue;
                fprintf(stderr,"list: %s: MVMVA kernel path: whole differs from the result of vector %zu (word %08x), "
                    "item %u, register %u: result %08x, whole %08x\n",list_variants[variant],i,cmd,item,out_regs[col],
                    expect,got);
                return 1;
            }
            list_add(&list_mac3_stats[variant][0],(double)c.cycles,(double)c.instructions);
            for(unsigned k=0;k<3;k++) {
                ww(&cpu,vp+2*k,(u16)v[k]);
                wl(&cpu,tp+4*k,(u32)t[k]);
                wl(&cpu,op+8*k,0);
                wl(&cpu,op+8*k+4,0);
            }
            c=invoke_sr("_sh2o_model_list_mac3",g+4*r,vp,tp,op,0);
            for(unsigned k=0;k<3;k++) {
                int64_t got=(int64_t)((uint64_t)rl(&cpu,op+8*k)<<32|rl(&cpu,op+8*k+4));
                u32 mac=list_get_u32(vector->out+24*item+4*(3+k));
                if(got==want[k]&&(u32)(int32_t)(got>>12)==mac)
                    continue;
                fprintf(stderr,"list: %s: MVMVA kernel path: kernel differs in vector %zu (word %08x), item %u, "
                    "out[%u]: %016" PRIx64 ", T*1000h + M*V %016" PRIx64 ", MAC%u %08x\n",list_variants[variant],i,
                    cmd,item,k,(uint64_t)got,(uint64_t)want[k],k+1,mac);
                return 1;
            }
            list_add(&list_mac3_stats[variant][1],(double)c.cycles,(double)c.instructions);
        }
        for(unsigned reg=0;reg<64;reg++) {
            u32 got=invoke_sr("_sh2o_gte_read",g,reg,0,0,0).result,expect=list_get_u32(vector->out+384+4*reg);
            if(got==expect)
                continue;
            fprintf(stderr,"list: %s: MVMVA kernel path: whole differs from the result of vector %zu (word %08x), "
                "register %u after the last item: result %08x, whole %08x\n",list_variants[variant],i,cmd,reg,expect,got);
            return 1;
        }
    }
    if(list_mac3_stats[variant][0].count==0) {
        fprintf(stderr,"list: %s: MVMVA kernel path: no oracle item takes it\n",list_variants[variant]);
        return 1;
    }
    printf("list: %s: MVMVA kernel path, SR.S 0: %" PRIu64 " items: whole == vector result, kernel == T*1000h + M*V "
        "and MAC1-3: 0 differ\n",list_variants[variant],list_mac3_stats[variant][0].count);
    return 0;
}

int main(int argc,char **argv) {
    long plant=-1;
    int plant_count=0;
    if(argc==12&&!strcmp(argv[10],"--plant")) {
        char *end;
        plant=strtol(argv[11],&end,10);
        if(*end||plant<0||(size_t)plant>=LIST_VECTOR_COUNT) {
            fprintf(stderr,"list: --plant needs a vector from 0 to %zu\n",LIST_VECTOR_COUNT-1);
            return 2;
        }
    } else if(argc==11&&!strcmp(argv[10],"--plant-count")) {
        plant_count=1;
    } else if(argc!=10) {
        fprintf(stderr,"usage: list_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
            "yabause-sha yaul-image native-sha256 portable-sha256 [--plant vector | --plant-count]\n");
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
        list_load(argv[1+2*variant],argv[2+2*variant]);
        if(list_verify(variant,plant))
            return 1;
        printf("list: %s: %zu vectors exact, singles equal (%s)\n",list_variants[variant],
            (size_t)LIST_VECTOR_COUNT,argv[1+2*variant]);
        if(list_counts(variant,plant_count))
            return 1;
        if(list_sr_s(variant))
            return 1;
        if(list_mac3(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. One list item (one sh2o_gte_mvmva_list call / n, over the oracle vectors) and one\n");
    printf("sh2o_gte_mvmva call (the singles of the oracle vectors).\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-13s %9s %9s %9s %9s %9s %9s %7s\n","variant","call","cyc min","cyc max","cyc mean","ins min",
        "ins max","ins mean","count");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned kind=0;kind<2;kind++) {
            const list_stats_t *s=&list_stats[variant][kind];
            printf("%-9s %-13s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %7" PRIu64 "\n",list_variants[variant],
                kind?"mvmva single":"list item",s->min_cycles,s->max_cycles,s->sum_cycles/(double)s->count,
                s->min_instructions,s->max_instructions,s->sum_instructions/(double)s->count,s->count);
        }
    }
    printf("\nMVMVA kernel path, SR.S 0: the items of the oracle vectors with cmd 12h, sf 1, mx < 3, cv != 2 and T\n");
    printf("inside the kernel's range. Row whole: one sh2o_gte_mvmva call (as the singles). Row kernel: one\n");
    printf("sh2o_model_list_mac3 call on the same inputs (sh2o_k_mvb3 and the wrapper's call). Overhead: whole mean -\n");
    printf("kernel mean.\n");
    printf("\n%-9s %-13s %9s %9s %9s %7s\n","variant","call","cyc min","cyc max","cyc mean","inputs");
    for(unsigned variant=0;variant<2;variant++) {
        const list_stats_t *w=&list_mac3_stats[variant][0],*k=&list_mac3_stats[variant][1];
        double whole=w->sum_cycles/(double)w->count,kernel=k->sum_cycles/(double)k->count;
        printf("%-9s %-13s %9.2f %9.2f %9.2f %7" PRIu64 "\n",list_variants[variant],"whole",w->min_cycles,
            w->max_cycles,whole,w->count);
        printf("%-9s %-13s %9.2f %9.2f %9.2f %7" PRIu64 "\n",list_variants[variant],"kernel",k->min_cycles,
            k->max_cycles,kernel,k->count);
        printf("%-9s %-13s %29.2f  %.1f%% of whole\n",list_variants[variant],"overhead",whole-kernel,
            100.0*(whole-kernel)/whole);
    }
    return 0;
}
