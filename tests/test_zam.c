/* Host run of the delegate suite and its trials, with every pinned figure.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Built only with SH2O_ZAM=1 (the CMake option SH2O_ZAM). The run: the full suite (tests/suite/sh2o_zam_test_suite.c),
 * the zam-2..4 trials, then the zam-5 trial; rc is the first failure in that order (the codes of sh2o_zam_test.h).
 * It prints five lines, then checks: with SH2O_MUTANT 0 (the delegate as built), rc 0 and every pinned figure; with
 * SH2O_MUTANT k = 1..5 (CMakeLists.txt builds the mutant programs zam_mutant_*), rc equal to the pin of mutant k.
 * Exit 0 = the check passes. */
#include <stdio.h>

#include "sh2o/zam_experimental.h"
#include "sh2o_zam_test.h"

#ifndef SH2O_MUTANT
#define SH2O_MUTANT 0
#endif

/* rc of each mutant of src/sh2o_zam.c: 1 rows shifted by 3, 2 gate 2^49-1, 3 TR dropped, 4 transpose twice,
 * 5 fog no-wrap gate dropped */
static const int mutant_rc[6]={0x0,0x1001,0x1101,0x1001,0x5001,0x6001};

_Static_assert(SH2O_MUTANT >= 0 && SH2O_MUTANT <= 5,"SH2O_MUTANT is 0..5");

static int failures;

static void pin(const char *name,unsigned long got,unsigned long want) {
    if(got != want) {
        fprintf(stderr,"zam: FAIL: %s is %lu, not %lu\n",name,got,want);
        failures++;
    }
}

static uint32_t zero_clock(void) {
    return 0;
}

