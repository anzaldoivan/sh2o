/* Byte-exact oracle test and suite of the GTE machine.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/gte.h"
#include "sh2o/k.h"
#include "sh2o_gte_test.h"
#include "gte_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each oracle vector goes through sh2o_gte_write, sh2o_gte_command and sh2o_gte_read. An input image is its 64
 * registers written in sh2o_gte_order on a zeroed image. cmd: the command, then its return code and the 64 reads.
 * write: one more register write, then the 64 reads and the read of the written register. Words are
 * little-endian bytes. Then the test runs the suite (tests/suite).
 * The test also checks the fixed-word entries sh2o_gte_nrtps and sh2o_gte_nrtpt (entry_differences), and runs each
 * DPCT vector through sh2o_gte_dpct directly (dpct_differences).
 * Modes: no argument, the test; --mutant <name>, the count of vectors that differ (CMakeLists.txt builds a mutant
 * from a changed copy of src/ and include/ and passes when the count is not 0). For swapped_flag_bit the suite
 * must also fail, with the digest SWAPPED_FLAG_DIGEST. --mutant-entry <name>: the same with the entry check (family
 * gte_entry).
 * --driver: one case on each stdin line, "<cmd hex|-> <n> <reg> <value hex> ..." (n pairs). Each pair goes through
 * sh2o_gte_write in the listed order on an image zeroed for each line, then sh2o_gte_command(cmd) unless cmd is "-".
 * Out: the 64 words of sh2o_gte_read, in hex, with "x " first when the command returned -1. Bad input: exit code 2.
 * tests/gte_sources_check.py compares this output with the Python model.
 * --unr: for each SZ 1..FFFFh one line "sz z recip" (decimal): z = sh2o_k_clz32(sz) - 16, recip =
 * sh2o_k_unr_recip(sz << z). --unr-sweep: sh2o_k_unr_div(h,sz) for each SZ 1..FFFFh and h 0..min(2 sz - 1,FFFFh),
 * against (((h << z) * recip + 8000h) >> 16) clamped to 1FFFFh. It stops at the first difference (on stderr, exit
 * code 1) and always prints "unr-sweep <pairs> <bad>". tests/unr_check.py checks both. */

#define SWAPPED_FLAG_DIGEST 0x016CD511u

static uint32_t get_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}

static void put_u32(uint8_t *bytes,uint32_t value) {
    for(unsigned i=0;i<4;i++)
        bytes[i]=(uint8_t)(value>>(8*i));
}

static void run(const gte_vector_t *vector,uint8_t *out) {
    sh2o_gte_t g;
    unsigned r;
    memset(&g,0,sizeof g);
    for(unsigned k=0;k<SH2O_GTE_ORDER_COUNT;k++)
        sh2o_gte_write(&g,sh2o_gte_order[k],get_u32(vector->in+4*sh2o_gte_order[k]));
    switch(vector->op) {
    case GTE_OP_CMD:
        put_u32(out,(uint32_t)sh2o_gte_command(&g,get_u32(vector->in+256)));
        for(r=0;r<64;r++)
            put_u32(out+4+4*r,sh2o_gte_read(&g,r));
        break;
    case GTE_OP_WRITE:
        r=(unsigned)get_u32(vector->in+256);
        sh2o_gte_write(&g,r,get_u32(vector->in+260));
        put_u32(out+256,sh2o_gte_read(&g,r));
        for(r=0;r<64;r++)
            put_u32(out+4*r,sh2o_gte_read(&g,r));
        break;
    default:
        fprintf(stderr,"gte: vector has unknown op %u\n",vector->op);
        exit(EXIT_FAILURE);
    }
}

/* Returns the count of vectors whose result differs, and prints the first difference. */
static unsigned long differences(const char *name,unsigned long *bytes_compared) {
    uint8_t out[GTE_OUT_MAX];
    unsigned long count=0;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        run(vector,out);
        *bytes_compared+=vector->out_size;
        if(memcmp(out,vector->out,vector->out_size)==0)
            continue;
        if(count==0) {
            unsigned byte=0;
            while(out[byte]==vector->out[byte])
                byte++;
            fprintf(stderr,"gte: %s: first difference in vector %zu (%s), result byte %u: expected 0x%02x, got "
                "0x%02x\n",name,i,gte_op_names[vector->op],byte,vector->out[byte],out[byte]);
        }
        count++;
    }
    return count;
}

static int driver(void) {
    char cmd[32];
    while(scanf("%31s",cmd)==1) {
        sh2o_gte_t g;
        int n,i;
        memset(&g,0,sizeof g);
        if(scanf("%d",&n)!=1)
            return 2;
        for(i=0;i<n;i++) {
            unsigned r,v;
            if(scanf("%u %x",&r,&v)!=2||r>63)
                return 2;
            sh2o_gte_write(&g,r,v);
        }
        if(strcmp(cmd,"-")!=0) {
            unsigned c;
            if(sscanf(cmd,"%x",&c)!=1)
                return 2;
            if(sh2o_gte_command(&g,c)!=0)
                printf("x ");
        }
        for(i=0;i<64;i++)
            printf(i?" %08lx":"%08lx",(unsigned long)sh2o_gte_read(&g,(unsigned)i));
        printf("\n");
    }
    return 0;
}

