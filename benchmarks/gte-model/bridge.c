/* Bridge for the SH-2 model run of the gte family.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the library functions sh2o_gte_write, sh2o_gte_command, sh2o_gte_read, sh2o_gte_nrtps,
 * sh2o_gte_nrtpt and sh2o_gte_dpct directly (each takes at most three arguments and gives its result in R0), and
 * the resumable digest of the gte suite, and sh2o_gte_mvmva. This file keeps these functions in the ELF and gives the
 * harness an I/O buffer. Its one wrapper, sh2o_model_gte_mac3, is one out-of-line call of sh2o_k_mvb3 (the MVMVA
 * kernel: `_w2` on the SH-2, else C), as benchmarks/k-model/bridge.c does; the harness writes its inputs into the
 * buffer. Buffer layout (bytes): 0..255 one sh2o_gte_t image, 256..263 the digest state s[2], 264..511 scratch. The image is larger than the 256 bytes of the k bridge's buffer: an oracle vector has up to 264
 * input bytes and 260 result bytes. */
#include "sh2o_gte_test.h"
#include "sh2o/k.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_gte_io[512];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts one sh2o_gte_t image at the start of the io buffer");

void sh2o_model_gte_mac3(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
void sh2o_model_gte_mac3(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3])
{
    sh2o_k_mvb3(w,v,t,out);
}

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h or sh2o_gte_test.h changes and this file does
 * not. The harness reads the write order of the suite (sh2o_gte_order) from the ELF. */
typedef struct {
    int (*digest_part)(uint32_t first,uint32_t n,uint32_t s[2]);
    void (*write)(sh2o_gte_t *g,unsigned reg,uint32_t v);
    int (*command)(sh2o_gte_t *g,uint32_t cmd);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    const uint8_t *order;
    void (*nrtps)(sh2o_gte_t *g);
    void (*nrtpt)(sh2o_gte_t *g);
    void (*dpct)(sh2o_gte_t *g,uint32_t cmd);
    void (*mac3)(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_gte_test_digest_part,
    sh2o_gte_write,
    sh2o_gte_command,
    sh2o_gte_read,
    sh2o_gte_order,
    sh2o_gte_nrtps,
    sh2o_gte_nrtpt,
    sh2o_gte_dpct,
    sh2o_model_gte_mac3,
};
