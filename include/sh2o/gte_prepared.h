/* MVMVA with a prepared shadow of the matrices and translations.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_GTE_PREPARED_H
#define SH2O_GTE_PREPARED_H

#include <stdint.h>

#include "sh2o/cdefs.h"
#include "sh2o/gte.h"
#include "sh2o/inline/sw/gte_prepared_sw.inl.h"
#include "sh2o/inline/sh2/gte_prepared_sh2.inl.h"
#ifdef __cplusplus
extern "C" {
#endif

/* New code. sh2o/sh2o.h does not include this header: no default path uses it. */

/* The shadow of an image's matrices and translations, made by sh2o_gte_prepared_load.
 * c[mx]: the rows of matrix mx (0 rotation r[32..36], 1 light r[40..44], 2 colour r[48..52]), each element
 * R SHL 4, row-major. pre[ts]: per row, MACH then MACL of t SHL 16 sign-extended to 64 bits (ts 0 translation
 * r[37..39], 1 back colour r[45..47], 2 none: zeros). ok[ts]: 1 when every |t| < 7FF40000h (ts 2: always 1). */
typedef struct sh2o_gte_prepared {
    int32_t c[3][9];
    uint32_t pre[3][6];
    uint8_t ok[3];
} sh2o_gte_prepared_t;

/* The parts of a shadow, for sh2o_gte_prepared_load. Part bit: the rotation matrix. */
#define SH2O_GTE_PREPARED_RT 1u
/* Part bit: the light matrix. */
#define SH2O_GTE_PREPARED_LIGHT 2u
/* Part bit: the light colour matrix. */
#define SH2O_GTE_PREPARED_COLOR 4u
/* Part bit: the translation vector. */
#define SH2O_GTE_PREPARED_TR 8u
/* Part bit: the back colour. */
#define SH2O_GTE_PREPARED_BK 16u
/* Part bit: the zero translation (ts 2). */
#define SH2O_GTE_PREPARED_NONE 32u
/* All the part bits: the first load of a shadow. */
#define SH2O_GTE_PREPARED_ALL 63u

/* Contract tag gte-mvmva-prepared-1.
 * sh2o_gte_prepared_load(p, g, parts): writes the parts of p that parts names (the bits above) from the image g,
 * and no other part. The first load of a shadow is SH2O_GTE_PREPARED_ALL. After each write to the registers of a
 * part, the caller loads that part again. A stale shadow gives a wrong result: no function checks it. */
void sh2o_gte_prepared_load(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);

/* Contract tag gte-mvmva-prepared-1.
 * sh2o_gte_mvmva_prepared(g, p, cmd): MVMVA (12h) with the shadow p of g. It calls sh2o_gte_mvmva(g, cmd) and
 * gives 0 when sf = 0, mx = 3, cv = 2, ok[ts] is 0 (ts = cv, cv 3 uses ts 2), or (SH-2 path only) SR.S = 1 at the
 * call (one STC SR; SR is not written). Else it runs the prepared kernel and gives 1: MAC1-3, IR1-3 (lm) and FLAG
 * bits 24-22. When p is fresh, the 64 words of g equal those of sh2o_gte_mvmva for every image and cmd. */
int sh2o_gte_mvmva_prepared(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);

/* Contract tag gte-mvmva-list-prepared-1.
 * sh2o_gte_mvmva_list_prepared(g, cmd, v, n, out): the same contract as sh2o_gte_mvmva_list (sh2o/list.h): for
 * k = 0 .. n-1 in order, the single MTC2 0 (VXY0) <- v[k][0], MTC2 1 (VZ0) <- v[k][1], MVMVA cmd; then out[k] = IR1,
 * IR2, IR3, MAC1, MAC2, MAC3 as MFC2 reads them (IR sign-extended from 16 bits); n = 0: nothing. The out rows and
 * the 64 words of g equal those of sh2o_gte_mvmva_list for every image, cmd and items. It loads a shadow of its own
 * (no caller shadow) once per call and runs the prepared kernel on each item when sf = 1, v = 0, mx < 3, cv != 2,
 * ok[ts] is 1 and (SH-2 path only) SR.S = 0 at the call (one STC SR; SR is not written); else it calls
 * sh2o_gte_mvmva_list(g, cmd, v, n, out). */
void sh2o_gte_mvmva_list_prepared(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);

#ifdef __cplusplus
}
#endif
#endif
