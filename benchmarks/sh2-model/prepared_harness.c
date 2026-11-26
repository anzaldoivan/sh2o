/* SPDX-License-Identifier: GPL-2.0-or-later
 * Executed-instruction checks and cycle model for the prepared family.
 * Copyright (c) 2026 sh2o Contributors.
 * This harness is licensed apart from the sh2o library (MIT): it includes
 * SH2ZAM's GPL model harness, which builds Yabause's GPL SH-2 interpreter.
 * See LICENSE in this folder. It is never linked into libsh2o or installed.
 * Flat RAM, no caches, wait states or bus contention: MODELLED cycles, not
 * Saturn timing.
 *
 * usage: prepared_harness native.elf native.sym portable.elf portable.sym
 *            sh2zam-sha yabause-sha yaul-image native-sha256 portable-sha256
 *            [--plant-known k]
 * Each ELF holds the library and benchmarks/prepared-model/bridge.c. The
 * prepared family has no oracle file of its own: its known answers are the
 * MVMVA (op 12h) cmd vectors of tests/gte_oracle.h. For each ELF:
 * 1. Each MVMVA vector's 64 registers are written through _sh2o_gte_write in
 *    the write order of tests/suite/sh2o_gte_test_suite.c on a zeroed image;
 *    _sh2o_gte_prepared_load loads all parts of the shadow from that image;
 *    _sh2o_gte_mvmva_prepared runs with the vector's word. The 64 words of
 *    _sh2o_gte_read must equal the vector's result, and the return value must
 *    be 1 exactly when the word is on the kernel path (sf 1, mx < 3, cv != 2,
 *    each t of the translation inside the gate) and, in the native ELF, SR.S
 *    is 0 at the call. Four runs: SR.S at the writes and the load, then at
 *    the call and the reads: 0 0, 0 1, 1 1, 1 0. The counts of vectors and of
 *    kernel runs are pins. --plant-known k flips bit 0 of a copy of the first
 *    result word of MVMVA vector k, to show that the comparison fails.
 * 2. The digest of tests/test_prepared.c over the vectors of
 *    tests/suite/sh2o_k_test.h, as a chain of
 *    _sh2o_model_prepared_digest_part calls at SR.S 0, PREP_DIGEST_CHUNK
 *    vectors each, so that no call reaches the 100,000,000 instruction limit
 *    of SH2ZAM's invoke. The digest and the count of vectors on the kernel
 *    path are pins.
 * 3. The random runs of tests/test_prepared.c as a chain of
 *    _sh2o_model_prepared_op_part calls, at SR.S 0 and at SR.S 1: no run may
 *    differ from sh2o_gte_mvmva, and the count of kernel runs is a pin (native
 *    SR.S 1: 0, the fallback).
 * All comparisons come before any figure; one difference stops the run
 * (exit 1).
 * The figures (step 4), at SR.S 0, over the vectors of step 1 on the kernel
 * path: row A whole, one _sh2o_gte_mvmva call; row C whole, one
 * _sh2o_gte_mvmva_prepared call (it must give 1) after a load of all parts;
 * row A kernel, one _sh2o_model_prepared_mac3_a call (sh2o_k_mvb3); row C
 * kernel, one _sh2o_model_prepared_mac3 call (the prepared kernel); rows load,
 * one _sh2o_gte_prepared_load call with parts RT|TR, RT, TR and ALL on the
 * same start image. Each output is checked: the whole calls' 64 words against
 * the vector's result; the kernels' sums against T*1000h + M*V computed here
 * and against the vector's MAC1-3; each load's shadow against the shadow
 * computed here, and the parts it does not name unchanged. Informational rows:
 * C whole over all MVMVA vectors (run 0 0 of step 1) and C whole at SR.S 1
 * over the kernel-path vectors (run 0 1 of step 1). */
#define main sh2_model_main
#include "../../external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c"
#undef main
#include "../../tests/gte_oracle.h"
#include "../../tests/suite/sh2o_k_test.h"