int main(void) {
    sh2o_zam_tally t;
    sh2o_zam_trial ra[3]={{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0}},tr={0,0,0,0,0},fog={0,0,0,0,0};
    sh2o_zam_ltrial lt={0,0,0,0,0,0,0};
    uint32_t d3,r3;
    int rc = sh2o_zam_test_suite(&t,SH2O_VEC_COUNT,SH2O_ZAM_SEEDED);
    sh2o_zam_ratan2_trial(ra);
    sh2o_zam_transpose_trial(&tr);
    sh2o_zam_fog_trial(&fog);
    if(!rc && tr.mismatches)
        rc = t.rc = 0x5001;
    if(!rc && fog.mismatches)
        rc = t.rc = 0x6001;
    if(!rc && (fog.delegated == 0 || fog.refused == 0))
        rc = t.rc = 0x6002;
    d3 = sh2o_zam_delegated;
    r3 = sh2o_zam_refused;
    sh2o_zam_list_trial(&lt);
    if(!rc && lt.mismatches)
        rc = t.rc = 0x7001;
    if(!rc && (lt.delegated == 0 || lt.refused == 0))
        rc = t.rc = 0x7002;
    if(!rc && lt.caught == 0)
        rc = t.rc = 0x7003;
    printf("sh2o_zam_mvmva_list: %lu runs, %lu items, %lu delegated, %lu refused, %lu mismatches, planted caught %lu "
        "of %lu\n",
        lt.runs,lt.items,lt.delegated,lt.refused,lt.mismatches,lt.caught,lt.planted);
    printf("sh2o_zam_ratan2: grid %lu cases %lu mismatches max %ld, xorshift %lu cases %lu mismatches max %ld, "
        "edges %lu cases %lu mismatches max %ld (4096/turn)\n",
        ra[0].cases,ra[0].mismatches,ra[0].maxerr,ra[1].cases,ra[1].mismatches,ra[1].maxerr,ra[2].cases,
        ra[2].mismatches,ra[2].maxerr);
    printf("sh2o_zam_TransposeMatrix: %lu cases, %lu delegated, %lu mismatches\n",tr.cases,tr.delegated,
        tr.mismatches);
    printf("sh2o_zam_SetFogNearFar: %lu cases, %lu delegated, %lu refused, %lu mismatches\n",fog.cases,fog.delegated,
        fog.refused,fog.mismatches);
    printf("sh2o_zam_test: %lu cases, %lu delegated, %lu refused (mac3 calls: %lu delegated, %lu refused), "
        "digest 0x%08lX, rc 0x%X\n",
        (unsigned long)t.cases,(unsigned long)t.delegated,(unsigned long)t.refused,(unsigned long)d3,
        (unsigned long)r3,(unsigned long)t.h,(unsigned)rc);
    if(SH2O_MUTANT != 0) {
        if(rc != mutant_rc[SH2O_MUTANT]) {
            fprintf(stderr,"zam: FAIL: mutant %d gives rc 0x%X, not 0x%X\n",SH2O_MUTANT,(unsigned)rc,
                (unsigned)mutant_rc[SH2O_MUTANT]);
            return 1;
        }
        printf("zam: mutant %d caught, rc 0x%X\n",SH2O_MUTANT,(unsigned)rc);
        return 0;
    }
    /* zam-1: the suite (sets A to C) */
    pin("rc",(unsigned long)rc,0);
    pin("suite cases",t.cases,SH2O_ZAM_PIN_CASES);
    pin("suite delegated",t.delegated,SH2O_ZAM_PIN_DELEGATED);
    pin("suite refused",t.refused,SH2O_ZAM_PIN_REFUSED);
    pin("mac3 delegated",d3,SH2O_ZAM_PIN_MAC3_DELEGATED);
    pin("mac3 refused",r3,SH2O_ZAM_PIN_MAC3_REFUSED);
    pin("suite digest",t.h,SH2O_ZAM_PIN_DIGEST);
    /* zam-2: a measurement; refused, so the mismatches must not be 0 */
    pin("ratan2 grid cases",ra[0].cases,16785409);
    pin("ratan2 grid mismatches",ra[0].mismatches,2342014);
    pin("ratan2 grid max",(unsigned long)ra[0].maxerr,1);
    pin("ratan2 xorshift cases",ra[1].cases,1048576);
    pin("ratan2 xorshift mismatches",ra[1].mismatches,146687);
    pin("ratan2 xorshift max",(unsigned long)ra[1].maxerr,1);
    pin("ratan2 edge cases",ra[2].cases,169);
    pin("ratan2 edge mismatches",ra[2].mismatches,37);
    pin("ratan2 edge max",(unsigned long)ra[2].maxerr,2048);
    if(ra[0].mismatches + ra[1].mismatches + ra[2].mismatches == 0) {
        fprintf(stderr,"zam: FAIL: ratan2 has no mismatch, but its status is refused\n");
        failures++;
    }
    /* zam-3 and zam-4: proven */
    pin("TransposeMatrix cases",tr.cases,SH2O_ZAM_PIN_TR_CASES);
    pin("TransposeMatrix delegated",tr.delegated,SH2O_ZAM_PIN_TR_DELEGATED);
    pin("TransposeMatrix mismatches",tr.mismatches,0);
    pin("SetFogNearFar cases",fog.cases,SH2O_ZAM_PIN_FOG_CASES);
    pin("SetFogNearFar delegated",fog.delegated,SH2O_ZAM_PIN_FOG_DELEGATED);
    pin("SetFogNearFar refused",fog.refused,SH2O_ZAM_PIN_FOG_REFUSED);
    pin("SetFogNearFar mismatches",fog.mismatches,0);
    /* zam-5 */
    pin("list runs",lt.runs,SH2O_ZAM_PIN_LIST_RUNS);
    pin("list items",lt.items,SH2O_ZAM_PIN_LIST_ITEMS);
    pin("list delegated",lt.delegated,SH2O_ZAM_PIN_LIST_DELEGATED);
    pin("list refused",lt.refused,SH2O_ZAM_PIN_LIST_REFUSED);
    pin("list mismatches",lt.mismatches,0);
    pin("list planted",lt.planted,SH2O_ZAM_PIN_LIST_PLANTED);
    pin("list caught",lt.caught,SH2O_ZAM_PIN_LIST_CAUGHT);
    /* the delegate's bench entry (its fixed subset of the suite) */
    {
        uint32_t w[SH2O_ZAM_W_SIZE];
        unsigned long erc = (unsigned long)sh2o_zam_test_entry(w,zero_clock);
        printf("sh2o_zam_test_entry: returned %lu, rc 0x%lX, %lu cases, %lu delegated, %lu refused, digest 0x%08lX\n",
            erc,(unsigned long)w[SH2O_ZAM_W_RC],(unsigned long)w[SH2O_ZAM_W_CASES],
            (unsigned long)w[SH2O_ZAM_W_DELEG],(unsigned long)w[SH2O_ZAM_W_REFUSED],
            (unsigned long)w[SH2O_ZAM_W_DIGEST]);
        pin("entry rc",(unsigned long)w[SH2O_ZAM_W_RC],0);
        pin("entry cases",(unsigned long)w[SH2O_ZAM_W_CASES],2348);
        pin("entry delegated",(unsigned long)w[SH2O_ZAM_W_DELEG],382);
        pin("entry refused",(unsigned long)w[SH2O_ZAM_W_REFUSED],1966);
        pin("entry digest",(unsigned long)w[SH2O_ZAM_W_DIGEST],0xDFAF9742ul);
    }
    if(failures != 0)
        return 1;
    printf("zam: every pinned figure equal\n");
    return 0;
}
