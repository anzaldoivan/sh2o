/* A consumer program that writes, runs and reads the GTE of sh2o/gte.h.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <stdio.h>
/* The tag alone, before the header, as a program that only passes the image on declares it. */
struct sh2o_gte;
#include "sh2o/gte.h"

int main(void) {
    static sh2o_gte_t g;
    sh2o_gte_write(&g,12,0x00100010u);
    sh2o_gte_write(&g,13,0x00200030u);
    sh2o_gte_write(&g,14,0x00500020u);
    printf("nclip %d\n",sh2o_gte_command(&g,0x06u));
    printf("mac0 %lu\n",(unsigned long)sh2o_gte_read(&g,24));
    printf("flag %lu\n",(unsigned long)sh2o_gte_read(&g,63));
    return 0;
}
