/* Bench verdict helper: an op table, a rounds loop and three verdicts.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_BENCH_H
#define SH2O_BENCH_H

#include <stdint.h>

/* The caller's clock: a free-running count (on the SH-2, the FRT). */
typedef uint32_t (*sh2o_bench_tick)(void);
typedef void (*sh2o_bench_fn)(uint32_t k);

typedef struct sh2o_bench_op
{
    const char *name;
    sh2o_bench_fn ref;  /* the C reference over K inputs */
    sh2o_bench_fn cand; /* the candidate over the same K inputs */
    uint32_t K;
} sh2o_bench_op;

typedef enum
{
    SH2O_BENCH_BETTER,
    SH2O_BENCH_NOISE,
    SH2O_BENCH_WORSE
} sh2o_bench_verdict;

/* BETTER when cand + band < ref, WORSE when cand > ref + band, else NOISE; band = ref x noise_pct / 100. */
sh2o_bench_verdict sh2o_bench_classify(uint32_t ref,uint32_t cand,uint32_t noise_pct);

/* rounds alternating ref and cand; the lowest tick count of each in *ref_min, *cand_min */
void sh2o_bench_run(const sh2o_bench_op *op,sh2o_bench_tick tick,uint32_t rounds,uint32_t *ref_min,
                    uint32_t *cand_min);

#endif
