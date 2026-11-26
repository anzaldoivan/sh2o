/* PSY-Q libgte bodies over the GTE image.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Contracts in sh2o/psyq.h.
 * New code: each body follows its PS1 object's steps in order (identity per body in the header).
 * Words: SH2OI_W(lo, hi) builds the PS1 word of two 16-bit fields, SH2OI_LO()/SH2OI_HI() split one; 32-bit fields
 * are words. Arithmetic on uint32_t (MIPS addu/subu/sll wrap); arithmetic right shifts through sh2oi_sra, divides
 * through sh2oi_div (the object's compiler checks: a zero divisor or INT32_MIN / -1 calls sh2o_psyq_break and the
 * body stops; never a C divide by zero or INT32_MIN / -1). Includes only <stdint.h> and sh2o headers. */
#include <stdint.h>

#include "sh2o/gte.h"
#include "sh2o/psyq.h"
#include "sh2o_count_internal.h"

#define SH2OI_W(lo,hi) ((uint32_t)(uint16_t)(lo) | (uint32_t)(uint16_t)(hi) << 16)
#define SH2OI_LO(w) ((int16_t)(uint16_t)((w) & 0xFFFFu))
#define SH2OI_HI(w) ((int16_t)(uint16_t)((w) >> 16))

/* MIPS sra: arithmetic right shift by s (1..31), defined for negative v */
static int32_t sh2oi_sra(int32_t v,unsigned s) {
    return v < 0 ? (int32_t)~(~(uint32_t)v >> s) : (int32_t)((uint32_t)v >> s);
}

void (*sh2o_psyq_break)(uint32_t code);

/* MIPS div, mflo, with the checks the object's compiler put around every div: d == 0 -> `break 7` (BRK 1C00h),
 * d == -1 and n == INT32_MIN -> `break 6` (BRK 1800h). 1 and *q = n / d; or 0 after calling the hook (q untouched) */
static int sh2oi_div(int32_t n,int32_t d,int32_t *q) {
    uint32_t code = d == 0 ? 0x1C00u : d == -1 && n == INT32_MIN ? 0x1800u : 0u;
    if(code) {
        if(sh2o_psyq_break)
            sh2o_psyq_break(code);
        return 0;
    }
    *q = n / d;
    return 1;
}

/* the five words of a matrix's m (and pad) as the object's lw 0..0x10 read them */
static void sh2oi_mwords(const sh2o_psyq_matrix_t *m,uint32_t w[5]) {
    w[0] = SH2OI_W(m->m[0][0],m->m[0][1]);
    w[1] = SH2OI_W(m->m[0][2],m->m[1][0]);
    w[2] = SH2OI_W(m->m[1][1],m->m[1][2]);
    w[3] = SH2OI_W(m->m[2][0],m->m[2][1]);
    w[4] = SH2OI_W(m->m[2][2],m->pad);
}

/* CTC2 base..base+4 <- the five m words (SetLightMatrix, SetColorMatrix, CompMatrix, ApplyMatrixLV) */
static void sh2oi_set3(sh2o_gte_t *g,unsigned base,const sh2o_psyq_matrix_t *m) {
    uint32_t w[5];
    unsigned k;
    sh2oi_mwords(m,w);
    for(k = 0; k < 5; k++)
        sh2o_gte_write(g,base + k,w[k]);
}

/* RT (CTC2 32..36, plain stores) <- the five m words, straight into the image (no accessor per word) */
static void sh2oi_rt(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m) {
    uint32_t *r = &g->r[32];
    SH2OI_COUNT_STORE(SH2O_COUNT_WRITE_RT,5);
    r[0] = SH2OI_W(m->m[0][0],m->m[0][1]);
    r[1] = SH2OI_W(m->m[0][2],m->m[1][0]);
    r[2] = SH2OI_W(m->m[1][1],m->m[1][2]);
    r[3] = SH2OI_W(m->m[2][0],m->m[2][1]);
    r[4] = SH2OI_W(m->m[2][2],m->pad);
}