#define PREP_IO_IMAGE 0u    /* one sh2o_gte_t: 64 words */
#define PREP_IO_SHADOW 256u /* one sh2o_gte_prepared_t: c 0..107, pre 108..179, ok 180..182 */
#define PREP_IO_V 448u      /* the kernels' v[3] (int16) */
#define PREP_IO_MAC 456u    /* the prepared kernel's mac[3] (int32) */
#define PREP_IO_T 468u      /* sh2o_k_mvb3's t[3] (int32) */
#define PREP_IO_OUT 480u    /* its out[3] (int64) */
#define PREP_IO_STATE 512u  /* the check state s[3] */
#define PREP_SHADOW_BYTES 183u
#define PREP_ORDER_COUNT 61u
#define PREP_MVMVA_PIN 71u  /* MVMVA cmd vectors in tests/gte_oracle.h */
#define PREP_FAST_PIN 21u   /* of them on the kernel path */
#define PREP_DIGEST_PIN UINT32_C(0x63A08AA3)
#define PREP_DIGEST_GATED_PIN 50970u
#define PREP_DIGEST_CHUNK 8192u
#define PREP_OP_RUNS 4096u
#define PREP_OP_SEED UINT32_C(0x53504B31)
#define PREP_OP_FAST_PIN 2297u
#define PREP_OP_CHUNK 1024u
#define PREP_LIMIT UINT64_C(100000000)

static const char *const prep_variants[2]={"native","portable"};
static const char *const prep_table_symbols[9]={"_sh2o_gte_write","_sh2o_gte_read","_sh2o_gte_mvmva",
    "_sh2o_gte_prepared_load","_sh2o_gte_mvmva_prepared","_sh2o_model_prepared_mac3","_sh2o_model_prepared_mac3_a",
    "_sh2o_model_prepared_digest_part","_sh2o_model_prepared_op_part"};
/* The write order of tests/suite/sh2o_gte_test_suite.c (sh2o_gte_order): every register but 15, 29 and 31, 28
 * before IR1-3. */
static const unsigned char prep_order[PREP_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,
    21,22,23,24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,
    62,63};
/* Step 1: SR at the writes and the load, then at the call and the reads; the pins of kernel runs [variant][run]. */
static const u32 prep_runs[4][2]={{0,0},{0,2},{2,2},{2,0}};
static const unsigned prep_run_fast_pins[2][4]={{PREP_FAST_PIN,0,0,PREP_FAST_PIN},
    {PREP_FAST_PIN,PREP_FAST_PIN,PREP_FAST_PIN,PREP_FAST_PIN}};
/* The loads timed: parts RT|TR, RT, TR, ALL (the last leaves the shadow for the C whole call). */
static const u32 prep_load_parts[4]={9,1,8,63};

enum {PREP_A_WHOLE,PREP_C_WHOLE,PREP_A_KERNEL,PREP_C_KERNEL,PREP_LOAD_RTTR,PREP_LOAD_RT,PREP_LOAD_TR,PREP_LOAD_ALL,
    PREP_C_WHOLE_ALL,PREP_C_WHOLE_S1,PREP_ROWS};
static const char *const prep_row_names[PREP_ROWS]={"A whole","C whole","A kernel","C kernel","load RT|TR",
    "load RT","load TR","load ALL","C whole, all MVMVA","C whole, SR.S 1"};

typedef struct {
    u32 min_cycles,max_cycles;
    uint64_t min_instructions,max_instructions,sum_cycles,sum_instructions,count;
} prep_stats_t;

static prep_stats_t prep_stats[2][PREP_ROWS]; /* [variant][row] */
static u32 prep_io;

static u32 prep_get_u32(const uint8_t *bytes) {
    return (u32)bytes[0]|((u32)bytes[1]<<8)|((u32)bytes[2]<<16)|((u32)bytes[3]<<24);
}

static int prep_is_mvmva(const gte_vector_t *vector) {
    return vector->op==GTE_OP_CMD&&(prep_get_u32(vector->in+256)&0x3Fu)==0x12u;
}

/* Loads one ELF into a clean RAM. load_symbols appends to the symbol list,
 * so the list is emptied first, else sym() gives the first ELF's addresses.
 * Then it checks the bridge table (layout: benchmarks/prepared-model/bridge.c). */
static void prep_load(const char *elf,const char *symbols_path) {
    nsymbols=0;
    memset(ram,0,sizeof(ram));
    load_symbols(symbols_path);
    load_elf(elf);
    prep_io=sym("_sh2o_model_prepared_io");
    u32 table=sym("_sh2o_model_bridge");
    for(unsigned entry=0;entry<9;entry++) {
        if(rl(&cpu,table+4*entry)!=sym(prep_table_symbols[entry])) {
            fprintf(stderr,"prepared: %s: bridge entry %u is %08x, symbol %s is %08x\n",elf,entry,
                rl(&cpu,table+4*entry),prep_table_symbols[entry],sym(prep_table_symbols[entry]));
            exit(2);
        }
    }
}

