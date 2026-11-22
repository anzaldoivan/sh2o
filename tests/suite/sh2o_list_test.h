/* Case set of the list suite.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_LIST_TEST_H
#define SH2O_LIST_TEST_H

/* A program on the SH-2 can compile tests/suite/sh2o_list_test_suite.c with this header: they use no C library.
 * tests/test_list.c runs the suite on the host. The seed is the gte suite's (SH2O_GTE_SEED). */
#include <stdint.h>

#include "sh2o/list.h"
#include "sh2o_gte_test.h"

#define SH2O_LIST_RUNS 256u
#define SH2O_LIST_MAX 16u
/* The counts of the suite (sh2o_list_test_counts): runs, items, prefixes that differ, reorders caught, reorders
 * planted. tests/test_list.c and the SH-2 model harness compare with them. */
#define SH2O_LIST_COUNTS {256u,2144u,0u,238u,238u}

/* The suite (sh2o_list_test_suite.c) prints nothing. sh2o_list_test_suite(): 0 = pass; else 0x601 (a list prefix
 * differs from its singles) or 0x602 (a planted reorder not caught, or none planted).
 * sh2o_list_test_counts(c): runs, items, prefixes that differ, reorders caught, reorders planted. */
int sh2o_list_test_suite(void);
void sh2o_list_test_counts(uint32_t c[5]);

#endif /* SH2O_LIST_TEST_H */