/* Contract tag psyq-setrotmatrix-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetRotMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETROTMATRIX);
    sh2oi_rt(g,m);
}

/* Contract tag psyq-setlightmatrix-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetLightMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETLIGHTMATRIX);
    sh2oi_set3(g,40,m);
}

/* Contract tag psyq-setcolormatrix-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetColorMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETCOLORMATRIX);
    sh2oi_set3(g,48,m);
}

/* Contract tag psyq-settransmatrix-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetTransMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETTRANSMATRIX);
    sh2o_gte_write(g,37,(uint32_t)m->t[0]);
    sh2o_gte_write(g,38,(uint32_t)m->t[1]);
    sh2o_gte_write(g,39,(uint32_t)m->t[2]);
}

/* Contract tag psyq-setbackcolor-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetBackColor(sh2o_gte_t *g,int32_t rbk,int32_t gbk,int32_t bbk) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETBACKCOLOR);
    sh2o_gte_write(g,45,(uint32_t)rbk << 4);
    sh2o_gte_write(g,46,(uint32_t)gbk << 4);
    sh2o_gte_write(g,47,(uint32_t)bbk << 4);
}

/* Contract tag psyq-setfarcolor-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetFarColor(sh2o_gte_t *g,int32_t rfc,int32_t gfc,int32_t bfc) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETFARCOLOR);
    sh2o_gte_write(g,53,(uint32_t)rfc << 4);
    sh2o_gte_write(g,54,(uint32_t)gfc << 4);
    sh2o_gte_write(g,55,(uint32_t)bfc << 4);
}

/* Contract tag psyq-setgeomoffset-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetGeomOffset(sh2o_gte_t *g,int32_t ofx,int32_t ofy) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETGEOMOFFSET);
    sh2o_gte_write(g,56,(uint32_t)ofx << 16);
    sh2o_gte_write(g,57,(uint32_t)ofy << 16);
}

/* Contract tag psyq-setgeomscreen-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetGeomScreen(sh2o_gte_t *g,int32_t h) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETGEOMSCREEN);
    sh2o_gte_write(g,58,(uint32_t)h);
}

/* Contract tag psyq-initgeom-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_InitGeom(sh2o_gte_t *g) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_INITGEOM);
    sh2o_gte_write(g,61,0x155u);
    sh2o_gte_write(g,62,0x100u);
    sh2o_gte_write(g,58,0x3E8u);
    sh2o_gte_write(g,59,0xFFFFEF9Eu); /* -1062h */
    sh2o_gte_write(g,60,0x01400000u);
    sh2o_gte_write(g,56,0u);
    sh2o_gte_write(g,57,0u);
}

/* Contract tag psyq-setfognearfar-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_SetFogNearFar(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h) {
    int32_t d = (int32_t)((uint32_t)b - (uint32_t)a),q1,q2,q3;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_SETFOGNEARFAR);
    if(d < 100)
        return;
    if(!sh2oi_div((int32_t)((0u - (uint32_t)a) * (uint32_t)b),d,&q1) ||
        !sh2oi_div((int32_t)((uint32_t)b << 12),d,&q2) || !sh2oi_div((int32_t)((uint32_t)q1 << 8),h,&q3))
        return;
    if(q3 < -0x8000)
        q3 = -0x8000;
    if(q3 > 0x7FFF)
        q3 = 0x7FFF;
    sh2o_gte_write(g,59,(uint32_t)q3);       /* SetDQA */
    sh2o_gte_write(g,60,(uint32_t)q2 << 12); /* SetDQB */
}

/* Contract tag psyq-transmatrix-1. Contract in sh2o/psyq.h. */
sh2o_psyq_matrix_t *sh2o_psyq_TransMatrix(sh2o_gte_t *g,sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v) {
    int32_t x = v->vx,y = v->vy,z = v->vz;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_TRANSMATRIX);
    (void)g;
    m->t[0] = x;
    m->t[1] = y;
    m->t[2] = z;
    return m;
}

/* Contract tag psyq-transposematrix-1. Contract in sh2o/psyq.h. */
sh2o_psyq_matrix_t *sh2o_psyq_TransposeMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1) {
    uint32_t r9,r10,r11;
    int16_t h10;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_TRANSPOSEMATRIX);
    (void)g;
    r9 = SH2OI_W(m0->m[0][0],m0->m[0][1]);
    r10 = SH2OI_W(m0->m[0][2],m0->m[1][0]);
    m1->m[0][2] = SH2OI_LO(r9),m1->m[1][0] = SH2OI_HI(r9);   /* sw r9, 4 */
    m1->m[0][0] = SH2OI_LO(r10),m1->m[0][1] = SH2OI_HI(r10); /* sw r10, 0 */
    m1->m[0][0] = SH2OI_LO(r9);                              /* sh r9, 0 */
    r11 = SH2OI_W(m0->m[1][1],m0->m[1][2]);
    r9 = SH2OI_W(m0->m[2][0],m0->m[2][1]);
    m1->m[2][0] = SH2OI_LO(r11),m1->m[2][1] = SH2OI_HI(r11); /* sw r11, 0xC */
    m1->m[1][1] = SH2OI_LO(r9),m1->m[1][2] = SH2OI_HI(r9);   /* sw r9, 8 */
    m1->m[2][0] = SH2OI_LO(r10);                             /* sh r10, 0xC */
    m1->m[1][1] = SH2OI_LO(r11);                             /* sh r11, 8 */
    h10 = m0->m[2][2];
    m1->m[0][2] = SH2OI_LO(r9); /* sh r9, 4 */
    m1->m[2][2] = h10;          /* sh r10, 0x10 */
    return m1;
}

