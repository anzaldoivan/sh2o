/* Test of the bench verdict helper: the band edges of classify and the minima of run.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o_bench.h"
#include <stdio.h>

/* A counting clock: tick reads it, each op adds its cost for this call from a fixed sequence. So each delta that
 * run measures is exactly one cost, and the minima are known. */
static uint32_t now;
static unsigned failures;

static void check(int ok,const char *what) {
    if(!ok) {
        printf("bench: FAIL %s\n",what);
        failures++;
    }
}

static uint32_t tick(void) {
    return now;
}

#define BENCH_ROUNDS 4u
static const uint32_t cost_a_ref[BENCH_ROUNDS]={9,4,7,5};
static const uint32_t cost_a_cand[BENCH_ROUNDS]={3,8,2,6};
static const uint32_t cost_b_ref[BENCH_ROUNDS]={120,130,110,140};
static const uint32_t cost_b_cand[BENCH_ROUNDS]={300,200,250,260};
static unsigned call_a_ref,call_a_cand,call_b_ref,call_b_cand;
static uint32_t last_k;

static void a_ref(uint32_t k) {
    last_k=k;
    now+=cost_a_ref[call_a_ref++];
}

static void a_cand(uint32_t k) {
    last_k=k;
    now+=cost_a_cand[call_a_cand++];
}

static void b_ref(uint32_t k) {
    last_k=k;
    now+=cost_b_ref[call_b_ref++];
}

static void b_cand(uint32_t k) {
    last_k=k;
    now+=cost_b_cand[call_b_cand++];
}

static const sh2o_bench_op ops[2]={
    {"a",a_ref,a_cand,16u},
    {"b",b_ref,b_cand,3u},
};

static void test_classify(void) {
    /* band = 1000 x 10 / 100 = 100 */
    check(sh2o_bench_classify(1000u,899u,10u)==SH2O_BENCH_BETTER,"ref-band-1 is BETTER");
    check(sh2o_bench_classify(1000u,900u,10u)==SH2O_BENCH_NOISE,"ref-band is NOISE");
    check(sh2o_bench_classify(1000u,1100u,10u)==SH2O_BENCH_NOISE,"ref+band is NOISE");
    check(sh2o_bench_classify(1000u,1101u,10u)==SH2O_BENCH_WORSE,"ref+band+1 is WORSE");
    /* noise_pct 0: the band is 0 */
    check(sh2o_bench_classify(1000u,999u,0u)==SH2O_BENCH_BETTER,"pct 0: ref-1 is BETTER");
    check(sh2o_bench_classify(1000u,1000u,0u)==SH2O_BENCH_NOISE,"pct 0: ref is NOISE");
    check(sh2o_bench_classify(1000u,1001u,0u)==SH2O_BENCH_WORSE,"pct 0: ref+1 is WORSE");
    /* the band rounds down: 7 x 10 / 100 = 0 */
    check(sh2o_bench_classify(7u,6u,10u)==SH2O_BENCH_BETTER,"band 0 after rounding: BETTER");
    /* no overflow at the top of the range */
    check(sh2o_bench_classify(UINT32_MAX,UINT32_MAX,100u)==SH2O_BENCH_NOISE,"UINT32_MAX is NOISE");
}

static void reset(void) {
    now=0xFFFFFFF0u; /* the deltas cross the wrap of the clock */
    call_a_ref=call_a_cand=call_b_ref=call_b_cand=0;
}

static void test_run(void) {
    uint32_t ref_min,cand_min;
    reset();
    sh2o_bench_run(&ops[0],tick,1u,&ref_min,&cand_min);
    check(ref_min==9u&&cand_min==3u,"a, 1 round: the first costs");
    check(last_k==16u,"a: K given to the ops");
    sh2o_bench_run(&ops[1],tick,1u,&ref_min,&cand_min);
    check(ref_min==120u&&cand_min==300u,"b, 1 round: the first costs");
    check(last_k==3u,"b: K given to the ops");
    reset();
    sh2o_bench_run(&ops[0],tick,BENCH_ROUNDS,&ref_min,&cand_min);
    check(ref_min==4u&&cand_min==2u,"a, 4 rounds: the minima");
    check(call_a_ref==BENCH_ROUNDS&&call_a_cand==BENCH_ROUNDS,"a: one call each per round");
    sh2o_bench_run(&ops[1],tick,BENCH_ROUNDS,&ref_min,&cand_min);
    check(ref_min==110u&&cand_min==200u,"b, 4 rounds: the minima");
    check(call_b_ref==BENCH_ROUNDS&&call_b_cand==BENCH_ROUNDS,"b: one call each per round");
    reset();
    sh2o_bench_run(&ops[0],tick,0u,&ref_min,&cand_min);
    check(ref_min==UINT32_MAX&&cand_min==UINT32_MAX&&call_a_ref==0u,"0 rounds: no call, UINT32_MAX");
}

int main(void) {
    test_classify();
    test_run();
    printf("bench: %s\n",failures?"FAIL":"ok");
    return failures?1:0;
}
