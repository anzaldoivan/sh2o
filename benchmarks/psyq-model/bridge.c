/* Bridge for the SH-2 model run of the psyq family.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The model harness calls the 19 PSY-Q bodies directly (each takes at most four argument words and gives its result
 * in R0), the resumable digest of the psyq suite, and the suite's case entry points sh2o_psyq_test_case_draw and
 * sh2o_psyq_test_case_hash. This file has one helper, sh2o_model_psyq_args, and an I/O buffer: the digest state s[2],
 * the four argument words of a body and one drawn case. */
#include <stddef.h>

#include "sh2o_psyq_test.h"

typedef struct {
    uint32_t s[2];
    uint32_t args[4];
    sh2o_psyq_test_case_t c;
} sh2o_model_psyq_io_t;

__attribute__((used,aligned(8))) sh2o_model_psyq_io_t sh2o_model_psyq_io;

_Static_assert(offsetof(sh2o_model_psyq_io_t,s)==0u,"the harness puts the digest state at io byte 0");
_Static_assert(offsetof(sh2o_model_psyq_io_t,args)==8u,"the harness reads the argument words at io byte 8");
_Static_assert(offsetof(sh2o_model_psyq_io_t,c)==24u,"the harness puts the case at io byte 24");
_Static_assert(offsetof(sh2o_psyq_test_case_t,body)==0u,"the harness reads the body index at case byte 0");

/* The four argument words of case c's body, as the suite's sh2o_psyq_call passes them: g, then the address of each
 * region (the field of its kind), then the integer arguments; 0 for an argument the body does not take. */
void sh2o_model_psyq_args(sh2o_psyq_test_case_t *c,uint32_t out[4]) {
    uint32_t k,n=1;
    out[0]=(uint32_t)(uintptr_t)&c->g;
    for(k=0;k<c->nr;k++,n++) {
        sh2o_psyq_test_region_t *r=&c->r[k];
        switch(r->kind) {
        case 'M': out[n]=(uint32_t)(uintptr_t)&r->m; break;
        case 'V': out[n]=(uint32_t)(uintptr_t)&r->v; break;
        case 'S': out[n]=(uint32_t)(uintptr_t)&r->s; break;
        case 'L': out[n]=(uint32_t)(uintptr_t)&r->l; break;
        default: out[n]=(uint32_t)(uintptr_t)&r->k;
        }
    }
    for(k=0;n<4;k++,n++)
        out[n]=(uint32_t)c->a[k];
}

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/psyq.h or sh2o_psyq_test.h changes and this file
 * does not. The bodies are in the suite's case order. */
typedef struct {
    int (*digest_part)(uint32_t first,uint32_t n,uint32_t s[2]);
    void (*case_draw)(uint32_t i,uint32_t s[2],sh2o_psyq_test_case_t *c);
    void (*case_hash)(const sh2o_psyq_test_case_t *c,uint32_t ret,uint32_t s[2]);
    void (*args)(sh2o_psyq_test_case_t *c,uint32_t out[4]);
    sh2o_psyq_vector_t *(*apply)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_svector_t *v0,
        sh2o_psyq_vector_t *v1);
    sh2o_psyq_vector_t *(*apply_lv)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v0,
        sh2o_psyq_vector_t *v1);
    sh2o_psyq_matrix_t *(*comp)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,const sh2o_psyq_matrix_t *m1,
        sh2o_psyq_matrix_t *m2);
    void (*init)(sh2o_gte_t *g);
    int (*pop)(sh2o_gte_t *g,sh2o_psyq_mstack_t *s);
    int (*push)(sh2o_gte_t *g,sh2o_psyq_mstack_t *s);
    void (*rot_trans)(sh2o_gte_t *g,const sh2o_psyq_svector_t *v0,sh2o_psyq_vector_t *v1,int32_t *flag);
    void (*back)(sh2o_gte_t *g,int32_t r,int32_t gr,int32_t b);
    void (*color)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);
    void (*far)(sh2o_gte_t *g,int32_t r,int32_t gr,int32_t b);
    void (*fog)(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h);
    void (*offset)(sh2o_gte_t *g,int32_t ofx,int32_t ofy);
    void (*screen)(sh2o_gte_t *g,int32_t h);
    void (*light)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);
    void (*rot)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);
    void (*set_trans)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);
    sh2o_psyq_matrix_t *(*trans)(sh2o_gte_t *g,sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v);
    sh2o_psyq_matrix_t *(*transpose)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1);
    int32_t (*ratan2)(sh2o_gte_t *g,int32_t y,int32_t x);
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_psyq_test_digest_part,
    sh2o_psyq_test_case_draw,
    sh2o_psyq_test_case_hash,
    sh2o_model_psyq_args,
    sh2o_psyq_ApplyMatrix,
    sh2o_psyq_ApplyMatrixLV,
    sh2o_psyq_CompMatrix,
    sh2o_psyq_InitGeom,
    sh2o_psyq_PopMatrix,
    sh2o_psyq_PushMatrix,
    sh2o_psyq_RotTrans,
    sh2o_psyq_SetBackColor,
    sh2o_psyq_SetColorMatrix,
    sh2o_psyq_SetFarColor,
    sh2o_psyq_SetFogNearFar,
    sh2o_psyq_SetGeomOffset,
    sh2o_psyq_SetGeomScreen,
    sh2o_psyq_SetLightMatrix,
    sh2o_psyq_SetRotMatrix,
    sh2o_psyq_SetTransMatrix,
    sh2o_psyq_TransMatrix,
    sh2o_psyq_TransposeMatrix,
    sh2o_psyq_ratan2,
};
