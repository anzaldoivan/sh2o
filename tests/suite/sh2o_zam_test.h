/* Case sets, trials and image entry of the delegate suite.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_ZAM_TEST_H
#define SH2O_ZAM_TEST_H

/* Only for a build with SH2O_ZAM=1 (the CMake option SH2O_ZAM). tests/test_zam.c runs the suite and the trials on
 * the host and checks the pinned figures; a program on the SH-2 can compile tests/suite/sh2o_zam_test_suite.c with
 * this header and call the entry or the parts. The sets use the k suite's vectors and the gte suite's seed. */
#include <stdint.h>

#include "sh2o/psyq.h"
#include "sh2o_gte_test.h"
#include "sh2o_k_test.h"

#define SH2O_ZAM_SEEDED (3u * 256u + 256u * 16u) /* (B): 256 RTPS, 256 RTPT, 256 MVMVA, every selector x 16 */
#define SH2O_ZAM_GATE_ITEMS 12u                 /* (C): 11 edge rows, then the domain checks by entry */
#define SH2O_ZAM_ITEMS (SH2O_VEC_COUNT + SH2O_ZAM_SEEDED + SH2O_ZAM_GATE_ITEMS) /* (A), (B), (C) in this order */

/* Pinned figures of the full suite (counts, digest; mac3: sh2o_zam_delegated and sh2o_zam_refused after it) and of
 * the zam-3 to zam-5 trials. tests/test_zam.c and the model harness check them. */
#define SH2O_ZAM_PIN_CASES 345321u
#define SH2O_ZAM_PIN_DELEGATED 101380u
#define SH2O_ZAM_PIN_REFUSED 243941u
#define SH2O_ZAM_PIN_DIGEST 0xD444928Cu
#define SH2O_ZAM_PIN_MAC3_DELEGATED 154694u
#define SH2O_ZAM_PIN_MAC3_REFUSED 162410u
#define SH2O_ZAM_PIN_TR_CASES 8198u
#define SH2O_ZAM_PIN_TR_DELEGATED 8198u
#define SH2O_ZAM_PIN_FOG_CASES 24832u
#define SH2O_ZAM_PIN_FOG_DELEGATED 5122u
#define SH2O_ZAM_PIN_FOG_REFUSED 19710u
#define SH2O_ZAM_PIN_LIST_RUNS 1024u
#define SH2O_ZAM_PIN_LIST_ITEMS 12544u
#define SH2O_ZAM_PIN_LIST_DELEGATED 549u
#define SH2O_ZAM_PIN_LIST_REFUSED 475u
#define SH2O_ZAM_PIN_LIST_PLANTED 547u
#define SH2O_ZAM_PIN_LIST_CAUGHT 547u

/* zam-4's items: 6 x 4096 drawn (a, b, h), then the edge set 8 x 8 x 4; its generator's seed */
#define SH2O_ZAM_FOG_ITEMS (6u * 4096u + 8u * 8u * 4u)
#define SH2O_ZAM_FOG_SEED 0x464F4721u

/* zam-3's sources (each two calls: apart and aliased) and its generator's seed; zam-5's generator's seed */
#define SH2O_ZAM_TR_SOURCES 4099u
#define SH2O_ZAM_TR_SEED 0x54524E53u
#define SH2O_ZAM_LIST_SEED (SH2O_GTE_SEED ^ 0x4C495354u)

/* The state of a run: the counts of (A) to (C), the digest h (FNV-1a 32), rc (the first failure) and the generator
 * state of each set: xa the vectors and ya the images of (A), xb (B), xc (C). */
typedef struct sh2o_zam_tally {
    uint32_t cases,delegated,refused,h;
    int rc;
    uint32_t xa,ya,xb,xc;
} sh2o_zam_tally;

/* A trial's counts; maxerr: the largest error of a mismatch (zam-2 only). */
typedef struct sh2o_zam_trial {
    unsigned long cases,delegated,refused,mismatches;
    long maxerr;
} sh2o_zam_trial;

/* zam-5's counts: planted = planted swaps, caught = the planted swaps that the list found. */
typedef struct sh2o_zam_ltrial {
    unsigned long runs,items,delegated,refused,mismatches,planted,caught;
} sh2o_zam_ltrial;

