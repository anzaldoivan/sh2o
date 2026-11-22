/* Check of the private input variables: set, there and outside the tree.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include "private_input.h"

/* The variables, in the order of docs/EXACTNESS.md. */
static const char *const sh2oi_variables[]={
    "SH2O_PS1_LIBGTE_OBJECTS",
    "SH2O_PS1_LINK_MAP",
    "SH2O_PS1_DATA",
    "SH2O_PS1_GTE_RECORDINGS",
};

/* argv[1] is the source folder. A path in the repository fails the test.
 * Otherwise a variable that is not set, or a path that is not there, skips
 * the test. The test passes when each input is there. */
int main(int argc,char **argv) {
    size_t i;
    int result=0;
    if(argc!=2) {
        printf("usage: %s <source folder>\n",argv[0]);
        return SH2O_PRIVATE_FAIL;
    }
    for(i=0;i<sizeof sh2oi_variables/sizeof sh2oi_variables[0];i++) {
        const char *path;
        int status=sh2oi_private_input(sh2oi_variables[i],argv[1],&path);
        if(status==0) {
            printf("found: %s=%s\n",sh2oi_variables[i],path);
        } else if(status==SH2O_PRIVATE_FAIL) {
            result=SH2O_PRIVATE_FAIL;
        } else if(result==0) {
            result=SH2O_PRIVATE_SKIP;
        }
    }
    return result;
}
