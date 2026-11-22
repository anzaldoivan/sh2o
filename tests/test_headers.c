/* Every public header in one translation unit, in sorted order.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "sh2o/count.h"
#include "sh2o/fixed.h"
#include "sh2o/gte.h"
#include "sh2o/gte_prepared.h"
#include "sh2o/k.h"
#include "sh2o/list.h"
#include "sh2o/psyq.h"
#include "sh2o/sh2o.h"
#include "sh2o/zam_experimental.h"
#include <stdio.h>

int main(void) {
    printf("headers: sh2o %s, backend %s\n",SH2O_VERSION,sh2o_backend());
    return 0;
}