static void prep_stats_add(prep_stats_t *s,const struct cost *c) {
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

/* A vector's input image: the 61 words in the write order on a zeroed image, with SR = sr. */
static void prep_image(const uint8_t *in,u32 sr) {
    u32 g=prep_io+PREP_IO_IMAGE;
    for(unsigned r=0;r<64;r++)
        wl(&cpu,g+4*r,0);
    for(unsigned k=0;k<PREP_ORDER_COUNT;k++)
        invoke_sr("_sh2o_gte_write",g,prep_order[k],prep_get_u32(in+4*prep_order[k]),0,sr);
}

/* 1 when the 64 words of _sh2o_gte_read (SR = sr) equal want; else 0, with the first register that differs. */
static int prep_same(const u32 want[64],u32 sr,unsigned *reg,u32 *got) {
    u32 g=prep_io+PREP_IO_IMAGE;
    for(unsigned r=0;r<64;r++) {
        *got=invoke_sr("_sh2o_gte_read",g,r,0,0,sr).result;
        if(*got!=want[r]) {
            *reg=r;
            return 0;
        }
    }
    return 1;
}

/* The kernel path of the word cmd on the image in RAM (the gate of src/sh2o_gte_prepared.c without SR.S): sf 1,
 * mx < 3, cv != 2 and each t of translation ts inside |t| < 7FF40000h. When on it: 1, with mx, ts, V, T (0 for
 * cv 3) and want[i] = (int64)T[i] << 12 + M[i].V from the image words. */
static int prep_path(u32 cmd,unsigned *mx,unsigned *ts,int16_t v[3],int32_t t[3],int64_t want[3]) {
    u32 g=prep_io+PREP_IO_IMAGE;
    unsigned vx=cmd>>15&3u,cv=cmd>>13&3u;
    int16_t m[9];
    *mx=cmd>>17&3u;
    *ts=cv==3?2u:cv;
    if((cmd&0x3Fu)!=0x12u||!(cmd&0x80000u)||*mx==3||cv==2)
        return 0;
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
        m[k]=(int16_t)(rl(&cpu,g+4*(32u+8u*(*mx)+k/2))>>(16*(k&1)));
    for(unsigned i=0;i<3;i++)
        want[i]=(int64_t)t[i]*4096+(int64_t)m[3*i]*v[0]+(int64_t)m[3*i+1]*v[1]+(int64_t)m[3*i+2]*v[2];
    return 1;
}

/* The shadow of the image in RAM, as gte_prepared.h states it, in the bytes of the SH-2 layout (big-endian). */
static void prep_shadow(uint8_t out[PREP_SHADOW_BYTES]) {
    u32 g=prep_io+PREP_IO_IMAGE;
    memset(out,0,PREP_SHADOW_BYTES);
    for(unsigned k=0;k<3;k++)
        for(unsigned i=0;i<9;i++) {
            u32 h=rl(&cpu,g+4*(32+8*k+i/2))>>(16*(i&1));
            u32 c=(u32)((int32_t)(int16_t)(uint16_t)h*16);
            for(unsigned b=0;b<4;b++)
                out[36*k+4*i+b]=(uint8_t)(c>>(24-8*b));
        }
    for(unsigned k=0;k<3;k++) {
        uint8_t ok=1;
        for(unsigned i=0;i<3;i++) {
            u32 u=k<2?rl(&cpu,g+4*(37+8*k+i)):0u;
            int64_t pre=(int64_t)(int32_t)u*65536;
            u32 hi=(u32)((uint64_t)pre>>32),lo=(u32)pre;
            for(unsigned b=0;b<4;b++) {
                out[108+24*k+8*i+b]=(uint8_t)(hi>>(24-8*b));
                out[108+24*k+8*i+4+b]=(uint8_t)(lo>>(24-8*b));
            }
            if((int32_t)u<=-0x7FF40000||(int32_t)u>=0x7FF40000)
                ok=0;
        }
        out[180+k]=ok;
    }
}

/* 1 when the shadow in RAM after a load of parts holds the computed parts that parts names and the fill byte
 * 0xA5 elsewhere; else 0, with the first byte that differs. */
static int prep_shadow_check(u32 parts,unsigned *byte,unsigned *got,unsigned *expect) {
    u32 p=prep_io+PREP_IO_SHADOW;
    uint8_t want[PREP_SHADOW_BYTES];
    prep_shadow(want);
    for(unsigned b=0;b<PREP_SHADOW_BYTES;b++) {
        unsigned part=b<108?b/36:b<180?3+(b-108)/24:3+(b-180);
        u32 w=parts>>part&1u?want[b]:0xA5u,v=rb(&cpu,p+b);
        if(v!=w) {
            *byte=b;
            *got=v;
            *expect=w;
            return 0;
        }
    }
    return 1;
}

/* Step 1. Returns 1 at the first difference or when a count is not its pin. */
static int prep_exact(unsigned variant,long plant_known) {
    u32 g=prep_io+PREP_IO_IMAGE,p=prep_io+PREP_IO_SHADOW;
    for(unsigned run=0;run<4;run++) {
        u32 srl=prep_runs[run][0],src=prep_runs[run][1];
        unsigned k=0,fast=0;
        for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
            const gte_vector_t *vector=&gte_vectors[i];
            u32 cmd=prep_get_u32(vector->in+256),want[64],got;
            unsigned mx,ts,reg;
            int16_t v[3];
            int32_t t[3];
            int64_t sums[3];
            if(!prep_is_mvmva(vector))
                continue;
            for(unsigned r=0;r<64;r++)
                want[r]=prep_get_u32(vector->out+4+4*r);
            if(plant_known>=0&&(unsigned long)plant_known==k)
                want[0]^=1u;
            prep_image(vector->in,srl);
            int path=prep_path(cmd,&mx,&ts,v,t,sums);
            u32 expect=(u32)(path&&!(variant==0&&src));
            invoke_sr("_sh2o_gte_prepared_load",p,g,63,0,srl);
            struct cost c=invoke_sr("_sh2o_gte_mvmva_prepared",g,p,cmd,0,src);
            if(c.result!=expect) {
                fprintf(stderr,"prepared: %s: SR.S %u then %u: MVMVA vector %u (oracle vector %zu, word %08x): "
                    "sh2o_gte_mvmva_prepared gives %u, expected %u\n",prep_variants[variant],srl>>1,src>>1,k,i,cmd,
                    c.result,expect);
                return 1;
            }
            if(!prep_same(want,src,&reg,&got)) {
                fprintf(stderr,"prepared: %s: SR.S %u then %u: the known answer of MVMVA vector %u (oracle vector "
                    "%zu, word %08x) does not hold: register %u expected %08x, got %08x\n",prep_variants[variant],
                    srl>>1,src>>1,k,i,cmd,reg,want[reg],got);
                return 1;
            }
            if(run==0)
                prep_stats_add(&prep_stats[variant][PREP_C_WHOLE_ALL],&c);
            if(run==1&&path)
                prep_stats_add(&prep_stats[variant][PREP_C_WHOLE_S1],&c);
            fast+=c.result;
            k++;
        }
        printf("prepared: %s: SR.S %u at the load, %u at the call: %u MVMVA vectors exact, %u on the kernel path "
            "(pins %u, %u)\n",prep_variants[variant],srl>>1,src>>1,k,fast,PREP_MVMVA_PIN,
            prep_run_fast_pins[variant][run]);
        if(k!=PREP_MVMVA_PIN||fast!=prep_run_fast_pins[variant][run]) {
            fprintf(stderr,"prepared: %s: SR.S %u then %u: the counts are not the pins\n",prep_variants[variant],
                srl>>1,src>>1);
            return 1;
        }
    }
    return 0;
}

