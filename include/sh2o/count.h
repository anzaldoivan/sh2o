/* Call counters of a library built with SH2O_COUNT=ON.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_COUNT_H
#define SH2O_COUNT_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* New code. The functions below are defined only in a library built with SH2O_COUNT=ON (CMake option, make
 * SH2O_COUNT=ON); a call against a library built without it fails at link. The counters are plain uint32_t, one
 * copy for the whole program, not atomic: count from one CPU, and they wrap at 2^32.
 * What adds 1:
 * - SH2O_COUNT_OP + op (op 0..63): every GTE command run, by any route (a per-op entry called directly,
 *   sh2o_gte_command, a list item, a psyq body); sh2o_gte_command also counts the op (cmd & 0x3F) of a command it
 *   returns -1 for. A per-op entry counts its own op.
 * - SH2O_COUNT_GTE_*, SH2O_COUNT_PSYQ_*, SH2O_COUNT_LIST_CALLS, SH2O_COUNT_LIST_PREPARED_CALLS: a call of that
 *   function from outside sh2o only (sh2o's own calls of its entries, e.g. sh2o_gte_command inside a psyq body,
 *   are not counted there).
 * - SH2O_COUNT_LIST_ITEMS: n per sh2o_gte_mvmva_list call and per sh2o_gte_mvmva_list_prepared call.
 * - SH2O_COUNT_WRITE_*: every sh2o_gte_write and every register a psyq body stores in place, by any route, by
 *   register: rotation 32-36, light 40-44, colour 48-52, translation 37-39, back colour 45-47, far colour
 *   53-55, other (any other register). */
enum {
    SH2O_COUNT_OP = 0, /* 64 slots, one per op */
    SH2O_COUNT_GTE_COMMAND = SH2O_COUNT_OP + 64,
    SH2O_COUNT_GTE_MVMVA,
    SH2O_COUNT_GTE_NCLIP,
    SH2O_COUNT_GTE_AVSZ3,
    SH2O_COUNT_GTE_SQR,
    SH2O_COUNT_GTE_GPF,
    SH2O_COUNT_GTE_GPL,
    SH2O_COUNT_GTE_DPCS,
    SH2O_COUNT_GTE_RTPS,
    SH2O_COUNT_GTE_RTPT,
    SH2O_COUNT_GTE_NCS,
    SH2O_COUNT_GTE_NCT,
    SH2O_COUNT_GTE_NCDS,
    SH2O_COUNT_GTE_NCDT,
    SH2O_COUNT_GTE_NCCS,
    SH2O_COUNT_GTE_NRTPS,
    SH2O_COUNT_GTE_NRTPT,
    SH2O_COUNT_GTE_DPCT,
    SH2O_COUNT_GTE_MVMVA_PREPARED,
    SH2O_COUNT_GTE_PREPARED_LOAD,
    SH2O_COUNT_PSYQ_SETROTMATRIX,
    SH2O_COUNT_PSYQ_SETLIGHTMATRIX,
    SH2O_COUNT_PSYQ_SETCOLORMATRIX,
    SH2O_COUNT_PSYQ_SETTRANSMATRIX,
    SH2O_COUNT_PSYQ_SETBACKCOLOR,
    SH2O_COUNT_PSYQ_SETFARCOLOR,
    SH2O_COUNT_PSYQ_SETGEOMOFFSET,
    SH2O_COUNT_PSYQ_SETGEOMSCREEN,
    SH2O_COUNT_PSYQ_INITGEOM,
    SH2O_COUNT_PSYQ_SETFOGNEARFAR,
    SH2O_COUNT_PSYQ_TRANSMATRIX,
    SH2O_COUNT_PSYQ_TRANSPOSEMATRIX,
    SH2O_COUNT_PSYQ_COMPMATRIX,
    SH2O_COUNT_PSYQ_APPLYMATRIX,
    SH2O_COUNT_PSYQ_APPLYMATRIXLV,
    SH2O_COUNT_PSYQ_ROTTRANS,
    SH2O_COUNT_PSYQ_PUSHMATRIX,
    SH2O_COUNT_PSYQ_POPMATRIX,
    SH2O_COUNT_PSYQ_RATAN2,
    SH2O_COUNT_LIST_CALLS,
    SH2O_COUNT_LIST_ITEMS,
    SH2O_COUNT_LIST_PREPARED_CALLS,
    SH2O_COUNT_WRITE_RT,
    SH2O_COUNT_WRITE_LLM,
    SH2O_COUNT_WRITE_LCM,
    SH2O_COUNT_WRITE_TR,
    SH2O_COUNT_WRITE_BK,
    SH2O_COUNT_WRITE_FC,
    SH2O_COUNT_WRITE_OTHER,
    SH2O_COUNT_N
};

/* The counter i; 0 when i >= SH2O_COUNT_N. */
uint32_t sh2o_count_get(unsigned i);
/* The name of counter i: "op_xx" (op in 2 lowercase hex digits), the function name (list calls:
 * "sh2o_gte_mvmva_list"), "list_items", "write_rt", "write_llm", "write_lcm", "write_tr", "write_bk", "write_fc",
 * "write_other"; NULL when i >= SH2O_COUNT_N. */
const char *sh2o_count_name(unsigned i);
/* Sets every counter to 0. */
void sh2o_count_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* SH2O_COUNT_H */
