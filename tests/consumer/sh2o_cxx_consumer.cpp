/* A C++ consumer program: each installed header, one call of each family, results checked.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <cstdio>
#include "sh2o/sh2o.h"
#include "sh2o/cdefs.h"
#include "sh2o/count.h"
#include "sh2o/fixed.h"
#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o/list.h"
#include "sh2o/psyq.h"

static int fails = 0;

static void check(const char *what, long long got, long long want) {
    if (got != want) {
        std::printf("FAIL %s: got %lld, want %lld\n", what, got, want);
        ++fails;
    }
}

int main() {
    /* fixed: 2.0 * 3.0 = 6.0 in Q12. */
    check("sh2o_mul_q12", sh2o_mul_q12(2 * 4096, 3 * 4096), 6 * 4096);

    /* k: (tr << 12) + m.v. */
    static const int16_t m[3] = {1, 2, 3};
    static const int16_t v[3] = {4, 5, 6};
    check("sh2o_k_dot3b", sh2o_k_dot3b(m, v, 1), 4096 + 32);

    /* gte: write and read back a data register; an opcode not modelled returns -1. */
    sh2o_gte_t g = {};
    sh2o_gte_write(&g, 1u, 0x1234u);
    check("sh2o_gte_read", sh2o_gte_read(&g, 1u), 0x1234);
    check("sh2o_gte_command", sh2o_gte_command(&g, 0x3Fu), -1);

    /* list: RT the identity (4096 on the diagonal), TR 0; MVMVA sf 1, mx 0, v 0, cv 0 gives IR = MAC = V0. */
    sh2o_gte_t h = {};
    sh2o_gte_write(&h, 32u, 0x1000u);
    sh2o_gte_write(&h, 34u, 0x1000u);
    sh2o_gte_write(&h, 36u, 0x1000u);
    static const uint32_t items[1][2] = {{(0xFFFDu << 16) | 7u, 0x0009u}};
    uint32_t out[1][6];
    sh2o_gte_mvmva_list(&h, 0x80012u, items, 1u, out);
    check("list IR1", static_cast<int32_t>(out[0][0]), 7);
    check("list IR2", static_cast<int32_t>(out[0][1]), -3);
    check("list IR3", static_cast<int32_t>(out[0][2]), 9);
    check("list MAC2", static_cast<int32_t>(out[0][4]), -3);

    /* psyq: a quarter turn is 1024. */
    check("sh2o_psyq_ratan2", sh2o_psyq_ratan2(&g, 1, 0), 1024);

    std::printf("sh2o_cxx_consumer: %s, %d failures\n", sh2o_backend(), fails);
    return fails != 0;
}