/* Step 2. Returns 1 when a check fails. */
static int prep_digest(unsigned variant) {
    u32 s=prep_io+PREP_IO_STATE;
    uint64_t most=0,instructions=0;
    wl(&cpu,s,SH2O_VEC_SEED);
    wl(&cpu,s+4,UINT32_C(0x811C9DC5));
    wl(&cpu,s+8,0);
    for(u32 first=0;first<SH2O_VEC_COUNT;first+=PREP_DIGEST_CHUNK) {
        u32 count=SH2O_VEC_COUNT-first<PREP_DIGEST_CHUNK?SH2O_VEC_COUNT-first:PREP_DIGEST_CHUNK;
        struct cost c=invoke("_sh2o_model_prepared_digest_part",first,count,s,0);
        instructions+=c.instructions;
        if(c.instructions>most)
            most=c.instructions;
    }
    u32 h=rl(&cpu,s+4),gated=rl(&cpu,s+8);
    printf("prepared: %s: SR.S 0: digest %08x (pin %08x), %u of %u vectors on the kernel path (pin %u), %" PRIu64
        " instructions, most of one _part call (%u vectors): %" PRIu64 "\n",prep_variants[variant],h,
        PREP_DIGEST_PIN,gated,SH2O_VEC_COUNT,PREP_DIGEST_GATED_PIN,instructions,PREP_DIGEST_CHUNK,most);
    if(most>=PREP_LIMIT) {
        fprintf(stderr,"prepared: %s: a digest _part call is at the instruction limit\n",prep_variants[variant]);
        return 1;
    }
    if(h!=PREP_DIGEST_PIN) {
        fprintf(stderr,"prepared: %s: the digest is not the pin\n",prep_variants[variant]);
        return 1;
    }
    if(gated!=PREP_DIGEST_GATED_PIN) {
        fprintf(stderr,"prepared: %s: digest: the counts are not the pins\n",prep_variants[variant]);
        return 1;
    }
    return 0;
}

