/* Count points of a library built with SH2O_COUNT=ON (sh2o/count.h).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_COUNT_INTERNAL_H
#define SH2O_COUNT_INTERNAL_H

/* New code. Each macro is one count point; without SH2O_COUNT each is ((void)0) and the objects are the same bytes
 * as without this header. The storage is in src/sh2o.c. sh2oi_count_depth is raised while
 * sh2o calls its own named entries (SH2OI_COUNT_IN/OUT), so a name counter (SH2OI_COUNT_NAME) counts only a call
 * from outside sh2o; the op and write counters count every route.
 * SH2O_COUNT_PLANT=1 (a test build, CMakeLists.txt count_plant): the list items are not counted. */
#if SH2O_COUNT
#include <stdint.h>

#include "sh2o/count.h"

extern uint32_t sh2oi_count[SH2O_COUNT_N];
extern uint32_t sh2oi_count_depth;

#define SH2OI_COUNT_NAME(i) (sh2oi_count_depth ? (void)0 : (void)sh2oi_count[(i)]++)
#define SH2OI_COUNT_IN() ((void)sh2oi_count_depth++)
#define SH2OI_COUNT_OUT() ((void)sh2oi_count_depth--)
/* a per-op entry: its op, and its name from outside */
#define SH2OI_COUNT_CMD(i,op) ((void)sh2oi_count[SH2O_COUNT_OP + (op)]++,SH2OI_COUNT_NAME(i))
/* sh2o_gte_command: its name from outside, and the op of a command it has no entry for (f NULL) */
#define SH2OI_COUNT_COMMAND(f,cmd) \
    (SH2OI_COUNT_NAME(SH2O_COUNT_GTE_COMMAND),(f) ? (void)0 : (void)sh2oi_count[SH2O_COUNT_OP + ((cmd) & 0x3Fu)]++)
#if SH2O_COUNT_PLANT
#define SH2OI_COUNT_LIST(n) SH2OI_COUNT_NAME(SH2O_COUNT_LIST_CALLS)
#else
#define SH2OI_COUNT_LIST(n) (SH2OI_COUNT_NAME(SH2O_COUNT_LIST_CALLS),(void)(sh2oi_count[SH2O_COUNT_LIST_ITEMS] += (n)))
#endif

/* The write group of control register reg (sh2o/count.h). */
static inline void sh2oi_count_write(unsigned reg)
{
    unsigned i = SH2O_COUNT_WRITE_OTHER;
    if(reg >= 32 && reg <= 36)
        i = SH2O_COUNT_WRITE_RT;
    else if(reg >= 37 && reg <= 39)
        i = SH2O_COUNT_WRITE_TR;
    else if(reg >= 40 && reg <= 44)
        i = SH2O_COUNT_WRITE_LLM;
    else if(reg >= 45 && reg <= 47)
        i = SH2O_COUNT_WRITE_BK;
    else if(reg >= 48 && reg <= 52)
        i = SH2O_COUNT_WRITE_LCM;
    else if(reg >= 53 && reg <= 55)
        i = SH2O_COUNT_WRITE_FC;
    sh2oi_count[i]++;
}
#define SH2OI_COUNT_WRITE(reg) sh2oi_count_write(reg)
/* n registers of group i stored in place by a psyq body (not through sh2o_gte_write) */
#define SH2OI_COUNT_STORE(i,n) ((void)(sh2oi_count[(i)] += (n)))
#else
#define SH2OI_COUNT_NAME(i) ((void)0)
#define SH2OI_COUNT_IN() ((void)0)
#define SH2OI_COUNT_OUT() ((void)0)
#define SH2OI_COUNT_CMD(i,op) ((void)0)
#define SH2OI_COUNT_COMMAND(f,cmd) ((void)0)
#define SH2OI_COUNT_LIST(n) ((void)0)
#define SH2OI_COUNT_WRITE(reg) ((void)0)
#define SH2OI_COUNT_STORE(i,n) ((void)0)
#endif

#endif /* SH2O_COUNT_INTERNAL_H */
