/* Byte-exact oracle test and bulk suite of the fixed-point kernels.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/k.h"
#include "sh2o_k_test.h"
#include "k_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each oracle vector goes through the public entry and through its C reference (`_c`). The test decodes the
 * inputs from their little-endian bytes and encodes the results with shifts, as tests/test_fixed.c does. A
 * 64-bit result is its low word, then its high word. Then the test runs the bulk suite (tests/suite).
 * Modes: no argument, the test; --dump, one line for each bulk vector (tests/k_bulk_check.py reads it);
 * --mutant <name>, the count of vectors that differ (CMakeLists.txt builds a mutant from a changed copy of
 * include/ and passes when the count is not 0). */

static uint32_t get_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}

static int16_t get_i16(const uint8_t *bytes) {
    return (int16_t)(uint16_t)((unsigned)bytes[0]|((unsigned)bytes[1]<<8));
}

static void put_u32(uint8_t *bytes,uint32_t value) {
    for(unsigned i=0;i<4;i++)
        bytes[i]=(uint8_t)(value>>(8*i));
}

static void put_i64(uint8_t *bytes,int64_t value) {
    put_u32(bytes,(uint32_t)((uint64_t)value&UINT32_C(0xFFFFFFFF)));
    put_u32(bytes+4,(uint32_t)((uint64_t)value>>32));
}

/* which: 0 the public entry, 1 the C reference */
static void run(unsigned which,const k_vector_t *vector,uint8_t *out) {
    const uint8_t *in=vector->in;
    int16_t m[3],v[3];
    uint32_t w[5],mac[3];
    int32_t t[3];
    int64_t r[3];
    unsigned i;
    switch(vector->op) {
    case K_OP_DOT3B:
        for(i=0;i<3;i++) {
            m[i]=get_i16(in+2*i);
            v[i]=get_i16(in+6+2*i);
        }
        put_i64(out,which?sh2o_k_dot3b_c(m,v,(int32_t)get_u32(in+12)):sh2o_k_dot3b(m,v,(int32_t)get_u32(in+12)));
        break;
    case K_OP_MVB3:
        for(i=0;i<5;i++)
            w[i]=get_u32(in+4*i);
        for(i=0;i<3;i++) {
            v[i]=get_i16(in+20+2*i);
            t[i]=(int32_t)get_u32(in+26+4*i);
        }
        if(which)
            sh2o_k_mvb3_c(w,v,t,r);
        else
            sh2o_k_mvb3(w,v,t,r);
        for(i=0;i<3;i++)
            put_i64(out+8*i,r[i]);
        break;
    case K_OP_GPF3:
        for(i=0;i<4;i++)
            w[i]=get_u32(in+4*i);
        if(which)
            sh2o_k_gpf3_c(w,r);
        else
            sh2o_k_gpf3(w,r);
        for(i=0;i<3;i++)
            put_i64(out+8*i,r[i]);
        break;
    case K_OP_GPL3:
        for(i=0;i<4;i++)
            w[i]=get_u32(in+4*i);
        for(i=0;i<3;i++)
            mac[i]=get_u32(in+16+4*i);
        if(which)
            sh2o_k_gpl3_c(w,mac,in[28],r);
        else
            sh2o_k_gpl3(w,mac,in[28],r);
        for(i=0;i<3;i++)
            put_i64(out+8*i,r[i]);
        break;
    case K_OP_CLZ32:
        out[0]=(uint8_t)(which?sh2o_k_clz32_c(get_u32(in)):sh2o_k_clz32(get_u32(in)));
        break;
    case K_OP_UNR_RECIP:
        put_u32(out,which?sh2o_k_unr_recip_c(get_u32(in)):sh2o_k_unr_recip(get_u32(in)));
        break;
    case K_OP_UNR_DIV:
        put_u32(out,which?sh2o_k_unr_div_c(get_u32(in),get_u32(in+4)):sh2o_k_unr_div(get_u32(in),get_u32(in+4)));
        break;
    default:
        fprintf(stderr,"k: vector has unknown op %u\n",vector->op);
        exit(EXIT_FAILURE);
    }
}

static void print_bytes(const char *label,const uint8_t *bytes,unsigned size) {
    fprintf(stderr,"  %-8s",label);
    for(unsigned i=0;i<size;i++)
        fprintf(stderr," %02x",bytes[i]);
    fprintf(stderr,"\n");
}

/* Returns the count of vectors whose result differs on either path, and prints the first difference. */
static unsigned long differences(const char *name,unsigned long *bytes_compared) {
    static const char *const paths[2]={"public entry","C reference"};
    uint8_t out[K_OUT_MAX];
    unsigned long count=0;
    for(size_t i=0;i<K_VECTOR_COUNT;i++) {
        const k_vector_t *vector=&k_vectors[i];
        int differs=0;
        for(unsigned which=0;which<2;which++) {
            run(which,vector,out);
            *bytes_compared+=vector->out_size;
            if(memcmp(out,vector->out,vector->out_size)==0)
                continue;
            if(!differs&&count==0) {
                unsigned byte=0;
                while(out[byte]==vector->out[byte])
                    byte++;
                fprintf(stderr,"k: %s: first difference in vector %zu (%s, %s), result byte %u\n",
                    name,i,k_op_names[vector->op],paths[which],byte);
                print_bytes("in",vector->in,vector->in_size);
                print_bytes("expected",vector->out,vector->out_size);
                print_bytes("got",out,vector->out_size);
            }
            differs=1;
        }
        count+=(unsigned long)differs;
    }
    return count;
}