/* Step 3 at SR = sr. Returns 1 when a run differs or the count is not its pin. */
static int prep_ops(unsigned variant,u32 sr) {
    u32 s=prep_io+PREP_IO_STATE,pin=variant==0&&sr?0u:PREP_OP_FAST_PIN;
    uint64_t most=0;
    wl(&cpu,s,PREP_OP_SEED);
    wl(&cpu,s+4,0);
    wl(&cpu,s+8,0);
    for(u32 done=0;done<PREP_OP_RUNS;done+=PREP_OP_CHUNK) {
        struct cost c=invoke_sr("_sh2o_model_prepared_op_part",PREP_OP_CHUNK,s,0,0,sr);
        if(c.instructions>most)
            most=c.instructions;
    }
    u32 differ=rl(&cpu,s+4),fast=rl(&cpu,s+8);
    printf("prepared: %s: SR.S %u: random: %u runs, %u differ, %u on the kernel path (pin %u), most instructions of "
        "one _part call (%u runs): %" PRIu64 "\n",prep_variants[variant],sr>>1,PREP_OP_RUNS,differ,fast,pin,
        PREP_OP_CHUNK,most);
    if(most>=PREP_LIMIT) {
        fprintf(stderr,"prepared: %s: a random _part call is at the instruction limit\n",prep_variants[variant]);
        return 1;
    }
    if(differ) {
        fprintf(stderr,"prepared: %s: SR.S %u: random: %u runs differ from sh2o_gte_mvmva\n",prep_variants[variant],
            sr>>1,differ);
        return 1;
    }
    if(fast!=pin) {
        fprintf(stderr,"prepared: %s: SR.S %u: random: the counts are not the pins\n",prep_variants[variant],sr>>1);
        return 1;
    }
    return 0;
}

