/* Bridge for the SH-2 model run of the list family.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the library functions sh2o_gte_write, sh2o_gte_read, sh2o_gte_mvmva and
 * sh2o_gte_mvmva_list directly, and the suite counts (sh2o_list_test_counts). This file keeps these functions in the
 * ELF and gives the harness an I/O buffer. Its one wrapper, sh2o_model_list_mac3, is one out-of-line call of
 * sh2o_k_mvb3 (the MVMVA kernel: `_w2` on the SH-2, else C), as benchmarks/k-model/bridge.c does. sh2o_gte_mvmva_list has five arguments: the
 * harness puts the fifth (out) on the stack. Buffer layout (bytes): 0..255 one sh2o_gte_t image, 256..383 the items
 * v[16][2], 384..767 the out rows out[16][6], 768..787 the suite counts c[5], 788..1023 scratch. */
#include "sh2o_list_test.h"
#include "sh2o/k.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_list_io[1024];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts one sh2o_gte_t image at the start of the io buffer");
_Static_assert(SH2O_LIST_MAX==16u,"the io buffer has space for 16 items and 16 out rows");

void sh2o_model_list_mac3(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
void sh2o_model_list_mac3(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3])
{
    sh2o_k_mvb3(w,v,t,out);
}

/* The write order of tests/test_list.c (the gte suite's order); the gte suite is not in this ELF. */
__attribute__((used)) const uint8_t sh2o_model_list_order[SH2O_GTE_ORDER_COUNT]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,
    13,14,16,17,18,19,20,21,22,23,24,25,26,27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,
    54,55,56,57,58,59,60,61,62,63};

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h, sh2o/list.h or sh2o_list_test.h changes and
 * this file does not. */
typedef struct {
    void (*write)(sh2o_gte_t *g,unsigned reg,uint32_t v);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    void (*mvmva)(sh2o_gte_t *g,uint32_t cmd);
    void (*mvmva_list)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
    void (*counts)(uint32_t c[5]);
    const uint8_t *order;
    void (*mac3)(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_gte_write,
    sh2o_gte_read,
    sh2o_gte_mvmva,
    sh2o_gte_mvmva_list,
    sh2o_list_test_counts,
    sh2o_model_list_order,
    sh2o_model_list_mac3,
};
