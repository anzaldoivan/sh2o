/* PSY-Q libgte bodies over the GTE image, one per PS1 routine.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_PSYQ_H
#define SH2O_PSYQ_H

#include <stdint.h>

#include "sh2o/gte.h"
#ifdef __cplusplus
extern "C" {
#endif

/* New code written from the method of each routine's PSY-Q object code; no text of the SDK, of another port or of a
 * decompilation. sh2o_psyq_<PSY-Q name>: the routine's steps in its order; GTE traffic only
 * through sh2o_gte_read (MFC2/CFC2/SWC2) / sh2o_gte_write (MTC2/CTC2/LWC2) / sh2o_gte_command, the same registers,
 * order and words as the object; except where a contract below says "direct": image words g->r[]
 * stored or read in place for registers whose accessor is a plain store or full-word read (V0 0-1, MAC1-3 25-27,
 * RT 32-36). Never a bare PSY-Q name (other ports export those). Equal to an R3000 interpreter running the
 * object's bytes on return, memory out and all 64 GTE registers.
 * Memory: structs hold native SH-2 (host) integers; convert PS1 little-endian data (a file, a dump) to them first;
 * never copy PS1 bytes in. Bodies read and write by field (a PS1 word = low | high << 16); arguments may alias as
 * the object allows (each field is read when the object loads its word). Divides: where the object's `div` meets a
 * zero divisor (`break 7`, BRK 1C00h) or INT32_MIN / -1 (`break 6`, BRK 1800h), the body calls sh2o_psyq_break(code)
 * and stops (below); trapping add/addi/sub wrap (the object traps on signed overflow; outside every domain below).
 * Not replayed: the PS1 COP0 SR write (InitGeom), BIOS _patch_gte (InitGeom), printf (Push/PopMatrix: return -1),
 * the object's two return-address spill words.
 * Identity, per body: the sha256 of its object's .text bytes (unrelocated, symbol to the next .text symbol or the
 * section end) and of each table range it reads. */

/* Break hook, set by the caller (0: none). PS1: BRK 1C00h -> BIOS SystemErrorUnresolvedException (hang); sh2o: break
 * hook. A body that reaches one of its object's `break` sites calls it with the BRK code (1C00h zero divisor, 1800h
 * INT32_MIN / -1) and does not go on: if the hook returns, the body writes nothing more and returns 0 (void: returns). */
extern void (*sh2o_psyq_break)(uint32_t code);

/* PS1 MATRIX (32 bytes): m at 0..0x11 (row major), pad 0x12, t at 0x14, 0x18, 0x1C */
typedef struct sh2o_psyq_matrix {
    int16_t m[3][3];
    int16_t pad;
    int32_t t[3];
} sh2o_psyq_matrix_t;

/* PS1 VECTOR (16 bytes) */
typedef struct sh2o_psyq_vector {
    int32_t vx,vy,vz,pad;
} sh2o_psyq_vector_t;
/* PS1 SVECTOR (8 bytes) */
typedef struct sh2o_psyq_svector {
    int16_t vx,vy,vz,pad;
} sh2o_psyq_svector_t;

/* PushMatrix/PopMatrix's state (the object's two stack globals: top and the stack), caller-owned; zero = empty.
 * top: bytes used, 0..0x280 in steps of 32 (20 entries); entry k = w[8k..8k+7]: CFC2 32..39 (RT, TR) as read. */
typedef struct sh2o_psyq_mstack {
    int32_t top;
    uint32_t w[160];
} sh2o_psyq_mstack_t;

/* Contract tag psyq-setrotmatrix-1.
 * SetRotMatrix(m): CTC2 32..36 <- m's words 0..4 (direct).
 * Identity: .text sha256 095799477a23b46afa40d99dc027e7ef467cd14b8e135234d7e5b5c9ebc07883 */
void sh2o_psyq_SetRotMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);

/* Contract tag psyq-setlightmatrix-1.
 * SetLightMatrix(m): CTC2 40..44 <- m's words 0..4.
 * Identity: .text sha256 b9ce521ecd6cadeda48a0828b73f9b37d260c4f83e4d3b099334608f9e8fe598 */
void sh2o_psyq_SetLightMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);

/* Contract tag psyq-setcolormatrix-1.
 * SetColorMatrix(m): CTC2 48..52 <- m's words 0..4.
 * Identity: .text sha256 32169d0ce375b3a1a24a5beadfd11cd857ce0a89d610ddf3a1f3855fa6ed34f6 */
