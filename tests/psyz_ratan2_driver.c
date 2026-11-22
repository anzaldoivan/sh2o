/* Sweep driver of sh2o_psyq_ratan2 against the psyz ratan2.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */

/* tests/psyz_ratan2_sweep.py builds this file with host cc, every src/ C file
 * and the psyz ratan.c, compiled in place with -Dratan2=p_ratan2 (the
 * reference; no text of it is in this file).
 * `sweep`: sh2o_psyq_ratan2 against p_ratan2 on (1) the grid y, x in
 * -2048..2048, (2) 2^20 xorshift64 pairs (seed 0x5348325A414D5330, y then x =
 * the high 32 bits of successive states, shifts 13, 7, 17), (3) the 13 x 13
 * edge set. A pair with INT32_MIN as y or x is not given to p_ratan2 (its `-dx`
 * overflows the PS1's 32-bit long: undefined behaviour) and is counted apart.
 * Out, per set: "<set> <pairs> pairs <compared> compared <excluded> excluded
 * <diffs> differences <breaks> breaks". A break inside the compared set is a
 * difference. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh2o/sh2o.h"

long p_ratan2(long y,long x);

static uint32_t brk_code,brk_calls;

static void on_break(uint32_t code) {
    brk_code=code;
    brk_calls++;
}

static uint64_t rng=0x5348325A414D5330ull;

static uint32_t next32(void) {
    rng^=rng<<13;
    rng^=rng>>7;
    rng^=rng<<17;
    return (uint32_t)(rng>>32);
}

typedef struct tally {
    unsigned long pairs,compared,excluded,diffs,breaks;
} tally;

static void one(tally *t,int32_t y,int32_t x) {
    int32_t r;
    brk_calls=0;
    r=sh2o_psyq_ratan2(NULL,y,x);
    t->pairs++;
    t->breaks+=brk_calls!=0;
    if(y==INT32_MIN||x==INT32_MIN) {
        t->excluded++;
        return;
    }
    t->compared++;
    t->diffs+=brk_calls!=0||(long)r!=p_ratan2(y,x);
}

static void report(const char *set,const tally *t) {
    printf("%s %lu pairs %lu compared %lu excluded %lu differences %lu breaks\n",set,t->pairs,t->compared,
        t->excluded,t->diffs,t->breaks);
}

int main(int argc,char **argv) {
    static const int32_t ev[13]={0,1,-1,(1<<21)-1,1<<21,(1<<21)+1,-(1<<21)+1,-(1<<21),-(1<<21)-1,
        INT32_MIN,INT32_MIN+1,INT32_MAX,INT32_MAX-1};
    sh2o_psyq_break=on_break;
    if(argc>1&&!strcmp(argv[1],"sweep")) {
        tally g={0,0,0,0,0},r={0,0,0,0,0},e={0,0,0,0,0};
        int32_t y,x;
        unsigned long i;
        unsigned a,b;
        for(y=-2048;y<=2048;y++)
            for(x=-2048;x<=2048;x++)
                one(&g,y,x);
        for(i=0;i<(1ul<<20);i++) {
            y=(int32_t)next32();
            x=(int32_t)next32();
            one(&r,y,x);
        }
        for(a=0;a<13;a++)
            for(b=0;b<13;b++)
                one(&e,ev[a],ev[b]);
        report("grid",&g);
        report("xorshift",&r);
        report("edges",&e);
        return 0;
    }
    fprintf(stderr,"usage: %s sweep\n",argv[0]);
    return 2;
}