/* Contract tag psyq-compmatrix-1. Contract in sh2o/psyq.h. */
sh2o_psyq_matrix_t *sh2o_psyq_CompMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,const sh2o_psyq_matrix_t *m1,
                                            sh2o_psyq_matrix_t *m2) {
    const uint32_t cmd = 0x0486012u; /* MVMVA sf 1, RT, V0, no translation */
    uint32_t a1,a2,a3,b1,b2,b3,c1,c2,c3,v0,vz,t0,t1,t2;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_COMPMATRIX);
    SH2OI_COUNT_IN();
    sh2oi_set3(g,32,m0);
    v0 = SH2OI_W(m1->m[0][0],m1->m[1][0]);
    vz = SH2OI_W(m1->m[2][0],m1->m[2][1]);
    sh2o_gte_write(g,0,v0);
    sh2o_gte_write(g,1,vz);
    (void)sh2o_gte_command(g,cmd);
    v0 = SH2OI_W(m1->m[0][1],m1->m[1][1]);
    vz = (uint32_t)(int32_t)m1->m[2][1];
    a1 = sh2o_gte_read(g,9),a2 = sh2o_gte_read(g,10),a3 = sh2o_gte_read(g,11);
    sh2o_gte_write(g,0,v0);
    sh2o_gte_write(g,1,vz);
    (void)sh2o_gte_command(g,cmd);
    v0 = SH2OI_W(m1->m[0][2],m1->m[1][2]);
    vz = SH2OI_W(m1->m[2][2],m1->pad);
    b1 = sh2o_gte_read(g,9),b2 = sh2o_gte_read(g,10),b3 = sh2o_gte_read(g,11);
    sh2o_gte_write(g,0,v0);
    sh2o_gte_write(g,1,vz);
    (void)sh2o_gte_command(g,cmd);
    m2->m[0][0] = SH2OI_LO(a1),m2->m[0][1] = SH2OI_LO(b1); /* sw 0 */
    m2->m[2][0] = SH2OI_LO(a3),m2->m[2][1] = SH2OI_LO(b3); /* sw 0xC */
    c1 = sh2o_gte_read(g,9),c2 = sh2o_gte_read(g,10);
    c3 = sh2o_gte_read(g,11);
    m2->m[2][2] = SH2OI_LO(c3),m2->pad = SH2OI_HI(c3); /* swc2 IR3, 0x10 */
    v0 = SH2OI_W(m1->t[0],m1->t[1]);                   /* lhu 0x14 | lw 0x18 << 16 */
    vz = (uint32_t)m1->t[2];
    sh2o_gte_write(g,0,v0);
    sh2o_gte_write(g,1,vz);
    (void)sh2o_gte_command(g,cmd);
    m2->m[0][2] = SH2OI_LO(c1),m2->m[1][0] = SH2OI_LO(a2); /* sw 4 */
    m2->m[1][1] = SH2OI_LO(b2),m2->m[1][2] = SH2OI_LO(c2); /* sw 8 */
    t0 = sh2o_gte_read(g,25),t1 = sh2o_gte_read(g,26),t2 = sh2o_gte_read(g,27);
    t0 += (uint32_t)m0->t[0],t1 += (uint32_t)m0->t[1],t2 += (uint32_t)m0->t[2]; /* add (traps on overflow) */
    m2->t[0] = (int32_t)t0,m2->t[1] = (int32_t)t1,m2->t[2] = (int32_t)t2;
    SH2OI_COUNT_OUT();
    return m2;
}

