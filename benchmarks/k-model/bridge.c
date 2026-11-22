/* Bridge for the SH-2 model run of the k family.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The k kernels are inline. Each wrapper below is one out-of-line call of one kernel path: the public entry,
 * the C reference (_c) and, on the SH-2 build, the MAC.W kernels: _w (SR.S-blind, -1) and _w2 (checks SR.S, -2).
 * A wrapper takes at most four arguments (R4 to R7). A wrapper with an int64 result writes it through its last
 * argument, because the model harness reads only R0. The harness writes the inputs into sh2o_model_k_io and gives
 * pointers into it. */
#include "sh2o_k_test.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_k_io[256];

#define SH2O_MODEL_K_PATH(suffix,dot3b,mvb3,gpf3,gpl3) \
    void sh2o_model_k_dot3b##suffix(const int16_t m[3],const int16_t v[3],int32_t tr,int64_t *out); \
    void sh2o_model_k_dot3b##suffix(const int16_t m[3],const int16_t v[3],int32_t tr,int64_t *out) \
    { \
        *out=dot3b(m,v,tr); \
    } \
    void sh2o_model_k_mvb3##suffix(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]); \
    void sh2o_model_k_mvb3##suffix(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]) \
    { \
        mvb3(w,v,t,out); \
    } \
    void sh2o_model_k_gpf3##suffix(const uint32_t w[4],int64_t out[3]); \
    void sh2o_model_k_gpf3##suffix(const uint32_t w[4],int64_t out[3]) \
    { \
        gpf3(w,out); \
    } \
    void sh2o_model_k_gpl3##suffix(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]); \
    void sh2o_model_k_gpl3##suffix(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]) \
    { \
        gpl3(w,mac,sf,out); \
    }

#define SH2O_MODEL_K_SCALAR(suffix,clz32,unr_recip,unr_div) \
    unsigned sh2o_model_k_clz32##suffix(uint32_t v); \
    unsigned sh2o_model_k_clz32##suffix(uint32_t v) \
    { \
        return clz32(v); \
    } \
    uint32_t sh2o_model_k_unr_recip##suffix(uint32_t d); \
    uint32_t sh2o_model_k_unr_recip##suffix(uint32_t d) \
    { \
        return unr_recip(d); \
    } \
    uint32_t sh2o_model_k_unr_div##suffix(uint32_t h,uint32_t sz); \
    uint32_t sh2o_model_k_unr_div##suffix(uint32_t h,uint32_t sz) \
    { \
        return unr_div(h,sz); \
    }

SH2O_MODEL_K_PATH(,sh2o_k_dot3b,sh2o_k_mvb3,sh2o_k_gpf3,sh2o_k_gpl3)
SH2O_MODEL_K_SCALAR(,sh2o_k_clz32,sh2o_k_unr_recip,sh2o_k_unr_div)
SH2O_MODEL_K_PATH(_c,sh2o_k_dot3b_c,sh2o_k_mvb3_c,sh2o_k_gpf3_c,sh2o_k_gpl3_c)
SH2O_MODEL_K_SCALAR(_c,sh2o_k_clz32_c,sh2o_k_unr_recip_c,sh2o_k_unr_div_c)
#if SH2OI_NATIVE
SH2O_MODEL_K_PATH(_w,sh2o_k_dot3b_w,sh2o_k_mvb3_w,sh2o_k_gpf3_w,sh2o_k_gpl3_w)
SH2O_MODEL_K_PATH(_w2,sh2o_k_dot3b_w2,sh2o_k_mvb3_w2,sh2o_k_gpf3_w2,sh2o_k_gpl3_w2)
#endif

typedef struct {
    void (*dot3b)(const int16_t m[3],const int16_t v[3],int32_t tr,int64_t *out);
    void (*mvb3)(const uint32_t w[5],const int16_t v[3],const int32_t t[3],int64_t out[3]);
    void (*gpf3)(const uint32_t w[4],int64_t out[3]);
    void (*gpl3)(const uint32_t w[4],const uint32_t mac[3],unsigned sf,int64_t out[3]);
} sh2o_model_k_path_t;

typedef struct {
    unsigned (*clz32)(uint32_t v);
    uint32_t (*unr_recip)(uint32_t d);
    uint32_t (*unr_div)(uint32_t h,uint32_t sz);
} sh2o_model_k_scalar_t;

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm
 * gives. The typed pointers make the build fail when a signature in sh2o/k.h or sh2o_k_test.h changes and this
 * file does not. The _w then _w2 entries are last, and only in the SH-2 build. */
typedef struct {
    void (*dot3b_part)(uint32_t first,uint32_t n,uint32_t s[2]);
    void (*mvb3_part)(unsigned which,uint32_t first,uint32_t n,uint32_t s[2]);
    void (*gp3_part)(unsigned which,uint32_t first,uint32_t n,uint32_t s[3]);
    sh2o_model_k_path_t entry;
    sh2o_model_k_scalar_t entry_scalar;
    sh2o_model_k_path_t c;
    sh2o_model_k_scalar_t c_scalar;
#if SH2OI_NATIVE
    sh2o_model_k_path_t w;
    sh2o_model_k_path_t w2;
#endif
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_k_test_dot3b_part,
    sh2o_k_test_mvb3_part,
    sh2o_k_test_gp3_part,
    {sh2o_model_k_dot3b,sh2o_model_k_mvb3,sh2o_model_k_gpf3,sh2o_model_k_gpl3},
    {sh2o_model_k_clz32,sh2o_model_k_unr_recip,sh2o_model_k_unr_div},
    {sh2o_model_k_dot3b_c,sh2o_model_k_mvb3_c,sh2o_model_k_gpf3_c,sh2o_model_k_gpl3_c},
    {sh2o_model_k_clz32_c,sh2o_model_k_unr_recip_c,sh2o_model_k_unr_div_c},
#if SH2OI_NATIVE
    {sh2o_model_k_dot3b_w,sh2o_model_k_mvb3_w,sh2o_model_k_gpf3_w,sh2o_model_k_gpl3_w},
    {sh2o_model_k_dot3b_w2,sh2o_model_k_mvb3_w2,sh2o_model_k_gpf3_w2,sh2o_model_k_gpl3_w2},
#endif
};
