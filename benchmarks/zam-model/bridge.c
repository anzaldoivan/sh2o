/* Bridge for the SH-2 model run of the zam family (the experimental delegate).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* sh2_model-option: SH2O_ZAM
 * The line above tells scripts/sh2_model.sh to build and run this family only with SH2O_ZAM=ON: this file and the
 * suite need the delegate (sh2o/zam_experimental.h).
 * The model harness calls the suite's resumable digest (sh2o_zam_test_seed, sh2o_zam_test_digest_part), the set (A)
 * image entry, the zam-3 to zam-5 trials and their case draws, the library functions sh2o_gte_command and
 * sh2o_gte_read, and the delegate and core entries of TransposeMatrix, SetFogNearFar and the MVMVA list directly, and
 * reads and writes the delegate's counters and its bypass switch. This file has no wrapper: it only keeps these in
 * the ELF and gives the harness an I/O buffer. Buffer layout (bytes): 0..35 the tally (sh2o_zam_tally), 64..319 the
 * image of a set (A) vector, 320..575 and 576..831 two copies of it, 832..843 its three commands, 848..867 a trial
 * (sh2o_zam_trial), 872..899 the list trial (sh2o_zam_ltrial), 904..907 the fog trial's generator, 908..1023
 * scratch; the timed trial calls (zam_harness.c ZAM_IO_*): 1024..1215 six matrices (source, m1, delegate m0 and m1,
 * core m0 and m1), 1216..1227 (a, b, h), 1228..1239 the generators of zam-3, zam-4 and zam-5, 1240..1243 the list's
 * command, 1280..1535 its v, 1536..2303 the delegate's out rows, 2304..3071 the core's, 3072..4095 scratch. */
#include "sh2o/list.h"
#include "sh2o/zam_experimental.h"
#include "sh2o_zam_test.h"

__attribute__((used,aligned(8))) uint8_t sh2o_model_zam_io[4096];

_Static_assert(sizeof(sh2o_gte_t)==256u,"the harness puts sh2o_gte_t images at 64, 320 and 576 in the io buffer");
_Static_assert(sizeof(sh2o_zam_tally)==36u,"the harness reads the tally as nine words");
_Static_assert(sizeof(sh2o_zam_trial)==20u,"the harness reads a trial as five words");
_Static_assert(sizeof(sh2o_zam_ltrial)==28u,"the harness reads the list trial as seven words");
_Static_assert(sizeof(sh2o_psyq_matrix_t)==32u,"the harness puts matrices 32 bytes apart from 1024 in the io buffer");
_Static_assert(SH2O_ZAM_LIST_MAX==32u,"the harness gives the list 32 items of v and of out in the io buffer");
_Static_assert(sizeof(int)==4u,"the harness writes sh2o_zam_bypass as one word");

/* The model harness reads this table from the ELF and compares each entry with the symbol address that nm gives.
 * The typed pointers make the build fail when a signature in sh2o/gte.h, sh2o/zam_experimental.h or
 * sh2o_zam_test.h changes and this file does not. */
typedef struct {
    void (*seed)(sh2o_zam_tally *t);
    int (*digest_part)(uint32_t first,uint32_t n,sh2o_zam_tally *t);
    void (*vector_image)(uint32_t i,sh2o_zam_tally *t,sh2o_gte_t *g,uint32_t cmd[3]);
    void (*transpose_trial)(sh2o_zam_trial *t);
    void (*fog_trial_part)(uint32_t first,uint32_t n,sh2o_zam_trial *t,uint32_t *x);
    void (*list_trial)(sh2o_zam_ltrial *t);
    void (*transpose_case)(uint32_t k,uint32_t *x,sh2o_psyq_matrix_t *src,sh2o_psyq_matrix_t *m1);
    void (*fog_case)(uint32_t k,uint32_t *x,sh2o_gte_t *img,int32_t abh[3]);
    unsigned (*list_case)(uint32_t i,uint32_t *x,sh2o_gte_t *img,uint32_t *cmd,uint32_t (*v)[2]);
    sh2o_psyq_matrix_t *(*zam_transpose)(const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1);
    sh2o_psyq_matrix_t *(*psyq_transpose)(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1);
    int (*zam_fog)(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h);
    void (*psyq_fog)(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h);
    int (*zam_list)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
    void (*gte_list)(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
    int (*command)(sh2o_gte_t *g,uint32_t cmd);
    uint32_t (*read)(const sh2o_gte_t *g,unsigned reg);
    int *bypass;
    uint32_t *delegated;
    uint32_t *refused;
} sh2o_model_bridge_t;

__attribute__((used)) const sh2o_model_bridge_t sh2o_model_bridge={
    sh2o_zam_test_seed,
    sh2o_zam_test_digest_part,
    sh2o_zam_test_vector_image,
    sh2o_zam_transpose_trial,
    sh2o_zam_fog_trial_part,
    sh2o_zam_list_trial,
    sh2o_zam_transpose_case,
    sh2o_zam_fog_case,
    sh2o_zam_list_case,
    sh2o_zam_TransposeMatrix,
    sh2o_psyq_TransposeMatrix,
    sh2o_zam_SetFogNearFar,
    sh2o_psyq_SetFogNearFar,
    sh2o_zam_mvmva_list,
    sh2o_gte_mvmva_list,
    sh2o_gte_command,
    sh2o_gte_read,
    &sh2o_zam_bypass,
    &sh2o_zam_delegated,
    &sh2o_zam_refused,
};