/* Step 4 at SR.S 0. Returns 1 at the first difference, or when no vector is on the kernel path. */
static int prep_timing(unsigned variant) {
    u32 g=prep_io+PREP_IO_IMAGE,p=prep_io+PREP_IO_SHADOW,vp=prep_io+PREP_IO_V,mp=prep_io+PREP_IO_MAC,
        tp=prep_io+PREP_IO_T,op=prep_io+PREP_IO_OUT;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        u32 cmd=prep_get_u32(vector->in+256),want[64],got;
        unsigned mx,ts,reg;
        int16_t v[3];
        int32_t t[3];
        int64_t sums[3];
        struct cost c;
        if(!prep_is_mvmva(vector))
            continue;
        for(unsigned r=0;r<64;r++)
            want[r]=prep_get_u32(vector->out+4+4*r);
        prep_image(vector->in,0);
        if(!prep_path(cmd,&mx,&ts,v,t,sums))
            continue;
        /* A whole */
        c=invoke_sr("_sh2o_gte_mvmva",g,cmd,0,0,0);
        if(!prep_same(want,0,&reg,&got)) {
            fprintf(stderr,"prepared: %s: timing: A whole differs from the result of vector %zu (word %08x), "
                "register %u: result %08x, got %08x\n",prep_variants[variant],i,cmd,reg,want[reg],got);
            return 1;
        }
        prep_stats_add(&prep_stats[variant][PREP_A_WHOLE],&c);
        /* the loads, on the start image */
        prep_image(vector->in,0);
        for(unsigned l=0;l<4;l++) {
            unsigned byte,gb,eb;
            for(unsigned b=0;b<PREP_SHADOW_BYTES;b++)
                wb(&cpu,p+b,0xA5);
            c=invoke_sr("_sh2o_gte_prepared_load",p,g,prep_load_parts[l],0,0);
            if(!prep_shadow_check(prep_load_parts[l],&byte,&gb,&eb)) {
                fprintf(stderr,"prepared: %s: timing: load of parts %u, vector %zu (word %08x): shadow byte %u is "
                    "%02x, expected %02x\n",prep_variants[variant],prep_load_parts[l],i,cmd,byte,gb,eb);
                return 1;
            }
            prep_stats_add(&prep_stats[variant][PREP_LOAD_RTTR+l],&c);
        }
        /* C whole, with the shadow of all parts */
        c=invoke_sr("_sh2o_gte_mvmva_prepared",g,p,cmd,0,0);
        if(c.result!=1||!prep_same(want,0,&reg,&got)) {
            fprintf(stderr,"prepared: %s: timing: C whole gives %u or differs from the result of vector %zu (word "
                "%08x), register %u: result %08x, got %08x\n",prep_variants[variant],c.result,i,cmd,reg,want[reg],
                got);
            return 1;
        }
        prep_stats_add(&prep_stats[variant][PREP_C_WHOLE],&c);
        /* C kernel: the shadow's rows of mx and preload of ts, V taken before the whole calls */
        for(unsigned k=0;k<3;k++) {
            ww(&cpu,vp+2*k,(u16)v[k]);
            wl(&cpu,mp+4*k,0);
        }
        c=invoke_sr("_sh2o_model_prepared_mac3",p+36*mx,p+108+24*ts,vp,mp,0);
        for(unsigned k=0;k<3;k++) {
            u32 mac=rl(&cpu,mp+4*k);
            if(mac==want[25+k]&&mac==(u32)(int32_t)(sums[k]>>12))
                continue;
            fprintf(stderr,"prepared: %s: timing: C kernel differs in vector %zu (word %08x), mac[%u]: %08x, "
                "(T*1000h + M*V) SAR 12 %08x, MAC%u %08x\n",prep_variants[variant],i,cmd,k,mac,
                (u32)(int32_t)(sums[k]>>12),k+1,want[25+k]);
            return 1;
        }
        prep_stats_add(&prep_stats[variant][PREP_C_KERNEL],&c);
        /* A kernel: the matrix words of the image (no MVMVA writes them), V and T */
        for(unsigned k=0;k<3;k++) {
            wl(&cpu,tp+4*k,(u32)t[k]);
            wl(&cpu,op+8*k,0);
            wl(&cpu,op+8*k+4,0);
        }
        c=invoke_sr("_sh2o_model_prepared_mac3_a",g+4*(32+8*mx),vp,tp,op,0);
        for(unsigned k=0;k<3;k++) {
            int64_t out=(int64_t)((uint64_t)rl(&cpu,op+8*k)<<32|rl(&cpu,op+8*k+4));
            if(out==sums[k]&&(u32)(int32_t)(out>>12)==want[25+k])
                continue;
            fprintf(stderr,"prepared: %s: timing: A kernel differs in vector %zu (word %08x), out[%u]: %016" PRIx64
                ", T*1000h + M*V %016" PRIx64 ", MAC%u %08x\n",prep_variants[variant],i,cmd,k,(uint64_t)out,
                (uint64_t)sums[k],k+1,want[25+k]);
            return 1;
        }
        prep_stats_add(&prep_stats[variant][PREP_A_KERNEL],&c);
    }
    if(prep_stats[variant][PREP_A_WHOLE].count!=PREP_FAST_PIN) {
        fprintf(stderr,"prepared: %s: timing: %" PRIu64 " vectors on the kernel path, pin %u\n",
            prep_variants[variant],prep_stats[variant][PREP_A_WHOLE].count,PREP_FAST_PIN);
        return 1;
    }
    printf("prepared: %s: timing, SR.S 0: %u kernel-path vectors: A whole, C whole == vector result, kernels == "
        "T*1000h + M*V and MAC1-3, 4 loads == computed shadow: 0 differ\n",prep_variants[variant],PREP_FAST_PIN);
    return 0;
}

