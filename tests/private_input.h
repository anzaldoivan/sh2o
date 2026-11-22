/* Private inputs for host tests, given by environment variables.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_PRIVATE_INPUT_H
#define SH2O_PRIVATE_INPUT_H

/* A private input (a Sony object, a map or data file of a PS1 build, a GTE
 * recording) is never in git. A test gets its path from an environment
 * variable. Each name is in docs/EXACTNESS.md. The test is registered with
 * sh2o_add_private_test (CMakeLists.txt), so the exit code
 * SH2O_PRIVATE_SKIP makes CTest show the test as skipped. A path in the
 * repository is an error: private bytes must not go into the tree. Include
 * this header first in the test, before any system header. */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SH2O_PRIVATE_SKIP 77 /* CTest SKIP_RETURN_CODE */
#define SH2O_PRIVATE_FAIL 1

/* Return nonzero if path is root or a path below root. Both are absolute. */
static int sh2oi_path_below(const char *path,const char *root) {
    size_t size=strlen(root);
    if(strncmp(path,root,size)!=0) {
        return 0;
    }
    return path[size]=='\0'||path[size]=='/'||(size>0&&root[size-1]=='/');
}

/* Check the input that variable names. root is the source folder of the
 * repository. Return 0 and set *path when the input is there, return
 * SH2O_PRIVATE_SKIP when the variable is not set or the path is not there,
 * and SH2O_PRIVATE_FAIL when the path is in the repository. Each result
 * other than 0 prints its reason. */
static int sh2oi_private_input(const char *variable,const char *root,const char **path) {
    char real_root[PATH_MAX];
    char full[PATH_MAX];
    const char *value=getenv(variable);
    *path=NULL;
    if(value==NULL||value[0]=='\0') {
        printf("skipped: %s is not set\n",variable);
        return SH2O_PRIVATE_SKIP;
    }
    if(realpath(root,real_root)==NULL) {
        printf("error: cannot resolve the source folder %s\n",root);
        return SH2O_PRIVATE_FAIL;
    }
    if(realpath(value,full)==NULL) {
        /* The path is not there: compare its absolute form. */
        char cwd[PATH_MAX];
        int size;
        if(value[0]!='/'&&getcwd(cwd,sizeof cwd)!=NULL) {
            size=snprintf(full,sizeof full,"%s/%s",cwd,value);
        } else {
            size=snprintf(full,sizeof full,"%s",value);
        }
        if(size<0||(size_t)size>=sizeof full) {
            printf("error: %s=%s is too long\n",variable,value);
            return SH2O_PRIVATE_FAIL;
        }
        if(sh2oi_path_below(full,real_root)||sh2oi_path_below(full,root)) {
            printf("error: %s=%s is in the repository %s; keep private inputs outside it\n",variable,value,real_root);
            return SH2O_PRIVATE_FAIL;
        }
        printf("skipped: %s=%s is not there\n",variable,value);
        return SH2O_PRIVATE_SKIP;
    }
    if(sh2oi_path_below(full,real_root)) {
        printf("error: %s=%s is in the repository %s; keep private inputs outside it\n",variable,value,real_root);
        return SH2O_PRIVATE_FAIL;
    }
    *path=value;
    return 0;
}

#endif
