/* Case set and pinned digest of the psyq suite.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_PSYQ_TEST_H
#define SH2O_PSYQ_TEST_H

/* A program on the SH-2 can compile tests/suite/sh2o_psyq_test_suite.c with this header: they use no C library.
 * tests/test_psyq.c runs the suite on the host. */
#include <stdint.h>

#include "sh2o/psyq.h"
#ifdef __cplusplus
extern "C" {
#endif

#define SH2O_PSYQ_SEED 0x50535951u
#define SH2O_PSYQ_BODIES 19u
#define SH2O_PSYQ_PER_LOG2 6
#define SH2O_PSYQ_PER (1u << SH2O_PSYQ_PER_LOG2)
#define SH2O_PSYQ_CASES (SH2O_PSYQ_BODIES * SH2O_PSYQ_PER) /* 1,216: 19 x 64 */
#define SH2O_DIGEST_PSYQ 0xE430E70Bu /* FNV-1a 32 of the returns, regions and register reads over the case set */

/* The suite (sh2o_psyq_test_suite.c) prints nothing. sh2o_psyq_test_suite(): 0 = pass; else 0x400 + 1 + the index
 * of the failing known answer (0x4FF: a pointer return names no argument), or 0x500 for a digest mismatch.
 * sh2o_psyq_test_digest(): the digest of the case set.
 * sh2o_psyq_test_digest_part(first, n, s): the digest over cases first..first+n-1, resumed: s = {generator x, hash},
 * seeded {SH2O_PSYQ_SEED, 0x811C9DC5}; call it with consecutive ranges from 0 (the cases are sequential). The digest
 * above is the seed and one part over the case set. The ratan2 known answers run in the part whose range ends at
 * case SH2O_PSYQ_CASES. It returns the known-answer result of its range: 0, or 0x400 + 1 + the index of the failing
 * known answer (0x4FF: a pointer return), as sh2o_psyq_test_suite(). */
int sh2o_psyq_test_suite(void);
uint32_t sh2o_psyq_test_digest(void);
int sh2o_psyq_test_digest_part(uint32_t first,uint32_t n,uint32_t s[2]);

/* One region of a case: kind M MATRIX, V VECTOR, S SVECTOR, L long, K matrix stack; only the field of its kind is
 * used. */
typedef struct sh2o_psyq_test_region {
    char kind;
    sh2o_psyq_matrix_t m;
    sh2o_psyq_vector_t v;
    sh2o_psyq_svector_t s;
    int32_t l;
    sh2o_psyq_mstack_t k;
} sh2o_psyq_test_region_t;

/* One drawn case: body (its index in case order), nr regions, integer arguments a (0 when not drawn), the image g. */
typedef struct sh2o_psyq_test_case {
    uint32_t body,nr;
    int32_t a[3];
    sh2o_gte_t g;
    sh2o_psyq_test_region_t r[3];
} sh2o_psyq_test_case_t;

/* For a lane that calls the bodies itself (the SH-2 model): sh2o_psyq_test_case_draw(i, s, c) draws case i into c from
 * generator s[0] and advances s[0]; the caller calls the body; sh2o_psyq_test_case_hash(c, ret, s) puts the return
 * word ret (void 0; pointer: the index of the argument it equals, counting g as 0, else 0xFFFFFFFF; int its value),
 * the regions and the 64 register reads of c into the hash s[1]. Cases 0..1215 in order give the pinned digest. */
void sh2o_psyq_test_case_draw(uint32_t i,uint32_t s[2],sh2o_psyq_test_case_t *c);
void sh2o_psyq_test_case_hash(const sh2o_psyq_test_case_t *c,uint32_t ret,uint32_t s[2]);

#ifdef __cplusplus
}
#endif
#endif
