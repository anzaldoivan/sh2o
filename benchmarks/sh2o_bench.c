/* Bench verdict helper: an op table, a rounds loop over a caller-supplied tick function, and classify().
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* No timer code: the program supplies tick. The method is SH4ZAM's in outline (alternate ref and candidate over
 * rounds, keep the lowest, three verdicts); no SH4ZAM text. A candidate is only taken as BETTER beyond the noise
 * band written before the run. */
#include "sh2o_bench.h"

/* noise_pct: the band, written before the run */
sh2o_bench_verdict sh2o_bench_classify(uint32_t ref,uint32_t cand,uint32_t noise_pct);
sh2o_bench_verdict sh2o_bench_classify(uint32_t ref,uint32_t cand,uint32_t noise_pct)
{
    uint64_t band = (uint64_t)ref * noise_pct / 100u;
    if((uint64_t)cand + band < ref)
        return SH2O_BENCH_BETTER;
    if((uint64_t)cand > ref + band)
        return SH2O_BENCH_WORSE;
    return SH2O_BENCH_NOISE;
}

/* rounds alternating ref and cand; the lowest tick count of each in *ref_min, *cand_min */
void sh2o_bench_run(const sh2o_bench_op *op,sh2o_bench_tick tick,uint32_t rounds,uint32_t *ref_min,
                    uint32_t *cand_min);
void sh2o_bench_run(const sh2o_bench_op *op,sh2o_bench_tick tick,uint32_t rounds,uint32_t *ref_min,
                    uint32_t *cand_min)
{
    uint32_t r,t0,d;
    *ref_min = *cand_min = UINT32_MAX;
    for(r = 0; r < rounds; r++)
    {
        t0 = tick();
        op->ref(op->K);
        d = tick() - t0;
        *ref_min = d < *ref_min ? d : *ref_min;
        t0 = tick();
        op->cand(op->K);
        d = tick() - t0;
        *cand_min = d < *cand_min ? d : *cand_min;
    }
}
