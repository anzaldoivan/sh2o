/* The list suite: each MVMVA list against its n singles.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* sh2o_list_test_suite() prints nothing. tests/test_list.c calls it on the host; a program on the SH-2 can call it
 * too. Constant shifts only, no divide or modulo.
 * Case set, the gte suite's: LCG x = x * 1664525 + 1013904223 from SH2O_GTE_SEED; one run per MVMVA selector
 * s = 0..255 (the gte suite's encoding: lm = s bit 0, sf 1, cv 2-3, v 4-5, mx 6-7): a draw d, n = 1 + (d >> 28)
 * (1..16 items); 64 draws, the words of registers 0..63 written through sh2o_gte_write in the gte suite's order on a
 * zeroed image; then n items of two draws each (VXY0, VZ0). Run s odd: every word AND 0x0FFF0FFF (tame).
 * Check: the singles (sh2o_gte_write 0, 1; sh2o_gte_command; sh2o_gte_read 9-11, 25-27) from the run's image, the
 * 64 read words kept after each item; for every prefix j = 1..n, sh2o_gte_mvmva_list on the same image with the
 * first j items: its 64 read words equal the singles' after item j, and its out rows the singles' reads.
 * Planted reorder: the same check with items 0 and 1 swapped in the list's input must fail at some prefix, in every
 * run with n >= 2 and two different items. */
#include <stdint.h>

#include "sh2o_list_test.h"

/* the gte suite's side-effect-free write order: no 15, 29, 31 */
static const uint8_t sh2o_list_order[61]={0,1,2,3,4,5,6,7,8,28,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,
    27,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};

static uint32_t sh2o_list_lcg(uint32_t *x) {
    *x = *x * 1664525u + 1013904223u;
    return *x;
}

/* 1 when the list on (base, cmd, v, j items) leaves the singles' state after item j (regs) and their reads (want) */
static int sh2o_list_same(const sh2o_gte_t *base,uint32_t cmd,const uint32_t (*v)[2],unsigned j,
    const uint32_t *regs,const uint32_t (*want)[6]) {
    static uint32_t out[SH2O_LIST_MAX][6];
    sh2o_gte_t g = *base;
    unsigned k,c;
    sh2o_gte_mvmva_list(&g,cmd,v,j,out);
    for(k = 0; k < 64; k++)
        if(sh2o_gte_read(&g,k) != regs[k])
            return 0;
    for(k = 0; k < j; k++)
        for(c = 0; c < 6; c++)
            if(out[k][c] != want[k][c])
                return 0;
    return 1;
}

/* the case set; c[0] runs, c[1] items, c[2] prefixes that differ, c[3] reorders caught, c[4] reorders planted */
static void sh2o_list_run(uint32_t c[5]) {
    static const uint8_t mac[6]={9,10,11,25,26,27};
    static uint32_t regs[SH2O_LIST_MAX][64],want[SH2O_LIST_MAX][6],v[SH2O_LIST_MAX][2],sw[SH2O_LIST_MAX][2];
    uint32_t x = SH2O_GTE_SEED,s,w[64];
    unsigned n,j,k;
    sh2o_gte_t base,g;
    for(k = 0; k < 5; k++)
        c[k] = 0;
    for(s = 0; s < SH2O_LIST_RUNS; s++) {
        uint32_t mask = (s & 1u) ? 0x0FFF0FFFu : 0xFFFFFFFFu,caught = 0;
        uint32_t cmd = 0x12u | (s & 1u) << 10 | ((s >> 1) & 1u) << 19 | ((s >> 2) & 3u) << 13 | ((s >> 4) & 3u) << 15 |
            ((s >> 6) & 3u) << 17;
        n = 1u + (sh2o_list_lcg(&x) >> 28);
        for(k = 0; k < 64; k++)
            w[k] = sh2o_list_lcg(&x) & mask;
        base.r[15] = base.r[29] = base.r[31] = 0; /* the order writes every other register: a zeroed image */
        for(k = 0; k < sizeof sh2o_list_order; k++)
            sh2o_gte_write(&base,sh2o_list_order[k],w[sh2o_list_order[k]]);
        g = base;
        for(j = 0; j < n; j++) {
            v[j][0] = sh2o_list_lcg(&x) & mask;
            v[j][1] = sh2o_list_lcg(&x) & mask;
            sh2o_gte_write(&g,0,v[j][0]);
            sh2o_gte_write(&g,1,v[j][1]);
            (void)sh2o_gte_command(&g,cmd);
            for(k = 0; k < 6; k++)
                want[j][k] = sh2o_gte_read(&g,mac[k]);
            for(k = 0; k < 64; k++)
                regs[j][k] = sh2o_gte_read(&g,k);
            sw[j][0] = v[j][0],sw[j][1] = v[j][1];
        }
        c[0]++;
        c[1] += n;
        for(j = 1; j <= n; j++)
            c[2] += !sh2o_list_same(&base,cmd,(const uint32_t (*)[2])v,j,regs[j - 1],(const uint32_t (*)[6])want);
        if(n >= 2 && (v[0][0] != v[1][0] || v[0][1] != v[1][1])) {
            sw[0][0] = v[1][0],sw[0][1] = v[1][1],sw[1][0] = v[0][0],sw[1][1] = v[0][1];
            for(j = 1; j <= n && !caught; j++)
                caught = !sh2o_list_same(&base,cmd,(const uint32_t (*)[2])sw,j,regs[j - 1],(const uint32_t (*)[6])want);
            c[3] += caught;
            c[4]++;
        }
    }
}

void sh2o_list_test_counts(uint32_t c[5]) {
    sh2o_list_run(c);
}

/* 0 = pass; else 0x601 (a prefix differs from its singles), 0x602 (a planted reorder not caught, or none planted) */
int sh2o_list_test_suite(void) {
    uint32_t c[5];
    sh2o_list_run(c);
    if(c[2])
        return 0x601;
    if(c[3] != c[4] || c[4] == 0)
        return 0x602;
    return 0;
}