static int unr(void) {
    for(uint32_t sz=1;sz<=0xFFFFu;sz++) {
        unsigned z=sh2o_k_clz32(sz)-16u;
        printf("%lu %u %lu\n",(unsigned long)sz,z,(unsigned long)sh2o_k_unr_recip(sz<<z));
    }
    return 0;
}

static int unr_sweep(void) {
    unsigned long long pairs=0,bad=0;
    for(uint32_t sz=1;sz<=0xFFFFu&&bad==0;sz++) {
        unsigned z=sh2o_k_clz32(sz)-16u;
        uint64_t recip=sh2o_k_unr_recip(sz<<z);
        for(uint32_t h=0;h<2*sz&&h<=0xFFFFu;h++) {
            uint64_t want=(((uint64_t)h<<z)*recip+0x8000u)>>16;
            uint32_t got=sh2o_k_unr_div(h,sz);
            if(want>0x1FFFFu)
                want=0x1FFFFu;
            pairs++;
            if(got!=want) {
                fprintf(stderr,"gte: unr-sweep: first difference at h %lx sz %lx: %lx, expected %lx\n",
                    (unsigned long)h,(unsigned long)sz,(unsigned long)got,(unsigned long)want);
                bad++;
                break;
            }
        }
    }
    printf("unr-sweep %llu %llu\n",pairs,bad);
    return bad?EXIT_FAILURE:0;
}

/* The fixed-word entries: for each cmd vector of op 01h or 30h, its input image as run() loads it goes through
 * sh2o_gte_nrtps or sh2o_gte_nrtpt on one copy and sh2o_gte_command with that entry's word on another; the 64 reads
 * must be equal, and where the vector's word has sf 1 and lm 0 they must also equal the vector's out (the model's
 * answer). Returns the count of vectors that differ and prints the first; compared and model count by entry (0 nRTPS,
 * 1 nRTPT). */
static unsigned long entry_differences(const char *name,unsigned long compared[2],unsigned long model[2]) {
    static const uint32_t words[2]={0x4A180001u,0x4A280030u};
    static void (*const entries[2])(sh2o_gte_t *g)={sh2o_gte_nrtps,sh2o_gte_nrtpt};
    unsigned long count=0;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        sh2o_gte_t a,b;
        uint32_t cmd;
        unsigned k;
        int with_model,bad=0;
        if(vector->op!=GTE_OP_CMD)
            continue;
        cmd=get_u32(vector->in+256);
        if((cmd&0x3Fu)==0x01u)
            k=0;
        else if((cmd&0x3Fu)==0x30u)
            k=1;
        else
            continue;
        memset(&a,0,sizeof a);
        for(unsigned j=0;j<SH2O_GTE_ORDER_COUNT;j++)
            sh2o_gte_write(&a,sh2o_gte_order[j],get_u32(vector->in+4*sh2o_gte_order[j]));
        b=a;
        entries[k](&a);
        (void)sh2o_gte_command(&b,words[k]);
        with_model=(cmd&0x80400u)==0x80000u;
        compared[k]++;
        model[k]+=(unsigned long)with_model;
        for(unsigned r=0;r<64&&!bad;r++) {
            uint32_t got=sh2o_gte_read(&a,r);
            uint32_t want=sh2o_gte_read(&b,r);
            if(got==want&&with_model)
                want=get_u32(vector->out+4+4*r);
            if(got!=want) {
                if(count==0)
                    fprintf(stderr,"gte: %s: entry %s differs in vector %zu (cmd 0x%08lX), register %u: 0x%08lX, "
                        "expected 0x%08lX\n",name,k?"nrtpt":"nrtps",i,(unsigned long)cmd,r,(unsigned long)got,
                        (unsigned long)want);
                bad=1;
            }
        }
        count+=(unsigned long)bad;
    }
    return count;
}

/* Each cmd vector of op 2Ah: its input image as run() loads it goes through sh2o_gte_dpct with the vector's word; the
 * 64 reads must equal the vector's out. Returns the count of vectors that differ and prints the first; *compared
 * counts the DPCT vectors. */