void sh2o_psyq_SetColorMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);

/* Contract tag psyq-settransmatrix-1.
 * SetTransMatrix(m): CTC2 37..39 <- m->t.
 * Identity: .text sha256 2f228789930df0a6d6db76e145ba3301694d7c01b9df2dbf45d2f48ad7aaee1e */
void sh2o_psyq_SetTransMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m);

/* Contract tag psyq-setbackcolor-1.
 * SetBackColor(r, g, b): CTC2 45..47 <- each << 4.
 * Identity: .text sha256 e4dc8b65f4b8d01e34793cfd9e1ecc838abd27d016ca1c2a4291a20b613ab61e */
void sh2o_psyq_SetBackColor(sh2o_gte_t *g,int32_t rbk,int32_t gbk,int32_t bbk);

/* Contract tag psyq-setfarcolor-1.
 * SetFarColor(r, g, b): CTC2 53..55 <- each << 4.
 * Identity: .text sha256 7c5febbc8fceb4e4b3065c8276416832651867316006f05c53e0a20b893d522d */
void sh2o_psyq_SetFarColor(sh2o_gte_t *g,int32_t rfc,int32_t gfc,int32_t bfc);

/* Contract tag psyq-setgeomoffset-1.
 * SetGeomOffset(ofx, ofy): CTC2 56, 57 <- each << 16.
 * Identity: .text sha256 9dea63b6a2984fc5f10a791e9a62d26dc22f571773be574799f9a1e77432dfa2 */
void sh2o_psyq_SetGeomOffset(sh2o_gte_t *g,int32_t ofx,int32_t ofy);

/* Contract tag psyq-setgeomscreen-1.
 * SetGeomScreen(h): CTC2 58 <- h.
 * Identity: .text sha256 05167a2e53143880036b5e3fcdcc28b55b50d5149904a3779121cbe420cd8e20 */
void sh2o_psyq_SetGeomScreen(sh2o_gte_t *g,int32_t h);

/* Contract tag psyq-initgeom-1.
 * InitGeom(): CTC2 61 = 155h, 62 = 100h, 58 = 3E8h, 59 = -1062h, 60 = 1400000h, 56 = 0, 57 = 0 (the object also calls
 * _patch_gte and sets COP0 SR bit 30: not replayed).
 * Identity: .text sha256 ed076abe41aa6b5a634dd74010cfc8b84cd75f7ee098c1b0533512c66828670f */
void sh2o_psyq_InitGeom(sh2o_gte_t *g);

/* Contract tag psyq-setfognearfar-1.
 * SetFogNearFar(a, b, h): d = b - a; d < 100: nothing. Else DQA (59) <- ((-a*b / d) << 8) / h clamped to
 * -8000h..7FFFh, DQB (60) <- ((b << 12) / d) << 12 (the object's SetDQA, SetDQB calls). Domain: any. h = 0, or
 * h = -1 with (-a*b / d) << 8 = INT32_MIN: PS1: BRK 1C00h / 1800h -> BIOS SystemErrorUnresolvedException (hang);
 * sh2o: break hook (sh2o_psyq_break), then nothing written.
 * Identity: .text sha256 6e592fd5a54cdbc5271bba919ba74b5ca0b1614b673e0352a789e318fbdb4473 */
void sh2o_psyq_SetFogNearFar(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h);

/* Contract tag psyq-transmatrix-1.
 * TransMatrix(m, v): m->t = v's x, y, z; returns m. No GTE traffic.
 * Identity: .text sha256 76e279d785a33faaa549a9fd86174713d87230858e0b12d2c54132d33274ea1e */
sh2o_psyq_matrix_t *sh2o_psyq_TransMatrix(sh2o_gte_t *g,sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v);

/* Contract tag psyq-transposematrix-1.
 * TransposeMatrix(m0, m1): m1's m = m0's m transposed (pad, t untouched); m1 may be m0; returns m1. No GTE traffic.
 * Identity: .text sha256 9fc271c9be12d61166eb7b2f679904083cb1caa92cb3e64d437e44d2598bf93e */
sh2o_psyq_matrix_t *sh2o_psyq_TransposeMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1);