/* One line for each bulk vector: i m0 m1 m2 v0 v1 v2 tr, then the C results dot3b, mvb3 x3, gpf3 x3, gpl3 x3
 * (the inputs as the suite builds them). */
static void dump(void) {
    uint32_t x=SH2O_VEC_SEED,i,w[5],g[4],mac[3];
    int32_t t[3];
    int64_t mv[3],gf[3],gl[3];
    sh2o_vec e;
    for(i=0;i<SH2O_VEC_COUNT;i++) {
        sh2o_vec_get(i,&x,&e);
        sh2o_k_test_mvb3_in(&e,w,t);
        sh2o_k_mvb3_c(w,e.v,t,mv);
        sh2o_k_test_gp3_in(&e,g,mac);
        sh2o_k_gpf3_c(g,gf);
        sh2o_k_gpl3_c(g,mac,i&1u,gl);
        printf("%lu %d %d %d %d %d %d %ld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",(unsigned long)i,
            e.m[0],e.m[1],e.m[2],e.v[0],e.v[1],e.v[2],(long)e.tr,(long long)sh2o_k_dot3b_c(e.m,e.v,e.tr),
            (long long)mv[0],(long long)mv[1],(long long)mv[2],(long long)gf[0],(long long)gf[1],(long long)gf[2],
            (long long)gl[0],(long long)gl[1],(long long)gl[2]);
    }
}

int main(int argc,char **argv) {
    unsigned long per_op[K_OP_COUNT]={0};
    unsigned long bytes=0;
    uint32_t hg[2],he[2];
    int rc;
    if(argc==2&&strcmp(argv[1],"--dump")==0) {
        dump();
        return 0;
    }
    if(argc==3&&strcmp(argv[1],"--mutant")==0) {
        unsigned long count=differences(argv[2],&bytes);
        printf("mutant %s: %lu differences in %zu oracle vectors\n",argv[2],count,(size_t)K_VECTOR_COUNT);
        return count>0?0:EXIT_FAILURE;
    }
    if(argc!=1) {
        fprintf(stderr,"usage: %s [--dump | --mutant <name>]\n",argv[0]);
        return EXIT_FAILURE;
    }
    for(size_t i=0;i<K_VECTOR_COUNT;i++)
        per_op[k_vectors[i].op]++;
    for(unsigned op=0;op<K_OP_COUNT;op++) {
        if(per_op[op]==0) {
            fprintf(stderr,"k: no vector for op %s\n",k_op_names[op]);
            return EXIT_FAILURE;
        }
    }
    if(differences("library",&bytes)!=0) {
        fprintf(stderr,"k: FAIL: the library differs from the oracle\n");
        return EXIT_FAILURE;
    }
    printf("k: %zu vectors, %lu bytes, public entry and C reference, zero differences\n",
        (size_t)K_VECTOR_COUNT,bytes);
    rc=sh2o_k_test_suite();
    sh2o_k_test_gp3_digest(1,SH2O_VEC_COUNT,hg);
    sh2o_k_test_gp3_digest(1,SH2O_VEC_EDGE_TR0,he);
    printf("k: suite: %u vectors (%u edge, %u random), dot3b 0x%08lX (pinned 0x%08lX), mvb3 0x%08lX (pinned 0x%08lX)\n",
        (unsigned)SH2O_VEC_COUNT,(unsigned)SH2O_VEC_EDGE,(unsigned)SH2O_VEC_RANDOM,
        (unsigned long)sh2o_k_test_digest(),(unsigned long)SH2O_DIGEST_DOT3B,
        (unsigned long)sh2o_k_test_mvb3_digest(1),(unsigned long)SH2O_DIGEST_MVB3);
    printf("k: suite: gpf3 0x%08lX (pinned 0x%08lX), gpl3 0x%08lX (pinned 0x%08lX); first %u: 0x%08lX 0x%08lX "
        "(pinned 0x%08lX 0x%08lX)\n",(unsigned long)hg[0],(unsigned long)SH2O_DIGEST_GPF3,(unsigned long)hg[1],
        (unsigned long)SH2O_DIGEST_GPL3,(unsigned)SH2O_VEC_EDGE_TR0,(unsigned long)he[0],(unsigned long)he[1],
        (unsigned long)SH2O_DIGEST_GPF3_EDGE,(unsigned long)SH2O_DIGEST_GPL3_EDGE);
    if(rc!=0) {
        fprintf(stderr,"k: FAIL: the suite returned 0x%X\n",(unsigned)rc);
        return EXIT_FAILURE;
    }
    printf("k: suite passed\n");
    return 0;
}