static double prep_mean(const prep_stats_t *s) {
    return (double)s->sum_cycles/(double)s->count;
}

int main(int argc,char **argv) {
    long plant_known=-1;
    if(argc==12&&!strcmp(argv[10],"--plant-known")) {
        char *end;
        plant_known=strtol(argv[11],&end,10);
        if(*end||plant_known<0||plant_known>=(long)PREP_MVMVA_PIN) {
            fprintf(stderr,"prepared: --plant-known needs an MVMVA vector from 0 to %u\n",PREP_MVMVA_PIN-1);
            return 2;
        }
    } else if(argc!=10) {
        fprintf(stderr,"usage: prepared_harness native.elf native.sym portable.elf portable.sym sh2zam-sha "
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
        prep_load(argv[1+2*variant],argv[2+2*variant]);
        printf("prepared: %s: %s\n",prep_variants[variant],argv[1+2*variant]);
        if(prep_exact(variant,plant_known))
            return 1;
        if(prep_digest(variant))
            return 1;
        if(prep_ops(variant,0)||prep_ops(variant,2))
            return 1;
        if(prep_timing(variant))
            return 1;
    }
    printf("\nMODELLED cycles: Yabause SH-2 interpreter, flat RAM, no caches, wait states or bus contention.\n");
    printf("Not Saturn timing. SR.S 0, over the MVMVA cmd vectors of tests/gte_oracle.h on the kernel path. A whole:\n");
    printf("one sh2o_gte_mvmva call. C whole: one sh2o_gte_mvmva_prepared call (it gives 1). A kernel: one\n");
    printf("sh2o_model_prepared_mac3_a call (sh2o_k_mvb3). C kernel: one sh2o_model_prepared_mac3 call (the prepared\n");
    printf("kernel). Load: one sh2o_gte_prepared_load call with the parts named, on the same start image. The kernel\n");
    printf("rows include the wrapper's call. Informational: C whole over all MVMVA vectors, and C whole at SR.S 1\n");
    printf("over the kernel-path vectors (native: the fallback to sh2o_gte_mvmva).\n");
    printf("  SH2ZAM   %s\n  Yabause  %s\n  Yaul     %s\n",argv[5],argv[6],argv[7]);
    printf("  sha256   %s  native\n  sha256   %s  portable\n",argv[8],argv[9]);
    printf("\n%-9s %-19s %7s %7s %9s %9s %7s\n","variant","call","cyc min","cyc max","cyc mean","ins mean",
        "inputs");
    for(unsigned variant=0;variant<2;variant++) {
        for(unsigned row=0;row<PREP_ROWS;row++) {
            const prep_stats_t *s=&prep_stats[variant][row];
            printf("%-9s %-19s %7u %7u %9.2f %9.2f %7" PRIu64 "\n",prep_variants[variant],prep_row_names[row],
                s->min_cycles,s->max_cycles,prep_mean(s),(double)s->sum_instructions/(double)s->count,s->count);
        }
    }
    printf("\nMODELLED, from the means above. Per call at r calls for each load: C whole + load / r.\n");
    printf("Break-even r: load / (A whole - C whole).\n");
    printf("\n%-9s %-9s %12s %12s %12s %12s\n","variant","load","C at 4.27","C at 3.06","A whole","break-even");
    for(unsigned variant=0;variant<2;variant++) {
        const prep_stats_t *s=prep_stats[variant];
        double a=prep_mean(&s[PREP_A_WHOLE]),cw=prep_mean(&s[PREP_C_WHOLE]);
        for(unsigned l=0;l<2;l++) {
            double load=prep_mean(&s[l?PREP_LOAD_ALL:PREP_LOAD_RTTR]);
            char even[32];
            if(cw>=a) {
                snprintf(even,sizeof(even),"none");
            } else {
                snprintf(even,sizeof(even),"%.2f",load/(a-cw));
            }
            printf("%-9s %-9s %12.2f %12.2f %12.2f %12s\n",prep_variants[variant],l?"ALL":"RT|TR",cw+load/4.27,
                cw+load/3.06,a,even);
        }
    }
    return 0;
}