/* Contract tag psyq-compmatrix-1.
 * CompMatrix(m0, m1, m2): RT = m0; m2's m = IR1-3 of MVMVA (sf 1, RT, V0, no translation) on each column of m1
 * (m2->pad = IR3's high half of the last column); m2->t = m0->t + MAC1-3 of the same on m1->t's low halves (VXY0)
 * and m1->t[2] (VZ0). Domain: no signed overflow in m0->t + MAC (the object's add traps). Returns m2.
 * Identity: .text sha256 484fff1d2984fac9730786048ad50065c006672118c2d6b4698dfb787d7b155c */
sh2o_psyq_matrix_t *sh2o_psyq_CompMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,const sh2o_psyq_matrix_t *m1,
                                            sh2o_psyq_matrix_t *m2);

/* Contract tag psyq-applymatrix-1.
 * ApplyMatrix(m, v0, v1): RT = m; V0 = v0 (VXY0, VZ0 with pad in the high half); MVMVA (sf 1, RT, V0, no
 * translation; 4A486012h); v1's x, y, z = MAC1-3 (v1->pad untouched). Returns v1. Every read precedes every write:
 * v1 may alias m. Direct.
 * Identity: .text sha256 b096f04a8aded97409fb838a52099375501261af496b816374e6c520a19b30b8 */
sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_svector_t *v0,
                                            sh2o_psyq_vector_t *v1);

/* Contract tag psyq-applymatrixlv-1.
 * ApplyMatrixLV(m, v0, v1): RT = m; each v0 component split into the high part (SAR 15) and 15-bit low part (sign
 * kept); MVMVA (sf 0, RT, IR) on the high parts, MAC << 3, plus MVMVA (sf 1) on the low parts -> v1's x, y, z.
 * Returns v1.
 * Identity: .text sha256 704b7d8ebd6995ceb4134b7c68371678756cff1becec9850d930302d8aeabd02 */
sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrixLV(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v0,
                                            sh2o_psyq_vector_t *v1);

/* Contract tag psyq-rottrans-1.
 * RotTrans(v0, v1, flag): V0 = v0; MVMVA (sf 1, RT, V0, TR); v1's x, y, z = MAC1-3; *flag = FLAG.
 * Identity: .text sha256 070b39be84985197f580f9e06e2ba32990bcf3ddd3ba0d5f958aec5bb0b66145 */
void sh2o_psyq_RotTrans(sh2o_gte_t *g,const sh2o_psyq_svector_t *v0,sh2o_psyq_vector_t *v1,int32_t *flag);

/* Contract tag psyq-pushmatrix-1.
 * PushMatrix(s): top < 280h: entry top/32 = CFC2 32..39, top += 32, 0. Full: nothing, -1 (the object prints).
 * -2: top outside the domain (negative or not a multiple of 32; the object writes outside its stack), nothing.
 * Identity: .text sha256 f5ded2c0768b041b1eb7b9ada5144d34d932884eff435cde2cfa8c144a218af9 */
int sh2o_psyq_PushMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s);

/* Contract tag psyq-popmatrix-1.
 * PopMatrix(s): top > 0: top -= 32, CTC2 32..39 = entry top/32, 0. Empty: nothing, -1 (the object prints).
 * -2: top outside the domain (above 280h or not a multiple of 32), nothing.
 * Identity: .text sha256 2fe454224bcfede5ac090f1aaf98b391d63c337ff6818c9fd47f188e5f265beb */
int sh2o_psyq_PopMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s);

/* Contract tag psyq-ratan2-1.
 * ratan2(y, x): atan2 in 4096 units per turn from |y|, |x| (negated in 32 bits: INT32_MIN stays negative), the octant
 * ratio q = (small << 10) / big (or small / (big >> 10) when small has bits 21-30) into ratan_tbl; 0 for (0, 0).
 * (0, INT32_MIN) and (INT32_MIN, 0) divide 0 by 0, the object's `break 7`: PS1: BRK 1C00h -> BIOS
 * SystemErrorUnresolvedException (hang); sh2o: break hook (sh2o_psyq_break), then returns 0.
 * No GTE traffic. Table: 1025 entries from the formula at its definition (src/sh2o_psyq.c), equal to the object's.
 * Identity: .text sha256 f0250f2b6a10a20b9b1f54d835ddb099030f564c0d81f801dad5ab285aa94655
 *     table sha256 b95a5540f30e1f0f40975157595d078bb07d3242d2c856a1c679e533e2f26233 (the arctangent table),
 *     table sha256 96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7 (the bytes before
 *     the table in the link) */
int32_t sh2o_psyq_ratan2(sh2o_gte_t *g,int32_t y,int32_t x);

#ifdef __cplusplus
}
#endif
#endif
