/* Case set, write order and pinned digest of the gte suite.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_GTE_TEST_H
#define SH2O_GTE_TEST_H

/* A program on the SH-2 can compile tests/suite/sh2o_gte_test_suite.c with this header: they use no C library.
 * tests/test_gte.c runs the suite on the host, and tests/gte_model_check.py makes its case set again in Python. */
#include <stdint.h>

#include "sh2o/gte.h"

#define SH2O_GTE_SEED 0x53484733u
#define SH2O_GTE_OPS 14u
#define SH2O_GTE_PER_OP_LOG2 8
#define SH2O_GTE_PER_OP (1u << SH2O_GTE_PER_OP_LOG2)
#define SH2O_GTE_CASES (SH2O_GTE_OPS * SH2O_GTE_PER_OP + 256u) /* 3,840: 14 x 256 + every MVMVA selector */
#define SH2O_DIGEST_GTE 0xA50D51B1u /* FNV-1a 32 of the register reads over the case set */
#define SH2O_GTE_ORDER_COUNT 61u

/* The write order of a case (sh2o_gte_test_suite.c) and of an oracle vector (tests/test_gte.c): every register
 * but 15, 29 and 31 (writes with a side effect or no effect), 28 before IR1-3. */
extern const uint8_t sh2o_gte_order[SH2O_GTE_ORDER_COUNT];

/* The suite (sh2o_gte_test_suite.c) prints nothing. sh2o_gte_test_suite(): 0 = pass; else 0x200 + 1 + the index
 * of the failing known answer (0x2FF: a command returned -1), or 0x300 for a digest mismatch.
 * sh2o_gte_test_digest(): the digest of the case set.
 * sh2o_gte_test_digest_part(first, n, s): the digest over cases first..first+n-1, resumed: s = {generator x, hash},
 * seeded {SH2O_GTE_SEED, 0x811C9DC5}; call it with consecutive ranges from 0 (the cases are sequential). The digest
 * above is the seed and one part over the case set. It returns the known-answer result of its range: 0, or 0x200 + 1
 * + the index of the failing known answer (0x2FF: a command returned -1), as sh2o_gte_test_suite(). */
int sh2o_gte_test_suite(void);
uint32_t sh2o_gte_test_digest(void);
int sh2o_gte_test_digest_part(uint32_t first,uint32_t n,uint32_t s[2]);

/* The MAC stage switch (src/sh2o_gte.c, built with -DSH2O_MVB3_SWITCH=1 by an SH-2 bench only): mode 0 the
 * dispatch line, 1 the flagged path forced, 2 the C kernel, 3 the wide kernel; fast and flagged path counts. */
#if defined(SH2O_MVB3_SWITCH) && SH2O_MVB3_SWITCH
extern int sh2o_gte_mvb3_mode;
extern uint32_t sh2o_gte_mvb3_fast;
extern uint32_t sh2o_gte_mvb3_flagged;
#endif

#endif /* SH2O_GTE_TEST_H */