static unsigned long dpct_differences(unsigned long *compared) {
    unsigned long count=0;
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++) {
        const gte_vector_t *vector=&gte_vectors[i];
        sh2o_gte_t g;
        uint32_t cmd;
        if(vector->op!=GTE_OP_CMD)
            continue;
        cmd=get_u32(vector->in+256);
        if((cmd&0x3Fu)!=0x2Au)
            continue;
        memset(&g,0,sizeof g);
        for(unsigned j=0;j<SH2O_GTE_ORDER_COUNT;j++)
            sh2o_gte_write(&g,sh2o_gte_order[j],get_u32(vector->in+4*sh2o_gte_order[j]));
        sh2o_gte_dpct(&g,cmd);
        (*compared)++;
        for(unsigned r=0;r<64;r++) {
            uint32_t got=sh2o_gte_read(&g,r);
            uint32_t want=get_u32(vector->out+4+4*r);
            if(got!=want) {
                if(count==0)
                    fprintf(stderr,"gte: sh2o_gte_dpct differs in vector %zu (cmd 0x%08lX), register %u: 0x%08lX, "
                        "expected 0x%08lX\n",i,(unsigned long)cmd,r,(unsigned long)got,(unsigned long)want);
                count++;
                break;
            }
        }
    }
    return count;
}

int main(int argc,char **argv) {
    unsigned long per_op[GTE_OP_COUNT]={0};
    unsigned long bytes=0,compared[2]={0},model[2]={0},entry_bad,dpct=0,dpct_bad;
    int rc;
    if(argc==2&&strcmp(argv[1],"--driver")==0)
        return driver();
    if(argc==2&&strcmp(argv[1],"--unr")==0)
        return unr();
    if(argc==2&&strcmp(argv[1],"--unr-sweep")==0)
        return unr_sweep();
    if(argc==3&&strcmp(argv[1],"--mutant")==0) {
        unsigned long count=differences(argv[2],&bytes);
        printf("mutant %s: %lu differences in %zu oracle vectors\n",argv[2],count,(size_t)GTE_VECTOR_COUNT);
        if(strcmp(argv[2],"swapped_flag_bit")==0) {
            uint32_t digest=sh2o_gte_test_digest();
            rc=sh2o_gte_test_suite();
            printf("mutant %s: suite returned 0x%X, digest 0x%08lX (expected 0x%08lX)\n",argv[2],(unsigned)rc,
                (unsigned long)digest,(unsigned long)SWAPPED_FLAG_DIGEST);
            if(rc==0||digest!=SWAPPED_FLAG_DIGEST)
                return EXIT_FAILURE;
        }
        return count>0?0:EXIT_FAILURE;
    }
    if(argc==3&&strcmp(argv[1],"--mutant-entry")==0) {
        entry_bad=entry_differences(argv[2],compared,model);
        printf("mutant %s: %lu entry differences in %lu vectors\n",argv[2],entry_bad,compared[0]+compared[1]);
        return entry_bad>0?0:EXIT_FAILURE;
    }
    if(argc!=1) {
        fprintf(stderr,"usage: %s [--mutant <name> | --mutant-entry <name> | --driver | --unr | --unr-sweep]\n",
            argv[0]);
        return EXIT_FAILURE;
    }
    for(size_t i=0;i<GTE_VECTOR_COUNT;i++)
        per_op[gte_vectors[i].op]++;
    for(unsigned op=0;op<GTE_OP_COUNT;op++) {
        if(per_op[op]==0) {
            fprintf(stderr,"gte: no vector for op %s\n",gte_op_names[op]);
            return EXIT_FAILURE;
        }
    }
    if(differences("library",&bytes)!=0) {
        fprintf(stderr,"gte: FAIL: the library differs from the oracle\n");
        return EXIT_FAILURE;
    }
    printf("gte: %zu vectors (%lu cmd, %lu write), %lu bytes, zero differences\n",(size_t)GTE_VECTOR_COUNT,
        per_op[GTE_OP_CMD],per_op[GTE_OP_WRITE],bytes);
    entry_bad=entry_differences("library",compared,model);
    printf("gte: entries: nrtps %lu vectors (%lu model), nrtpt %lu vectors (%lu model), %lu differences\n",
        compared[0],model[0],compared[1],model[1],entry_bad);
    if(entry_bad!=0||model[0]<1||model[1]<1) {
        fprintf(stderr,"gte: FAIL: an entry differs, or an entry has no model compare\n");
        return EXIT_FAILURE;
    }
    dpct_bad=dpct_differences(&dpct);
    printf("gte: sh2o_gte_dpct: %lu vectors, %lu differences\n",dpct,dpct_bad);
    if(dpct_bad!=0||dpct<1) {
        fprintf(stderr,"gte: FAIL: sh2o_gte_dpct differs from the oracle, or no DPCT vector\n");
        return EXIT_FAILURE;
    }
    rc=sh2o_gte_test_suite();
    printf("gte: suite: %u cases, digest 0x%08lX (pinned 0x%08lX)\n",(unsigned)SH2O_GTE_CASES,
        (unsigned long)sh2o_gte_test_digest(),(unsigned long)SH2O_DIGEST_GTE);
    if(rc!=0) {
        fprintf(stderr,"gte: FAIL: the suite returned 0x%X\n",(unsigned)rc);
        return EXIT_FAILURE;
    }
    printf("gte: suite passed\n");
    return 0;
}
