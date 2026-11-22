/* Byte-exact oracle test and suite of the MVMVA list.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/gte.h"
#include "sh2o/list.h"
#include "sh2o_gte_test.h"
#include "sh2o_list_test.h"
#include "list_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each oracle vector goes through sh2o_gte_write and sh2o_gte_mvmva_list. An input image is its 64 registers
 * written in the gte suite's order on a zeroed image; then the list of cmd on its n items. Results: the 16 out rows
 * (rows n..15 stay 0) and the 64 reads of sh2o_gte_read. Words are little-endian bytes. Then the test runs the
 * suite (tests/suite) and checks its counts.
 * Modes: no argument, the test; --mutant <name>, the count of vectors that differ (CMakeLists.txt builds a mutant
 * from a changed copy of src/ and include/ and passes when the count is not 0). */

/* The counts of the suite: runs, items, prefixes that differ, reorders caught, reorders planted. */
static const uint32_t list_counts[5]=SH2O_LIST_COUNTS;

/* The write order of the gte suite (sh2o_gte_order); the list suite has its own copy. */
static const uint8_t list_order[SH2O_GTE_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,
    22,23,24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,
    62,63};

static uint32_t get_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}

static void put_u32(uint8_t *bytes,uint32_t value) {
    for(unsigned i=0;i<4;i++)
        bytes[i]=(uint8_t)(value>>(8*i));
}

static void run(const list_vector_t *vector,uint8_t *out) {
    sh2o_gte_t g;
    uint32_t items[SH2O_LIST_MAX][2],rows[SH2O_LIST_MAX][6];
    unsigned n,k,c;
    memset(&g,0,sizeof g);
    memset(rows,0,sizeof rows);
    for(k=0;k<SH2O_GTE_ORDER_COUNT;k++)
        sh2o_gte_write(&g,list_order[k],get_u32(vector->in+4*list_order[k]));
    n=(unsigned)get_u32(vector->in+260);
    if(vector->op!=LIST_OP_LIST||n>SH2O_LIST_MAX) {
        fprintf(stderr,"list: vector has unknown op %u or n %u\n",vector->op,n);
        exit(EXIT_FAILURE);
    }
    for(k=0;k<SH2O_LIST_MAX;k++)
        for(c=0;c<2;c++)
            items[k][c]=get_u32(vector->in+264+8*k+4*c);
    sh2o_gte_mvmva_list(&g,get_u32(vector->in+256),(const uint32_t (*)[2])items,n,rows);
    for(k=0;k<SH2O_LIST_MAX;k++)
        for(c=0;c<6;c++)
            put_u32(out+24*k+4*c,rows[k][c]);
    for(k=0;k<64;k++)
        put_u32(out+384+4*k,sh2o_gte_read(&g,k));
}

/* Returns the count of vectors whose result differs, and prints the first difference. */
static unsigned long differences(const char *name,unsigned long *bytes_compared) {
    uint8_t out[LIST_OUT_MAX];
    unsigned long count=0;
    for(size_t i=0;i<LIST_VECTOR_COUNT;i++) {
        const list_vector_t *vector=&list_vectors[i];
        run(vector,out);
        *bytes_compared+=vector->out_size;
        if(memcmp(out,vector->out,vector->out_size)==0)
            continue;
        if(count==0) {
            unsigned byte=0;
            while(out[byte]==vector->out[byte])
                byte++;
            fprintf(stderr,"list: %s: first difference in vector %zu, result byte %u: expected 0x%02x, got 0x%02x\n",
                name,i,byte,vector->out[byte],out[byte]);
        }
        count++;
    }
    return count;
}

int main(int argc,char **argv) {
    unsigned long bytes=0;
    uint32_t c[5];
    int rc;
    if(argc==3&&strcmp(argv[1],"--mutant")==0) {
        unsigned long count=differences(argv[2],&bytes);
        printf("mutant %s: %lu differences in %zu oracle vectors\n",argv[2],count,(size_t)LIST_VECTOR_COUNT);
        return count>0?0:EXIT_FAILURE;
    }
    if(argc!=1) {
        fprintf(stderr,"usage: %s [--mutant <name>]\n",argv[0]);
        return EXIT_FAILURE;
    }
    if(differences("library",&bytes)!=0) {
        fprintf(stderr,"list: FAIL: the library differs from the oracle\n");
        return EXIT_FAILURE;
    }
    printf("list: %zu vectors, %lu bytes, zero differences\n",(size_t)LIST_VECTOR_COUNT,bytes);
    rc=sh2o_list_test_suite();
    sh2o_list_test_counts(c);
    printf("list: suite: %lu runs, %lu items, %lu prefixes differ, reorder caught %lu of %lu\n",(unsigned long)c[0],
        (unsigned long)c[1],(unsigned long)c[2],(unsigned long)c[3],(unsigned long)c[4]);
    if(rc!=0) {
        fprintf(stderr,"list: FAIL: the suite returned 0x%X\n",(unsigned)rc);
        return EXIT_FAILURE;
    }
    if(memcmp(c,list_counts,sizeof c)!=0) {
        fprintf(stderr,"list: FAIL: the suite counts are not 256, 2144, 0, 238, 238\n");
        return EXIT_FAILURE;
    }
    printf("list: suite passed\n");
    return 0;
}
