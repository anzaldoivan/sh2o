/* Library-wide checks and the backend name.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/sh2o.h"
#include "sh2o_internal.h"
#include "sh2o_count_internal.h"

/* Every exact contract assumes two's-complement integers. C11 also permits
 * other representations, so the build stops on them. */
_Static_assert((-1&3)==3,"two's-complement integers required");

const char *sh2o_backend(void) {
    return SH2OI_NATIVE?"sh2":"portable";
}

#if SH2O_COUNT
/* The counters of sh2o/count.h (SH2O_COUNT=ON only); the count points are in src/sh2o_count_internal.h. */
uint32_t sh2oi_count[SH2O_COUNT_N];
uint32_t sh2oi_count_depth;

#define SH2OI_OP16(d) "op_" d "0","op_" d "1","op_" d "2","op_" d "3","op_" d "4","op_" d "5","op_" d "6",\
    "op_" d "7","op_" d "8","op_" d "9","op_" d "a","op_" d "b","op_" d "c","op_" d "d","op_" d "e","op_" d "f"
static const char *const sh2oi_count_names[SH2O_COUNT_N] = {
    SH2OI_OP16("0"),SH2OI_OP16("1"),SH2OI_OP16("2"),SH2OI_OP16("3"),
    "sh2o_gte_command","sh2o_gte_mvmva","sh2o_gte_nclip","sh2o_gte_avsz3","sh2o_gte_sqr","sh2o_gte_gpf",
    "sh2o_gte_gpl","sh2o_gte_dpcs","sh2o_gte_rtps","sh2o_gte_rtpt","sh2o_gte_ncs","sh2o_gte_nct","sh2o_gte_ncds",
    "sh2o_gte_ncdt","sh2o_gte_nccs","sh2o_gte_nrtps","sh2o_gte_nrtpt","sh2o_gte_dpct",
    "sh2o_gte_mvmva_prepared","sh2o_gte_prepared_load",
    "sh2o_psyq_SetRotMatrix","sh2o_psyq_SetLightMatrix","sh2o_psyq_SetColorMatrix","sh2o_psyq_SetTransMatrix",
    "sh2o_psyq_SetBackColor","sh2o_psyq_SetFarColor","sh2o_psyq_SetGeomOffset","sh2o_psyq_SetGeomScreen",
    "sh2o_psyq_InitGeom","sh2o_psyq_SetFogNearFar","sh2o_psyq_TransMatrix","sh2o_psyq_TransposeMatrix",
    "sh2o_psyq_CompMatrix","sh2o_psyq_ApplyMatrix","sh2o_psyq_ApplyMatrixLV","sh2o_psyq_RotTrans",
    "sh2o_psyq_PushMatrix","sh2o_psyq_PopMatrix","sh2o_psyq_ratan2",
    "sh2o_gte_mvmva_list","list_items","sh2o_gte_mvmva_list_prepared",
    "write_rt","write_llm","write_lcm","write_tr","write_bk","write_fc","write_other",
};

uint32_t sh2o_count_get(unsigned i)
{
    return i < SH2O_COUNT_N ? sh2oi_count[i] : 0u;
}

const char *sh2o_count_name(unsigned i)
{
    return i < SH2O_COUNT_N ? sh2oi_count_names[i] : (const char *)0;
}

/* Through a volatile pointer: GCC keeps the loop (no memset call; the SH-2 library needs no C library). */
void sh2o_count_reset(void)
{
    volatile uint32_t *c = sh2oi_count;
    unsigned i;
    for(i = 0; i < SH2O_COUNT_N; i++)
        c[i] = 0u;
}
#endif