/* Contract tag psyq-applymatrix-1. Contract in sh2o/psyq.h. */
sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_svector_t *v0,
                                            sh2o_psyq_vector_t *v1) {
    int32_t x,y,z;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_APPLYMATRIX);
    SH2OI_COUNT_IN();
    sh2oi_rt(g,m);
    g->r[0] = SH2OI_W(v0->vx,v0->vy);     /* VXY0 */
    g->r[1] = SH2OI_W(v0->vz,v0->pad);    /* VZ0 (pad in the high half) */
    SH2OI_COUNT_STORE(SH2O_COUNT_WRITE_OTHER,2);
    (void)sh2o_gte_command(g,0x0486012u); /* MVMVA sf 1, RT, V0, no translation */
    x = (int32_t)g->r[25],y = (int32_t)g->r[26],z = (int32_t)g->r[27];
    v1->vx = x;
    v1->vy = y;
    v1->vz = z;
    SH2OI_COUNT_OUT();
    return v1;
}

/* ApplyMatrixLV's split of one component: hi = x SAR 15 and lo = x & 7FFFh, both of |x| with x's sign */
static void sh2oi_split(int32_t x,uint32_t *hi,uint32_t *lo) {
    if(x < 0) {
        uint32_t n = 0u - (uint32_t)x;
        *hi = 0u - (uint32_t)sh2oi_sra((int32_t)n,15);
        *lo = 0u - (n & 0x7FFFu);
    } else {
        *hi = (uint32_t)sh2oi_sra(x,15);
        *lo = (uint32_t)x & 0x7FFFu;
    }
}

/* ApplyMatrixLV's x8 of a MAC: negu, sll 3, negu for a negative word, else sll 3 */
static uint32_t sh2oi_x8(uint32_t v) {
    return (int32_t)v < 0 ? 0u - ((0u - v) << 3) : v << 3;
}

/* Contract tag psyq-applymatrixlv-1. Contract in sh2o/psyq.h. */
sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrixLV(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v0,
                                            sh2o_psyq_vector_t *v1) {
    uint32_t hx,hy,hz,lx,ly,lz,a1,a2,a3,b1,b2,b3;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_APPLYMATRIXLV);
    SH2OI_COUNT_IN();
    sh2oi_set3(g,32,m);
    sh2oi_split(v0->vx,&hx,&lx);
    sh2oi_split(v0->vy,&hy,&ly);
    sh2oi_split(v0->vz,&hz,&lz);
    sh2o_gte_write(g,9,hx);
    sh2o_gte_write(g,10,hy);
    sh2o_gte_write(g,11,hz);
    (void)sh2o_gte_command(g,0x041E012u); /* MVMVA sf 0, RT, IR, no translation */
    a1 = sh2o_gte_read(g,25),a2 = sh2o_gte_read(g,26),a3 = sh2o_gte_read(g,27);
    sh2o_gte_write(g,9,lx);
    sh2o_gte_write(g,10,ly);
    sh2o_gte_write(g,11,lz);
    (void)sh2o_gte_command(g,0x049E012u); /* MVMVA sf 1, RT, IR, no translation */
    a1 = sh2oi_x8(a1),a2 = sh2oi_x8(a2),a3 = sh2oi_x8(a3);
    b1 = sh2o_gte_read(g,25),b2 = sh2o_gte_read(g,26),b3 = sh2o_gte_read(g,27);
    v1->vx = (int32_t)(b1 + a1);
    v1->vy = (int32_t)(b2 + a2);
    v1->vz = (int32_t)(b3 + a3);
    SH2OI_COUNT_OUT();
    return v1;
}

/* Contract tag psyq-rottrans-1. Contract in sh2o/psyq.h. */
void sh2o_psyq_RotTrans(sh2o_gte_t *g,const sh2o_psyq_svector_t *v0,sh2o_psyq_vector_t *v1,int32_t *flag) {
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_ROTTRANS);
    SH2OI_COUNT_IN();
    sh2o_gte_write(g,0,SH2OI_W(v0->vx,v0->vy));
    sh2o_gte_write(g,1,SH2OI_W(v0->vz,v0->pad));
    (void)sh2o_gte_command(g,0x0480012u); /* MVMVA sf 1, RT, V0, TR */
    v1->vx = (int32_t)sh2o_gte_read(g,25);
    v1->vy = (int32_t)sh2o_gte_read(g,26);
    v1->vz = (int32_t)sh2o_gte_read(g,27);
    *flag = (int32_t)sh2o_gte_read(g,63);
    SH2OI_COUNT_OUT();
}

