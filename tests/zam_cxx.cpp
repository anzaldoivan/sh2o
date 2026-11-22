/* The SH2ZAM delegate header from C++: two delegate entries called and checked (SH2O_ZAM=ON only).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <cstdio>
#include "sh2o/zam_experimental.h"

int main() {
    int fails = 0;
    /* zam-ratan2-1: a quarter turn is 1024, as sh2o_psyq_ratan2 gives. */
    const int32_t a = sh2o_zam_ratan2(1, 0);
    if (a != 1024) {
        std::printf("FAIL sh2o_zam_ratan2(1, 0): got %ld, want 1024\n", static_cast<long>(a));
        ++fails;
    }
    /* zam-transposematrix-1: m1->m = m0->m transposed, pad and t of m1 untouched, returns m1. */
    sh2o_psyq_matrix_t m0 = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, 0, {0, 0, 0}};
    sh2o_psyq_matrix_t m1 = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 11, {12, 13, 14}};
    if (sh2o_zam_TransposeMatrix(&m0, &m1) != &m1) {
        std::printf("FAIL sh2o_zam_TransposeMatrix: not m1\n");
        ++fails;
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (m1.m[i][j] != m0.m[j][i]) {
                std::printf("FAIL sh2o_zam_TransposeMatrix: m[%d][%d]\n", i, j);
                ++fails;
            }
        }
    }
    if (m1.pad != 11 || m1.t[0] != 12 || m1.t[1] != 13 || m1.t[2] != 14) {
        std::printf("FAIL sh2o_zam_TransposeMatrix: pad or t changed\n");
        ++fails;
    }
    std::printf("zam_cxx: %d failures\n", fails);
    return fails != 0;
}
