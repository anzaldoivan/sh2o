/* MVMVA lists: n of one GTE command in one call.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_LIST_H
#define SH2O_LIST_H

#include <stdint.h>

#include "sh2o/gte.h"
#ifdef __cplusplus
extern "C" {
#endif

/* New code. A list is the in-order fold of its n singles on the calling CPU: the image after item k, and what it
 * gives back for item k, equal what the singles leave after their k-th (tests/suite/sh2o_list_test_suite.c, every
 * item, all 64 registers). Only the calling CPU runs a list. */

/* Contract tag gte-mvmva-list-1.
 * sh2o_gte_mvmva_list(g, cmd, v, n, out): for k = 0 .. n-1 in order, the single MTC2 0 (VXY0) <- v[k][0], MTC2 1
 * (VZ0) <- v[k][1], MVMVA cmd (sh2o_gte_mvmva); then out[k] = IR1, IR2, IR3, MAC1, MAC2, MAC3 as MFC2 reads them
 * (IR sign-extended from 16 bits). v: V0 in its word pair form (the PS1 words of an SVECTOR); n = 0: nothing.
 * cmd as sh2o_gte_mvmva's (any mx, v, cv, sf, lm; with v != 0 the items still load V0, as the singles do). */
void sh2o_gte_mvmva_list(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);

#ifdef __cplusplus
}
#endif
#endif /* SH2O_LIST_H */
