/* SH2ZAM delegate for the MAC stage of the GTE core (experimental).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_ZAM_EXPERIMENTAL_H
#define SH2O_ZAM_EXPERIMENTAL_H

#include <stdint.h>

#include "sh2o/psyq.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Not in the umbrella sh2o.h and not installed. Names sh2o types only; the one file that links SH2ZAM is
 * src/sh2o_zam.c. The GTE core (src/sh2o_gte.c) calls sh2o_zam_mac3 only when built with SH2O_ZAM=1 (the CMake
 * option SH2O_ZAM, OFF by default); the derivation is in src/sh2o_zam.c. */

/* Contract tag zam-mac3-1.
 * sh2o_zam_mac3(m, v, t, mac)
 *   Domain: every |m[i][j]|, |v[j]| <= 0x8000; per row i, 16*sum_j |m[i][j]|*|v[j]| + |t[i]|*65536 <= 2^47-1; and the
 *           delegate's plan reports its fast path. Checked here, before SH2ZAM runs.
 *   Exact:  inside it, returns 1 and mac[i] = (t[i]*4096 + m[i].v) SAR 12 (no 44-bit MAC partial overflows: the GTE
 *           core's MAC flags are 0). Outside it, returns 0 and mac is untouched.
 *   Caller: the GTE core only with sf = 1, MVMVA mx < 3 and cv != 2, and RTPS/RTPT. */
int sh2o_zam_mac3(const int32_t m[3][3],const int32_t v[3],const int32_t t[3],int32_t mac[3]);

/* zam-ratan2-1, zam-transposematrix-1, zam-setfognearfar-1: candidates for sh2o_psyq bodies, measured against the
 * body in the delegate's suite; no body calls them (the SW backend keeps sh2o's own bodies), so none runs outside a
 * build with SH2O_ZAM=ON. */

/* Contract tag zam-ratan2-1.
 * sh2o_zam_ratan2(y, x): sh2z_atan2(y, x) (65536 per turn) remapped to 4096 per turn, ((a + 8) >> 4) & 4095,
 *   then signed as ratan2 (t > 800h, or t = 800h with y < 0: t - 1000h). Domain: none proven; status refused. */
int32_t sh2o_zam_ratan2(int32_t y,int32_t x);
/* Contract tag zam-transposematrix-1.
 * sh2o_zam_TransposeMatrix(m0, m1): m1->m = m0->m transposed through sh2z_mat33_transpose (m read whole first,
 *   so m1 may be m0); pad and t of m1 untouched; returns m1. Exact on every input (a permutation, no arithmetic). */
sh2o_psyq_matrix_t *sh2o_zam_TransposeMatrix(const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1);
/* Contract tag zam-setfognearfar-1.
 * sh2o_zam_SetFogNearFar(g, a, b, h): with d = b - a (32-bit wrap), inside the gate d >= 100, h != 0, and
 *   -a*b, b*4096, q1*256 each in int32 (the object's 32-bit products do not wrap) and not (h = -1 and q1*256 =
 *   INT32_MIN): writes DQA, DQB as sh2o_psyq_SetFogNearFar through sh2z_muldiv (exact 64-bit product, quotient
 *   truncated) and returns 1; outside it returns 0 and writes nothing (the caller runs its own body). */
int sh2o_zam_SetFogNearFar(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h);

/* Contract tag zam-mvmva-list-1.
 * sh2o_zam_mvmva_list(g, cmd, v, n, out): sh2o_gte_mvmva_list's contract (sh2o/list.h) through
 *   one prepared plan (sh2z_transform_plan_execute_mac over the n points).
 *   Domain: MVMVA with sf 1, mx < 3, vx 0 (V0), cv != 2; n <= SH2O_ZAM_LIST_MAX; zam-mac3-1's row gate on m, t and
 *           the per-axis largest |v| over the n items; the plan reports its fast path. Checked before SH2ZAM runs.
 *   Exact:  inside it, returns 1 with g and out as the list leaves them (each item zam-mac3-1's MAC, the GTE core's
 *           IR clamp and FLAG 24-22; the image: V0 the last item's words, MAC1-3, IR1-3, FLAG the last item's);
 *           n = 0 returns 1 and writes nothing. Outside it (or bypassed) returns 0 and writes nothing (the caller
 *           runs the list).
 *   Proof:  vectors (tests/suite/sh2o_zam_test_suite.c sh2o_zam_list_trial: 0 differences vs the list on 64
 *           registers and out, a planted mismatch caught). */
#define SH2O_ZAM_LIST_MAX 32u /* zam-mvmva-list-1's largest n (one plan's points on the stack) */
int sh2o_zam_mvmva_list(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);

/* test-only knobs (tests/suite/sh2o_zam_test_suite.c): bypass != 0 makes sh2o_zam_mac3 return 0 at once (the GTE
 * core's own kernel runs); delegated / refused count its non-bypassed calls by result */
extern int sh2o_zam_bypass;
extern uint32_t sh2o_zam_delegated,sh2o_zam_refused;

#ifdef __cplusplus
}
#endif
#endif /* SH2O_ZAM_EXPERIMENTAL_H */
