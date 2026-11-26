/* The GTE machine (PS1 COP2) in C.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_GTE_H
#define SH2O_GTE_H

#include <stdint.h>

#include "sh2o/cdefs.h"
#ifdef __cplusplus
extern "C" {
#endif

/* New code from psx-spx's formulas (psx-spx.consoledev.net/geometrytransformationenginegte/). Built on K
 * (sh2o_k_unr_div, sh2o_k_clz32); no int64 x int64 multiply, no variable 64-bit shift, no divide instruction. */

/* The register image, in register index order: r[0..31] data (MTC2/MFC2), r[32..63] control (CTC2/CFC2).
 * Storage holds the written words; 16-bit fields are the halves read by shifts (endian-neutral). Zero is a valid
 * state. Commands keep their results here; sh2o_gte_read() gives the MFC2/CFC2 view. */
typedef struct sh2o_gte {
    uint32_t r[64];
} sh2o_gte_t;

/* Contract tag gte-regs-1.
 * sh2o_gte_read(g, reg): MFC2/CFC2 of reg 0..63 (the model's read_view): VZ0-2, IR0-3, RT33, L33, LB3, H, DQA,
 * ZSF3, ZSF4 sign-extended from 16 bits; OTZ, SZ0-3 zero-extended; 15 (SXYP) reads SXY2; 28/29 (IRGB/ORGB) = IR1-3
 * / 80h saturated 0..1Fh, packed 5:5:5; 31 (LZCR) = leading 0s (LZCS >= 0) or 1s (LZCS < 0) of LZCS; 63 (FLAG)
 * bits 12-30 as stored, bit 31 = OR of bits 30-23 and 18-13; every other register its full 32-bit word.
 * reg > 63: 0. */
uint32_t sh2o_gte_read(const sh2o_gte_t *g,unsigned reg);

/* Contract tag gte-regs-1.
 * sh2o_gte_write(g, reg, v): MTC2/CTC2 of v to reg 0..63: 15 (SXYP) pushes SXY0 <- SXY1 <- SXY2 <- v; 28 (IRGB)
 * also sets IR1-3 = its 5-bit fields << 7; 29 (ORGB) and 31 (LZCR) read-only (no effect); 63 (FLAG) keeps bits
 * 12-30; every other register stores v. reg > 63: no effect. */
void sh2o_gte_write(sh2o_gte_t *g,unsigned reg,uint32_t v);

/* Contract tag gte-command-1.
 * sh2o_gte_command(g, cmd): one GTE command (cmd: the COP2 word's low 25 bits; op = bits 0-5, sf 19, mx 17-18,
 * v 15-16, cv 13-14, lm 10). FLAG is reset at the start and accumulated (psx-spx bit meanings).
 * Returns 0, or -1 for an opcode not modelled, the image unchanged. Modelled: the 15 below. */
int sh2o_gte_command(sh2o_gte_t *g,uint32_t cmd);

/* One function per command (the op field of cmd is not checked; sf, lm, mx, v, cv are read from cmd). */
/* Contract tag gte-mvmva-1. */
void sh2o_gte_mvmva(sh2o_gte_t *g,uint32_t cmd); /* 12h: (TR|BK|FC|0)*1000h + Mx*Vx, mx=3 and cv=2 hw bugs */
/* Contract tag gte-nclip-1. */
void sh2o_gte_nclip(sh2o_gte_t *g,uint32_t cmd); /* 06h: MAC0 = SX0*SY1 + SX1*SY2 + SX2*SY0 - ... */
/* Contract tag gte-avsz3-1. */
void sh2o_gte_avsz3(sh2o_gte_t *g,uint32_t cmd); /* 2Dh: MAC0 = ZSF3*(SZ1+SZ2+SZ3), OTZ */
/* Contract tag gte-sqr-1. */
void sh2o_gte_sqr(sh2o_gte_t *g,uint32_t cmd);   /* 28h: IRn*IRn */
/* Contract tag gte-gpf-1. */
void sh2o_gte_gpf(sh2o_gte_t *g,uint32_t cmd);   /* 3Dh: IR*IR0, colour FIFO */
/* Contract tag gte-gpl-1. */
void sh2o_gte_gpl(sh2o_gte_t *g,uint32_t cmd);   /* 3Eh: IR*IR0 + MAC SHL sf, colour FIFO */
/* Contract tag gte-dpcs-1. */
void sh2o_gte_dpcs(sh2o_gte_t *g,uint32_t cmd);  /* 10h: RGB SHL 16, MAC+(FC-MAC)*IR0 */
/* Contract tag gte-dpct-1. */
void sh2o_gte_dpct(sh2o_gte_t *g,uint32_t cmd);  /* 2Ah: DPCS on RGB0, three times (colour FIFO) */
/* Contract tag gte-dpct32-1.
 * sh2o_gte_dpct32(g, cmd): the same image as sh2o_gte_dpct(g, cmd) for every image and cmd, computed in 32-bit
 * words only (no 64-bit value, no libgcc call, no MAC.L or MAC.W). On the SH-2 build sh2o_gte_dpct (and
 * sh2o_gte_command op 2Ah) selects it; the portable build and the host keep the C path. */
void sh2o_gte_dpct32(sh2o_gte_t *g,uint32_t cmd);
/* Contract tag gte-rtps-1. */
void sh2o_gte_rtps(sh2o_gte_t *g,uint32_t cmd);  /* 01h: perspective transform of V0, depth cue */
/* Contract tag gte-rtpt-1. */
void sh2o_gte_rtpt(sh2o_gte_t *g,uint32_t cmd);  /* 30h: RTPS on V0, V1, V2, depth cue on V2 */
/* Contract tag gte-ncs-1. */
void sh2o_gte_ncs(sh2o_gte_t *g,uint32_t cmd);   /* 1Eh: light V0, colour FIFO */
/* Contract tag gte-nct-1. */
void sh2o_gte_nct(sh2o_gte_t *g,uint32_t cmd);   /* 20h: NCS on V0, V1, V2 */
/* Contract tag gte-ncds-1. */
void sh2o_gte_ncds(sh2o_gte_t *g,uint32_t cmd);  /* 13h: light V0, RGB*IR SHL 4, MAC+(FC-MAC)*IR0 */
/* Contract tag gte-ncdt-1. */
void sh2o_gte_ncdt(sh2o_gte_t *g,uint32_t cmd);  /* 16h: NCDS on V0, V1, V2 */
/* Contract tag gte-nccs-1. */
void sh2o_gte_nccs(sh2o_gte_t *g,uint32_t cmd);  /* 1Bh: light V0, RGB*IR SHL 4 SAR sf, colour FIFO */
/* The fixed words of the PSY-Q macro family's nRTPS and nRTPT (sf 1, lm 0). The macros' two NOPs wait for the PS1
 * GTE's load delay, which this register image does not have. */
/* Contract tag gte-nrtps-1. */
void sh2o_gte_nrtps(sh2o_gte_t *g);              /* RTPS with 0x4A180001 */
/* Contract tag gte-nrtpt-1. */
void sh2o_gte_nrtpt(sh2o_gte_t *g);              /* RTPT with 0x4A280030 */

#ifdef __cplusplus
}
#endif
#endif
