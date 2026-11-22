/* Byte-exact oracle test for the fixed-point primitives.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/sh2o.h"
#include "fixed_oracle.h"
#include <stdio.h>
#include <stdlib.h>

/* The test decodes each input from its little-endian bytes, calls one variant
 * of the functions, and encodes the result to bytes with shifts. It does not
 * copy host memory, so the bytes are the same on a little-endian host and on
 * the big-endian SH-2. Each byte is compared with the oracle. The library must
 * give zero differences. Each mutant plants one known bug, and the test must
 * find a difference for each mutant. */
typedef struct {
    const char *name;
    int32_t (*mul_q12)(int32_t a,int32_t b);
    int32_t (*lim_ir)(int32_t value,bool lm);
    bool big_endian; /* the result bytes are stored in the wrong order */
} variant_t;

static int32_t wrap32(int64_t value) {
    uint32_t bits=(uint32_t)((uint64_t)value&UINT32_C(0xFFFFFFFF));
    return bits<=UINT32_C(0x7FFFFFFF)?(int32_t)bits:(int32_t)((int64_t)bits-INT64_C(0x100000000));
}

/* Mutant: the shift rounds toward zero, not toward negative infinity. */
static int32_t mutant_mul_truncate(int32_t a,int32_t b) {
    return wrap32(((int64_t)a*b)/4096);
}

/* Mutant: the result saturates to int32_t, not wraps. */
static int32_t mutant_mul_saturate(int32_t a,int32_t b) {
    int64_t product=(int64_t)a*b;
    int64_t floor=product>=0?product/4096:-((-product+4095)/4096);
    return floor>INT32_MAX?INT32_MAX:(floor<INT32_MIN?INT32_MIN:(int32_t)floor);
}

/* Mutant: lm is ignored. */
static int32_t mutant_lim_ignore_lm(int32_t value,bool lm) {
    (void)lm;
    return sh2o_lim_ir(value,false);
}

static const variant_t library={"library",sh2o_mul_q12,sh2o_lim_ir,false};
static const variant_t mutants[]={
    {"mul_truncate",mutant_mul_truncate,sh2o_lim_ir,false},
    {"mul_saturate",mutant_mul_saturate,sh2o_lim_ir,false},
    {"lim_ignore_lm",sh2o_mul_q12,mutant_lim_ignore_lm,false},
    {"big_endian_result",sh2o_mul_q12,sh2o_lim_ir,true},
};
#define MUTANT_COUNT (sizeof(mutants)/sizeof(mutants[0]))

static int32_t get_i32(const uint8_t *bytes) {
    uint32_t bits=(uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
    return wrap32((int64_t)bits);
}

static void put_i32(uint8_t *bytes,int32_t value,bool big_endian) {
    uint32_t bits=(uint32_t)value;
    for(unsigned i=0;i<4;i++)
        bytes[big_endian?3-i:i]=(uint8_t)(bits>>(8*i));
}

static void run(const variant_t *variant,const fixed_vector_t *vector,uint8_t *out) {
    switch(vector->op) {
    case FIXED_OP_MUL_Q12:
        put_i32(out,variant->mul_q12(get_i32(vector->in),get_i32(vector->in+4)),variant->big_endian);
        break;
    case FIXED_OP_LIM_IR:
        put_i32(out,variant->lim_ir(get_i32(vector->in),vector->in[4]!=0),variant->big_endian);
        break;
    default:
        fprintf(stderr,"fixed: vector has unknown op %u\n",vector->op);
        exit(EXIT_FAILURE);
    }
}

static void print_bytes(const char *label,const uint8_t *bytes,unsigned size) {
    fprintf(stderr,"  %-8s",label);
    for(unsigned i=0;i<size;i++)
        fprintf(stderr," %02x",bytes[i]);
    fprintf(stderr,"\n");
}

/* Compares each byte of each vector. Returns the index of the first vector
 * with a difference, or -1. Prints the first difference. */
static long first_difference(const variant_t *variant,unsigned long *bytes_compared) {
    uint8_t out[FIXED_OUT_MAX];
    for(size_t i=0;i<FIXED_VECTOR_COUNT;i++) {
        const fixed_vector_t *vector=&fixed_vectors[i];
        run(variant,vector,out);
        for(unsigned byte=0;byte<vector->out_size;byte++) {
            *bytes_compared+=1;
            if(out[byte]==vector->out[byte])
                continue;
            fprintf(stderr,"fixed: %s: first difference in vector %zu (%s), result byte %u\n",
                variant->name,i,fixed_op_names[vector->op],byte);
            print_bytes("in",vector->in,vector->in_size);
            print_bytes("expected",vector->out,vector->out_size);
            print_bytes("got",out,vector->out_size);
            return (long)i;
        }
    }
    return -1;
}

int main(void) {
    unsigned long per_op[FIXED_OP_COUNT]={0};
    for(size_t i=0;i<FIXED_VECTOR_COUNT;i++)
        per_op[fixed_vectors[i].op]++;
    for(unsigned op=0;op<FIXED_OP_COUNT;op++) {
        if(per_op[op]==0) {
            fprintf(stderr,"fixed: no vector for op %s\n",fixed_op_names[op]);
            return EXIT_FAILURE;
        }
    }
    unsigned long bytes=0;
    if(first_difference(&library,&bytes)>=0) {
        fprintf(stderr,"fixed: FAIL: the library differs from the oracle\n");
        return EXIT_FAILURE;
    }
    for(size_t m=0;m<MUTANT_COUNT;m++) {
        unsigned long ignored=0;
        if(first_difference(&mutants[m],&ignored)<0) {
            fprintf(stderr,"fixed: FAIL: mutant %s was not caught\n",mutants[m].name);
            return EXIT_FAILURE;
        }
        fprintf(stderr,"fixed: mutant %s caught (expected)\n",mutants[m].name);
    }
    printf("fixed: %zu vectors, %lu bytes, backend %s, zero differences, %zu of %zu mutants caught\n",
        (size_t)FIXED_VECTOR_COUNT,bytes,sh2o_backend(),(size_t)MUTANT_COUNT,(size_t)MUTANT_COUNT);
    return 0;
}