/* Contract tag psyq-pushmatrix-1. Contract in sh2o/psyq.h. */
int sh2o_psyq_PushMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s) {
    int32_t top = s->top;
    uint32_t *e;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_PUSHMATRIX);
    if(top >= 0x280)
        return -1;
    if(top < 0 || (top & 31))
        return -2;
    e = &s->w[(uint32_t)top >> 2];
    e[0] = g->r[32];
    e[1] = g->r[33];
    e[2] = g->r[34];
    e[3] = g->r[35];
    e[4] = (uint32_t)(int32_t)(int16_t)(uint16_t)g->r[36];
    e[5] = g->r[37];
    e[6] = g->r[38];
    e[7] = g->r[39];
    s->top = top + 32;
    return 0;
}

/* Contract tag psyq-popmatrix-1. Contract in sh2o/psyq.h. */
int sh2o_psyq_PopMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s) {
    int32_t top = s->top;
    const uint32_t *e;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_POPMATRIX);
    if(top <= 0)
        return -1;
    if(top > 0x280 || (top & 31))
        return -2;
    top -= 32;
    s->top = top;
    e = &s->w[(uint32_t)top >> 2];
    g->r[32] = e[0];
    g->r[33] = e[1];
    g->r[34] = e[2];
    g->r[35] = e[3];
    g->r[36] = e[4];
    g->r[37] = e[5];
    g->r[38] = e[6];
    g->r[39] = e[7];
    SH2OI_COUNT_STORE(SH2O_COUNT_WRITE_RT,5);
    SH2OI_COUNT_STORE(SH2O_COUNT_WRITE_TR,3);
    return 0;
}

/* ratan2's table, i 0..1024: floor(atan((i + 3/8) / 1024) * 2048 / pi + 1/2), equal to the object's table */
/* clang-format off */
static const int16_t sh2oi_ratan_tbl[1025] SH2OI_RODATA(sh2oi_ratan_tbl) = {
    0,1,2,2,3,3,4,5,5,6,7,7,8,9,9,10,
    10,11,12,12,13,14,14,15,16,16,17,17,18,19,19,20,
    21,21,22,23,23,24,24,25,26,26,27,28,28,29,30,30,
    31,31,32,33,33,34,35,35,36,36,37,38,38,39,40,40,
    41,42,42,43,43,44,45,45,46,47,47,48,49,49,50,50,
    51,52,52,53,54,54,55,55,56,57,57,58,59,59,60,61,
    61,62,62,63,64,64,65,66,66,67,67,68,69,69,70,71,
    71,72,73,73,74,74,75,76,76,77,78,78,79,79,80,81,
    81,82,83,83,84,84,85,86,86,87,88,88,89,89,90,91,
    91,92,93,93,94,94,95,96,96,97,98,98,99,99,100,101,
    101,102,103,103,104,104,105,106,106,107,107,108,109,109,110,111,
    111,112,112,113,114,114,115,116,116,117,117,118,119,119,120,120,
    121,122,122,123,124,124,125,125,126,127,127,128,128,129,130,130,
    131,131,132,133,133,134,135,135,136,136,137,138,138,139,139,140,
    141,141,142,142,143,144,144,145,145,146,147,147,148,148,149,150,
    150,151,152,152,153,153,154,155,155,156,156,157,158,158,159,159,
    160,161,161,162,162,163,164,164,165,165,166,166,167,168,168,169,
    169,170,171,171,172,172,173,174,174,175,175,176,177,177,178,178,
    179,180,180,181,181,182,182,183,184,184,185,185,186,187,187,188,
    188,189,190,190,191,191,192,192,193,194,194,195,195,196,197,197,
    198,198,199,199,200,201,201,202,202,203,203,204,205,205,206,206,
    207,207,208,209,209,210,210,211,211,212,213,213,214,214,215,215,
    216,217,217,218,218,219,219,220,221,221,222,222,223,223,224,225,
    225,226,226,227,227,228,228,229,230,230,231,231,232,232,233,234,
    234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
    243,244,244,245,245,246,246,247,247,248,248,249,250,250,251,251,
    252,252,253,253,254,254,255,256,256,257,257,258,258,259,259,260,
    260,261,262,262,263,263,264,264,265,265,266,266,267,267,268,269,
    269,270,270,271,271,272,272,273,273,274,274,275,275,276,276,277,
    278,278,279,279,280,280,281,281,282,282,283,283,284,284,285,285,
    286,286,287,288,288,289,289,290,290,291,291,292,292,293,293,294,
    294,295,295,296,296,297,297,298,298,299,299,300,300,301,301,302,
    302,303,303,304,304,305,305,306,307,307,308,308,309,309,310,310,
    311,311,312,312,313,313,314,314,315,315,316,316,317,317,318,318,
    319,319,320,320,321,321,322,322,322,323,323,324,324,325,325,326,
    326,327,327,328,328,329,329,330,330,331,331,332,332,333,333,334,
    334,335,335,336,336,337,337,338,338,339,339,340,340,340,341,341,
    342,342,343,343,344,344,345,345,346,346,347,347,348,348,349,349,
    349,350,350,351,351,352,352,353,353,354,354,355,355,356,356,356,
    357,357,358,358,359,359,360,360,361,361,362,362,362,363,363,364,
    364,365,365,366,366,367,367,368,368,368,369,369,370,370,371,371,
    372,372,372,373,373,374,374,375,375,376,376,377,377,377,378,378,
    379,379,380,380,381,381,381,382,382,383,383,384,384,385,385,385,
    386,386,387,387,388,388,388,389,389,390,390,391,391,391,392,392,
    393,393,394,394,395,395,395,396,396,397,397,398,398,398,399,399,
    400,400,401,401,401,402,402,403,403,403,404,404,405,405,406,406,
    406,407,407,408,408,409,409,409,410,410,411,411,411,412,412,413,
    413,413,414,414,415,415,416,416,416,417,417,418,418,418,419,419,
    420,420,420,421,421,422,422,422,423,423,424,424,425,425,425,426,
    426,427,427,427,428,428,429,429,429,430,430,431,431,431,432,432,
    432,433,433,434,434,434,435,435,436,436,436,437,437,438,438,438,
    439,439,440,440,440,441,441,441,442,442,443,443,443,444,444,445,
    445,445,446,446,446,447,447,448,448,448,449,449,450,450,450,451,
    451,451,452,452,453,453,453,454,454,454,455,455,456,456,456,457,
    457,457,458,458,459,459,459,460,460,460,461,461,461,462,462,463,
    463,463,464,464,464,465,465,465,466,466,467,467,467,468,468,468,
    469,469,469,470,470,471,471,471,472,472,472,473,473,473,474,474,
    474,475,475,476,476,476,477,477,477,478,478,478,479,479,479,480,
    480,480,481,481,481,482,482,483,483,483,484,484,484,485,485,485,
    486,486,486,487,487,487,488,488,488,489,489,489,490,490,490,491,
    491,491,492,492,492,493,493,493,494,494,494,495,495,495,496,496,
    496,497,497,497,498,498,498,499,499,499,500,500,500,501,501,501,
    502,502,502,503,503,503,504,504,504,505,505,505,506,506,506,507,
    507,507,508,508,508,509,509,509,510,510,510,511,511,511,511,512,
    512
};
/* clang-format on */

