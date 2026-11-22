/* A consumer program that calls each kernel of sh2o/k.h.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#include <stdio.h>
#include "sh2o/k.h"

int main(void) {
    static const int16_t m[3]={-32768,12345,-1};
    static const int16_t v[3]={32767,-23456,2};
    static const uint32_t w[5]={0x8000FFFFu,0x12347FFFu,0xFFFF0001u,0x00018000u,0x00007FFFu};
    static const int32_t t[3]={INT32_MIN,0x12345,-7};
    static const uint32_t mac[3]={0x80000000u,0x7FFFFu,0x1234u};
    int64_t out[3];
    printf("dot3b %lld\n",(long long)sh2o_k_dot3b(m,v,-5));
    sh2o_k_mvb3(w,v,t,out);
    printf("mvb3 %lld %lld %lld\n",(long long)out[0],(long long)out[1],(long long)out[2]);
    sh2o_k_gpf3(w,out);
    printf("gpf3 %lld %lld %lld\n",(long long)out[0],(long long)out[1],(long long)out[2]);
    sh2o_k_gpl3(w,mac,1u,out);
    printf("gpl3 %lld %lld %lld\n",(long long)out[0],(long long)out[1],(long long)out[2]);
    printf("clz32 %u\n",sh2o_k_clz32(0x00012345u));
    printf("unr_recip %lu\n",(unsigned long)sh2o_k_unr_recip(0xA000u));
    printf("unr_div %lu\n",(unsigned long)sh2o_k_unr_div(1000u,1500u));
    return 0;
}
