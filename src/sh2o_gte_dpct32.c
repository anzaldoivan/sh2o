/* DPCT in 32-bit words.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* The same register image as sh2o_gte_dpct (src/sh2o_gte.c) for every image and cmd, with int32 and uint32 values
 * only: no 64-bit value, no libgcc helper, no MAC.L or MAC.W. Per step n (0-2) and channel i (0-2), with b the byte i
 * of RGB0 (r[20], read again at each step) and fc the word FC i (r[53 + i]):
 * - step 1: the 44-bit sum (fc SHL 12) - (b SHL 16) is 4096 (fc - 16 b), at most 2^43 - 1; it is below -2^43 (FLAG
 *   27-i) when fc - 16 b overflows int32, i.e. when fc + 2^31 < 16 b in unsigned words. MAC = fc - 16 b (sf 1, the
 *   sum SAR 12 is the wrapped difference) or (fc SHL 12) - (b SHL 16) (sf 0, the low word); IR saturated with lm 0.
 * - step 2: IR * IR0 + (b SHL 16) is at most 2^30 + 2^24 in size: it fits int32 and never sets a MAC flag.
 *   MAC = that sum SAR 12 (sf 1) or the sum (sf 0); IR saturated with lm; the colour FIFO as sh2o_gte_dpct.
 * Shifts are by constants and logical (sh2oi_asr for SAR), and the code calls no function, so that the SH-2 object
 * has no call and no undefined symbol (tests/sh2/dpct32_static.txt). It does not count calls (SH2O_COUNT). */
#include <stdint.h>

#include "sh2o_gte_internal.h"

/* Test-only planted bugs (tests/test_dpct32.c includes this file with each): 1 the FLAG bit 27-i of step 1 is
 * dropped; 2 step 2 shifts when sf is 0, not 1; 3 RGB0 is read once, not at each step. 0 (default): none. */
#ifndef SH2OI_DPCT32_MUTANT
#define SH2OI_DPCT32_MUTANT 0
#endif

/* Contract tag gte-dpct32-1. Contract in sh2o/gte.h. */
void sh2o_gte_dpct32(sh2o_gte_t *g,uint32_t cmd)
{
    uint32_t flag = 0; /* FLAG reset at the command start */
    uint32_t sf = cmd >> 19 & 1u;
    int32_t lo = (cmd >> 10 & 1u) ? 0 : -0x8000; /* lower IR limit of step 2 (lm) */
    int32_t ir0 = (int32_t)((g->r[8] & 0xFFFFu) ^ 0x8000u) - 0x8000;
    unsigned n,i;
#if SH2OI_DPCT32_MUTANT == 3
    uint32_t rgb0 = g->r[20];
#endif
    for(n = 0; n < 3; n++)
    {
#if SH2OI_DPCT32_MUTANT == 3
        uint32_t c = rgb0;
#else
        uint32_t c = g->r[20]; /* RGB0: each step's push moves the next colour down to it */
#endif
        uint32_t col = 0; /* the pushed colour's bytes, channel i in bits 16-23 after its turn */
        uint32_t fb = 1u << 27,ib = 1u << 24,cb = 1u << 21; /* FLAG bits 27-i, 24-i, 21-i */
        for(i = 0; i < 3; i++)
        {
            uint32_t b = c & 0xFFu,fc = g->r[53 + i];
            int32_t mac,ir,v;
            /* step 1 */
#if SH2OI_DPCT32_MUTANT == 1
            (void)fb;
#else
            if((fc ^ 0x80000000u) < b << 4)
                flag |= fb;
#endif
            mac = (int32_t)(sf ? fc - (b << 4) : (fc << 12) - (b << 16));
            if(mac < -0x8000)
            {
                flag |= ib;
                ir = -0x8000;
            }
            else if(mac > 0x7FFF)
            {
                flag |= ib;
                ir = 0x7FFF;
            }
            else
                ir = mac;
            /* step 2 */
            mac = ir * ir0 + (int32_t)(b << 16);
#if SH2OI_DPCT32_MUTANT == 2
            if(!sf)
#else
            if(sf)
#endif
                mac = sh2oi_asr(mac,12);
            if(mac < lo)
            {
                flag |= ib;
                ir = lo;
            }
            else if(mac > 0x7FFF)
            {
                flag |= ib;
                ir = 0x7FFF;
            }
            else
                ir = mac;
            g->r[25 + i] = (uint32_t)mac;
            g->r[9 + i] = (uint32_t)ir;
            /* colour channel: MAC SAR 4 saturated 0..FFh; no FFh literal, whose pool word 00FFh objdump shows as
             * mac.l (the static check forbids it) */
            v = sh2oi_asr(mac,4);
            if((uint32_t)v >> 8)
            {
                flag |= cb;
                v = (int32_t)(uint8_t)(((uint32_t)v >> 31) - 1u); /* 0 below 0, FFh above */
            }
            col = col >> 8 | (uint32_t)v << 16;
            c >>= 8;
            fb >>= 1;
            ib >>= 1;
            cb >>= 1;
        }
        /* Colour FIFO: RGB0 <- RGB1 <- RGB2 <- [colour, CODE] */
        g->r[20] = g->r[21];
        g->r[21] = g->r[22];
        g->r[22] = (g->r[6] & 0xFF000000u) | col;
    }
    g->r[63] = flag;
}