/* the halfword at ratan_tbl + 2q, q 0..1024 (every q a checked divide gives; 0 outside, never reached) */
static int32_t sh2oi_ratan_at(int32_t q) {
    return (uint32_t)q <= 1024u ? sh2oi_ratan_tbl[q] : 0;
}

/* Contract tag psyq-ratan2-1. Contract in sh2o/psyq.h. */
int32_t sh2o_psyq_ratan2(sh2o_gte_t *g,int32_t y,int32_t x) {
    int xneg = 0,yneg = 0;
    int32_t q,t;
    SH2OI_COUNT_NAME(SH2O_COUNT_PSYQ_RATAN2);
    (void)g;
    if(x < 0) {
        xneg = 1;
        x = (int32_t)(0u - (uint32_t)x);
    }
    if(y < 0) {
        yneg = 1;
        y = (int32_t)(0u - (uint32_t)y);
    }
    if(x == 0 && y == 0)
        return 0;
    if(y < x) {
        if(!((uint32_t)y & 0x7FE00000u ? sh2oi_div(y,sh2oi_sra(x,10),&q) :
                sh2oi_div((int32_t)((uint32_t)y << 10),x,&q)))
            return 0;
        t = sh2oi_ratan_at(q);
    } else {
        if(!((uint32_t)x & 0x7FE00000u ? sh2oi_div(x,sh2oi_sra(y,10),&q) :
                sh2oi_div((int32_t)((uint32_t)x << 10),y,&q)))
            return 0;
        t = 0x400 - sh2oi_ratan_at(q);
    }
    if(xneg)
        t = 0x800 - t;
    if(yneg)
        t = -t;
    return t;
}