/* The suite (sh2o_zam_test_suite.c) prints nothing.
 * sh2o_zam_test_seed(t): the start state (counts 0, h 0x811C9DC5, rc 0, the seeds of each set) and
 * sh2o_zam_delegated = sh2o_zam_refused = 0.
 * sh2o_zam_test_digest_part(first, n, t): items first..first+n-1 of SH2O_ZAM_ITEMS ((A) SH2O_VEC_COUNT, then (B)
 * SH2O_ZAM_SEEDED, then (C) SH2O_ZAM_GATE_ITEMS), resumed from t; call it with consecutive ranges of each set from
 * the set's first item (each set has its own generator). Returns t->rc. The seed and one part over all items give
 * the counts, digest and rc of the full suite (before its 0x4000 and 0x4001 checks).
 * sh2o_zam_test_suite(t, nvec, nseeded): the seed, the first nvec items of (A), the first nseeded of (B), all of
 * (C). 0 = pass; else 0x1000 (A), 0x2000 (B), 0x3000 (C, edge << 4) | 1 registers differ, 2 delegated outside by
 * the command, 3 delegation not as required, 0xFF command returned -1; 0x3F01/0x3F02 an entry over 0x8000
 * delegated; 0x4000 no delegated case, 0x4001 no refused case.
 * Trials, outside the digest and the counts above: zam-2 sh2o_zam_ratan2_trial (t[0] grid, t[1] xorshift, t[2]
 * edges; a measurement), zam-3 sh2o_zam_transpose_trial, zam-4 sh2o_zam_fog_trial, zam-5 sh2o_zam_list_trial. The
 * codes of tests/test_zam.c: zam-3 0x5001 a mismatch; zam-4 0x6001 a mismatch, 0x6002 no delegated or no refused
 * case; zam-5 0x7001 a mismatch, 0x7002 no delegated or no refused run, 0x7003 the planted swap never caught.
 * sh2o_zam_test_vector_image(i, t, g, cmd): the image *g and the three commands cmd[0..2] (MVMVA, RTPS, RTPT) of
 * vector i of (A), as the suite runs them; call it with i = 0, 1, ... from the seed (t->xa and t->ya advance).
 * sh2o_zam_fog_trial_part(first, n, t, x): items first..first+n-1 of SH2O_ZAM_FOG_ITEMS of zam-4, resumed from t
 * and the generator *x (SH2O_ZAM_FOG_SEED at item 0); call it with consecutive ranges from 0. One part over all
 * items gives sh2o_zam_fog_trial.
 * Case draws (each trial runs on them; the model harness chains them to time the calls on the same inputs):
 * sh2o_zam_transpose_case(k, x, src, m1): zam-3's source k < SH2O_ZAM_TR_SOURCES into *src and its m1 (the apart
 * call's destination) into *m1, from *x (SH2O_ZAM_TR_SEED at k = 0; call it with k = 0, 1, ...).
 * sh2o_zam_fog_case(k, x, img, abh): zam-4's item k: (a, b, h) into abh[0..2] and its image into *img, from *x
 * (SH2O_ZAM_FOG_SEED at k = 0; call it with k = 0, 1, ...).
 * sh2o_zam_list_case(i, x, img, cmd, v): zam-5's run i: image *img, command *cmd, v[0..n-1]; returns n, from *x
 * (SH2O_ZAM_LIST_SEED at i = 0; call it with i = 0, 1, ...). */
void sh2o_zam_test_seed(sh2o_zam_tally *t);
int sh2o_zam_test_digest_part(uint32_t first,uint32_t n,sh2o_zam_tally *t);
void sh2o_zam_test_vector_image(uint32_t i,sh2o_zam_tally *t,sh2o_gte_t *g,uint32_t cmd[3]);
void sh2o_zam_fog_trial_part(uint32_t first,uint32_t n,sh2o_zam_trial *t,uint32_t *x);
void sh2o_zam_transpose_case(uint32_t k,uint32_t *x,sh2o_psyq_matrix_t *src,sh2o_psyq_matrix_t *m1);
void sh2o_zam_fog_case(uint32_t k,uint32_t *x,sh2o_gte_t *img,int32_t abh[3]);
unsigned sh2o_zam_list_case(uint32_t i,uint32_t *x,sh2o_gte_t *img,uint32_t *cmd,uint32_t (*v)[2]);
int sh2o_zam_test_suite(sh2o_zam_tally *t,uint32_t nvec,uint32_t nseeded);
void sh2o_zam_ratan2_trial(sh2o_zam_trial t[3]);
void sh2o_zam_transpose_trial(sh2o_zam_trial *t);
void sh2o_zam_fog_trial(sh2o_zam_trial *t);
void sh2o_zam_list_trial(sh2o_zam_ltrial *t);

/* Image entry (sh2o_zam_test_suite.c; for a program on the SH-2): the zam suite and the zam-3/zam-4 trials into
 * w[SH2O_ZAM_W_*], now() stamps around each (the image's clock); returns rc as tests/test_zam.c's codes.
 * The entry's suite is a fixed subset (the full suite is too long for one boot of the image): the first
 * SH2O_ZAM_SUB_VEC vectors of (A), the first SH2O_ZAM_SUB_SEEDED cases of (B) (its drawn RTPS/RTPT/MVMVA part),
 * all of (C). tests/test_zam.c runs the full suite. */
#define SH2O_ZAM_SUB_VEC 512u
#define SH2O_ZAM_SUB_SEEDED 768u
enum {
    SH2O_ZAM_W_RC,
    SH2O_ZAM_W_CASES,
    SH2O_ZAM_W_DELEG,
    SH2O_ZAM_W_REFUSED,
    SH2O_ZAM_W_DIGEST,
    SH2O_ZAM_W_MAC3_DELEG,
    SH2O_ZAM_W_MAC3_REFUSED,
    SH2O_ZAM_W_MAC,
    SH2O_ZAM_W_TR_CASES,
    SH2O_ZAM_W_TR_DELEG,
    SH2O_ZAM_W_TR_MISM,
    SH2O_ZAM_W_FOG_CASES,
    SH2O_ZAM_W_FOG_DELEG,
    SH2O_ZAM_W_FOG_REFUSED,
    SH2O_ZAM_W_FOG_MISM,
    SH2O_ZAM_W_T0,
    SH2O_ZAM_W_T1,
    SH2O_ZAM_W_Z0,
    SH2O_ZAM_W_Z1,
    SH2O_ZAM_W_SIZE
};
int sh2o_zam_test_entry(uint32_t *w,uint32_t (*now)(void));

#endif /* SH2O_ZAM_TEST_H */
