/* psyz libgte test shim over the sh2o GTE image.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */

/* tests/psyz_libgte_check.py builds this file with the psyz libgte test and the
 * ztest runner, both compiled in place and unchanged (-D__psyz, psyz headers
 * with -isystem), and every src/ C file. No psyz text is in this file.
 * One static GTE image is the state of all names below.
 * (1) The PSY-Q names the sh2o library has: each calls its sh2o_psyq_ body.
 *     The psyz types are converted field by field to the sh2o types and back.
 * (2) The psyz GTE endpoints: register reads and writes go to sh2o_gte_read and
 *     sh2o_gte_write (control register r is register 32 + r). Each other
 *     endpoint does the register and command steps of its PSY-Q libgte.h
 *     macro (the PS1 branch), in the same order. A PS1 store at an offset of a
 *     POLY_ packet goes to the host field at that PS1 offset (the host packet
 *     has a longer tag). A command that sh2o does not model skips the case.
 * (3) Each other name the test calls skips the case: sh2o does not have it. */
#include <stdint.h>

#include <psyz.h>
#include <libgte.h>
#include <libgpu.h>
#include <psyz/gte.h>
#include <ztest.h>

#include "sh2o/gte.h"
#include "sh2o/psyq.h"

static sh2o_gte_t shim_gte;
static sh2o_psyq_mstack_t shim_stack;

static uint32_t shim_rd(unsigned reg) {
    return sh2o_gte_read(&shim_gte,reg);
}

static void shim_wr(unsigned reg,uint32_t v) {
    sh2o_gte_write(&shim_gte,reg,v);
}

/* One COP2 command word; name is the command for the skip message. */
static void shim_cop2(uint32_t word,const char *name) {
    if(sh2o_gte_command(&shim_gte,word&0x1FFFFFFu)<0)
        ztest__skip(__LINE__,"sh2o lacks %s",name);
}

static uint32_t shim_pair(int lo,int hi) {
    return (uint32_t)(uint16_t)lo|(uint32_t)(uint16_t)hi<<16;
}

static uint32_t shim_cword(const CVECTOR *c) {
    return (uint32_t)c->r|(uint32_t)c->g<<8|(uint32_t)c->b<<16|(uint32_t)c->cd<<24;
}

static void shim_cstore(CVECTOR *c,uint32_t w) {
    c->r=(u_char)w;
    c->g=(u_char)(w>>8);
    c->b=(u_char)(w>>16);
    c->cd=(u_char)(w>>24);
}

static void shim_xy(short *x,short *y,uint32_t w) {
    *x=(short)(uint16_t)w;
    *y=(short)(uint16_t)(w>>16);
}

static void shim_to_matrix(sh2o_psyq_matrix_t *s,const MATRIX *m) {
    int i,j;

    for(i=0;i<3;i++) {
        for(j=0;j<3;j++)
            s->m[i][j]=(int16_t)m->m[i][j];
        s->t[i]=(int32_t)m->t[i];
    }
    s->pad=0;
}

static void shim_from_matrix(MATRIX *m,const sh2o_psyq_matrix_t *s) {
    int i,j;

    for(i=0;i<3;i++) {
        for(j=0;j<3;j++)
            m->m[i][j]=(short)s->m[i][j];
        m->t[i]=(int)s->t[i];
    }
}

static void shim_to_vector(sh2o_psyq_vector_t *s,const VECTOR *v) {
    s->vx=(int32_t)v->vx;
    s->vy=(int32_t)v->vy;
    s->vz=(int32_t)v->vz;
    s->pad=(int32_t)v->pad;
}

static void shim_from_vector(VECTOR *v,const sh2o_psyq_vector_t *s) {
    v->vx=(int)s->vx;
    v->vy=(int)s->vy;
    v->vz=(int)s->vz;
    v->pad=(int)s->pad;
}

static void shim_to_svector(sh2o_psyq_svector_t *s,const SVECTOR *v) {
    s->vx=(int16_t)v->vx;
    s->vy=(int16_t)v->vy;
    s->vz=(int16_t)v->vz;
    s->pad=(int16_t)v->pad;
}

/* (1) The PSY-Q names of sh2o/psyq.h. */
void InitGeom(void) {
    sh2o_psyq_InitGeom(&shim_gte);
}

void SetRotMatrix(MATRIX *m) {
    sh2o_psyq_matrix_t s;

    shim_to_matrix(&s,m);
    sh2o_psyq_SetRotMatrix(&shim_gte,&s);
}

void SetLightMatrix(MATRIX *m) {
    sh2o_psyq_matrix_t s;

    shim_to_matrix(&s,m);
    sh2o_psyq_SetLightMatrix(&shim_gte,&s);
}

void SetColorMatrix(MATRIX *m) {
    sh2o_psyq_matrix_t s;

    shim_to_matrix(&s,m);
    sh2o_psyq_SetColorMatrix(&shim_gte,&s);
}

void SetTransMatrix(MATRIX *m) {
    sh2o_psyq_matrix_t s;

    shim_to_matrix(&s,m);
    sh2o_psyq_SetTransMatrix(&shim_gte,&s);
}

void SetBackColor(long rbk,long gbk,long bbk) {
    sh2o_psyq_SetBackColor(&shim_gte,(int32_t)rbk,(int32_t)gbk,(int32_t)bbk);
}

void SetFarColor(long rfc,long gfc,long bfc) {
    sh2o_psyq_SetFarColor(&shim_gte,(int32_t)rfc,(int32_t)gfc,(int32_t)bfc);
}

void SetGeomOffset(long ofx,long ofy) {
    sh2o_psyq_SetGeomOffset(&shim_gte,(int32_t)ofx,(int32_t)ofy);
}

void SetGeomScreen(long h) {
    sh2o_psyq_SetGeomScreen(&shim_gte,(int32_t)h);
}

void SetFogNearFar(long a,long b,long h) {
    sh2o_psyq_SetFogNearFar(&shim_gte,(int32_t)a,(int32_t)b,(int32_t)h);
}

MATRIX *TransMatrix(MATRIX *m,VECTOR *v) {
    sh2o_psyq_matrix_t s;
    sh2o_psyq_vector_t t;

    shim_to_matrix(&s,m);
    shim_to_vector(&t,v);
    sh2o_psyq_TransMatrix(&shim_gte,&s,&t);
    shim_from_matrix(m,&s);
    return m;
}

MATRIX *TransposeMatrix(MATRIX *m0,MATRIX *m1) {
    sh2o_psyq_matrix_t s0,s1;

    shim_to_matrix(&s0,m0);
    shim_to_matrix(&s1,m1);
    sh2o_psyq_TransposeMatrix(&shim_gte,&s0,&s1);
    shim_from_matrix(m1,&s1);
    return m1;
}

MATRIX *CompMatrix(MATRIX *m0,MATRIX *m1,MATRIX *m2) {
    sh2o_psyq_matrix_t s0,s1,s2;

    shim_to_matrix(&s0,m0);
    shim_to_matrix(&s1,m1);
    shim_to_matrix(&s2,m2);
    sh2o_psyq_CompMatrix(&shim_gte,&s0,&s1,&s2);
    shim_from_matrix(m2,&s2);
    return m2;
}

void ApplyMatrix(MATRIX *m,SVECTOR *v0,VECTOR *v1) {
    sh2o_psyq_matrix_t s;
    sh2o_psyq_svector_t a;
    sh2o_psyq_vector_t b;

    shim_to_matrix(&s,m);
    shim_to_svector(&a,v0);
    shim_to_vector(&b,v1);
    sh2o_psyq_ApplyMatrix(&shim_gte,&s,&a,&b);
    shim_from_vector(v1,&b);
}

VECTOR *ApplyMatrixLV(MATRIX *m,VECTOR *v0,VECTOR *v1) {
    sh2o_psyq_matrix_t s;
    sh2o_psyq_vector_t a,b;

    shim_to_matrix(&s,m);
    shim_to_vector(&a,v0);
    shim_to_vector(&b,v1);
    sh2o_psyq_ApplyMatrixLV(&shim_gte,&s,&a,&b);
    shim_from_vector(v1,&b);
    return v1;
}

void RotTrans(SVECTOR *v0,VECTOR *v1,int *flag) {
    sh2o_psyq_svector_t a;
    sh2o_psyq_vector_t b;
    int32_t f=0;

    shim_to_svector(&a,v0);
    shim_to_vector(&b,v1);
    sh2o_psyq_RotTrans(&shim_gte,&a,&b,&f);
    shim_from_vector(v1,&b);
    *flag=(int)f;
}

void PushMatrix(void) {
    (void)sh2o_psyq_PushMatrix(&shim_gte,&shim_stack);
}

void PopMatrix(void) {
    (void)sh2o_psyq_PopMatrix(&shim_gte,&shim_stack);
}

long ratan2(long y,long x) {
    return (long)sh2o_psyq_ratan2(&shim_gte,(int32_t)y,(int32_t)x);
}

/* (2) The psyz GTE endpoints. */
unsigned int Psyz_GteDataRead(unsigned reg) {
    return shim_rd(reg&31u);
}

void Psyz_GteDataWrite(unsigned reg,unsigned int value) {
    shim_wr(reg&31u,value);
}

unsigned int Psyz_GteCtrlRead(unsigned reg) {
    return shim_rd(32u+(reg&31u));
}

void Psyz_GteCtrlWrite(unsigned reg,unsigned int value) {
    shim_wr(32u+(reg&31u),value);
}

void Psyz_GteCommand(unsigned int cmd) {
    if(sh2o_gte_command(&shim_gte,cmd&0x1FFFFFFu)<0)
        ztest__skip(__LINE__,"sh2o lacks GTE op %02Xh",cmd&0x3Fu);
}

/* Loads. */
void Psyz_GteLdRgb(CVECTOR *v) {
    shim_wr(6,shim_cword(v));
}

void Psyz_GteLdRgb3(CVECTOR *v0,CVECTOR *v1,CVECTOR *v2) {
    shim_wr(20,shim_cword(v0));
    shim_wr(21,shim_cword(v1));
    shim_wr(22,shim_cword(v2));
    shim_wr(6,shim_cword(v2));
}

void Psyz_GteLdRgb3c(CVECTOR *v) {
    shim_wr(20,shim_cword(&v[0]));
    shim_wr(21,shim_cword(&v[1]));
    shim_wr(22,shim_cword(&v[2]));
    shim_wr(6,shim_cword(&v[2]));
}

void Psyz_GteLdDp(long p) {
    shim_wr(8,(uint32_t)p);
}

void Psyz_GteLdSz3(long sz1,long sz2,long sz3) {
    shim_wr(17,(uint32_t)sz1);
    shim_wr(18,(uint32_t)sz2);
    shim_wr(19,(uint32_t)sz3);
}

void Psyz_GteLdSz4(long sz0,long sz1,long sz2,long sz3) {
    shim_wr(16,(uint32_t)sz0);
    shim_wr(17,(uint32_t)sz1);
    shim_wr(18,(uint32_t)sz2);
    shim_wr(19,(uint32_t)sz3);
}

void Psyz_GteLdv0(SVECTOR *v) {
    shim_wr(0,shim_pair(v->vx,v->vy));
    shim_wr(1,shim_pair(v->vz,v->pad));
}

void Psyz_GteLdv3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2) {
    shim_wr(0,shim_pair(v0->vx,v0->vy));
    shim_wr(1,shim_pair(v0->vz,v0->pad));
    shim_wr(2,shim_pair(v1->vx,v1->vy));
    shim_wr(3,shim_pair(v1->vz,v1->pad));
    shim_wr(4,shim_pair(v2->vx,v2->vy));
    shim_wr(5,shim_pair(v2->vz,v2->pad));
}

void Psyz_GteLdlv0(VECTOR *v) {
    shim_wr(0,shim_pair(v->vx,v->vy));
    shim_wr(1,(uint32_t)v->vz);
}

void Psyz_GteLdopv1(VECTOR *v) {
    shim_wr(32,(uint32_t)v->vx);
    shim_wr(34,(uint32_t)v->vy);
    shim_wr(36,(uint32_t)v->vz);
}

void Psyz_GteLdopv2(VECTOR *v) {
    shim_wr(11,(uint32_t)v->vz);
    shim_wr(9,(uint32_t)v->vx);
    shim_wr(10,(uint32_t)v->vy);
}

void Psyz_GteLdbv(unsigned char *v) {
    shim_wr(9,v[0]);
    shim_wr(10,v[1]);
}

void Psyz_GteLdcv(unsigned char *v) {
    shim_wr(9,v[0]);
    shim_wr(10,v[1]);
    shim_wr(11,v[2]);
}

void Psyz_GteLdfc(VECTOR *v) {
    shim_wr(53,(uint32_t)v->vx);
    shim_wr(54,(uint32_t)v->vy);
    shim_wr(55,(uint32_t)v->vz);
}

void Psyz_GteLdsxy0(unsigned int sxy) {
    shim_wr(12,sxy);
}

void Psyz_GteLdsxy1(unsigned int sxy) {
    shim_wr(13,sxy);
}

void Psyz_GteLdsxy2(unsigned int sxy) {
    shim_wr(14,sxy);
}

void Psyz_GteLdsxy3(unsigned int sxy0,unsigned int sxy1,unsigned int sxy2) {
    shim_wr(12,sxy0);
    shim_wr(14,sxy2);
    shim_wr(13,sxy1);
}

void Psyz_GteLdsxy3c(unsigned int *sxy) {
    shim_wr(12,sxy[0]);
    shim_wr(13,sxy[1]);
    shim_wr(14,sxy[2]);
}

void Psyz_GteLdsvrtrow0(SVECTOR *v) {
    shim_wr(32,shim_pair(v->vx,v->vy));
    shim_wr(33,shim_pair(v->vz,v->pad));
}

void Psyz_GteLdsvllrow0(SVECTOR *v) {
    shim_wr(40,shim_pair(v->vx,v->vy));
    shim_wr(41,shim_pair(v->vz,v->pad));
}

void Psyz_GteLdsvlcrow0(SVECTOR *v) {
    shim_wr(48,shim_pair(v->vx,v->vy));
    shim_wr(49,shim_pair(v->vz,v->pad));
}

/* Stores. */
void Psyz_GteStRgb(CVECTOR *v) {
    shim_cstore(v,shim_rd(22));
}

void Psyz_GteStRgb3(CVECTOR *v0,CVECTOR *v1,CVECTOR *v2) {
    shim_cstore(v0,shim_rd(20));
    shim_cstore(v1,shim_rd(21));
    shim_cstore(v2,shim_rd(22));
}

/* PS1 offsets 4, 12, 20: the colour words 0, 1, 2. */
void Psyz_GteStRgb3G3(void *polyG3) {
    POLY_G3 *p=polyG3;
    uint32_t w;

    w=shim_rd(20);
    p->r0=(u_char)w;
    p->g0=(u_char)(w>>8);
    p->b0=(u_char)(w>>16);
    p->code=(u_char)(w>>24);
    w=shim_rd(21);
    p->r1=(u_char)w;
    p->g1=(u_char)(w>>8);
    p->b1=(u_char)(w>>16);
    p->pad1=(u_char)(w>>24);
    w=shim_rd(22);
    p->r2=(u_char)w;
    p->g2=(u_char)(w>>8);
    p->b2=(u_char)(w>>16);
    p->pad2=(u_char)(w>>24);
}

/* PS1 offsets 4, 12, 20: the colour words 0, 1, 2. */
void Psyz_GteStRgb3G4(void *polyG4) {
    POLY_G4 *p=polyG4;
    uint32_t w;

    w=shim_rd(20);
    p->r0=(u_char)w;
    p->g0=(u_char)(w>>8);
    p->b0=(u_char)(w>>16);
    p->code=(u_char)(w>>24);
    w=shim_rd(21);
    p->r1=(u_char)w;
    p->g1=(u_char)(w>>8);
    p->b1=(u_char)(w>>16);
    p->pad1=(u_char)(w>>24);
    w=shim_rd(22);
    p->r2=(u_char)w;
    p->g2=(u_char)(w>>8);
    p->b2=(u_char)(w>>16);
    p->pad2=(u_char)(w>>24);
}

/* PS1 offsets 4, 16, 28: the colour words 0, 1, 2. */
void Psyz_GteStRgb3Gt3(void *polyGt3) {
    POLY_GT3 *p=polyGt3;
    uint32_t w;

    w=shim_rd(20);
    p->r0=(u_char)w;
    p->g0=(u_char)(w>>8);
    p->b0=(u_char)(w>>16);
    p->code=(u_char)(w>>24);
    w=shim_rd(21);
    p->r1=(u_char)w;
    p->g1=(u_char)(w>>8);
    p->b1=(u_char)(w>>16);
    p->p1=(u_char)(w>>24);
    w=shim_rd(22);
    p->r2=(u_char)w;
    p->g2=(u_char)(w>>8);
    p->b2=(u_char)(w>>16);
    p->p2=(u_char)(w>>24);
}

/* PS1 offsets 4, 16, 28: the colour words 0, 1, 2. */
void Psyz_GteStRgb3Gt4(void *polyGt4) {
    POLY_GT4 *p=polyGt4;
    uint32_t w;

    w=shim_rd(20);
    p->r0=(u_char)w;
    p->g0=(u_char)(w>>8);
    p->b0=(u_char)(w>>16);
    p->code=(u_char)(w>>24);
    w=shim_rd(21);
    p->r1=(u_char)w;
    p->g1=(u_char)(w>>8);
    p->b1=(u_char)(w>>16);
    p->p1=(u_char)(w>>24);
    w=shim_rd(22);
    p->r2=(u_char)w;
    p->g2=(u_char)(w>>8);
    p->b2=(u_char)(w>>16);
    p->p2=(u_char)(w>>24);
}

void Psyz_GteStsxy(unsigned int *out) {
    *out=shim_rd(14);
}

void Psyz_GteStsxy0(unsigned int *out) {
    *out=shim_rd(12);
}

void Psyz_GteStsxy1(unsigned int *out) {
    *out=shim_rd(13);
}

void Psyz_GteStsxy2(unsigned int *out) {
    *out=shim_rd(14);
}

void Psyz_GteStsxy01(unsigned int *out0,unsigned int *out1) {
    *out0=shim_rd(12);
    *out1=shim_rd(13);
}

void Psyz_GteStsxy3(unsigned int *out0,unsigned int *out1,unsigned int *out2) {
    *out0=shim_rd(12);
    *out1=shim_rd(13);
    *out2=shim_rd(14);
}

void Psyz_GteStsxy3c(unsigned int *out) {
    out[0]=shim_rd(12);
    out[1]=shim_rd(13);
    out[2]=shim_rd(14);
}

/* PS1 offsets 8, 12, 16. */
void Psyz_GteStsxy3F3(void *polyF3) {
    POLY_F3 *p=polyF3;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 12, 16. */
void Psyz_GteStsxy3F4(void *polyF4) {
    POLY_F4 *p=polyF4;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 16, 24. */
void Psyz_GteStsxy3Ft3(void *polyFt3) {
    POLY_FT3 *p=polyFt3;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 16, 24. */
void Psyz_GteStsxy3Ft4(void *polyFt4) {
    POLY_FT4 *p=polyFt4;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 16, 24. */
void Psyz_GteStsxy3G3(void *polyG3) {
    POLY_G3 *p=polyG3;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 16, 24. */
void Psyz_GteStsxy3G4(void *polyG4) {
    POLY_G4 *p=polyG4;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

/* PS1 offsets 8, 20, 32. */
void Psyz_GteStsxy3Gt4(void *polyGt4) {
    POLY_GT4 *p=polyGt4;

    shim_xy(&p->x0,&p->y0,shim_rd(12));
    shim_xy(&p->x1,&p->y1,shim_rd(13));
    shim_xy(&p->x2,&p->y2,shim_rd(14));
}

void Psyz_GteStopz(int *out) {
    *out=(int)shim_rd(24);
}

void Psyz_GteStotz(unsigned int *out) {
    *out=shim_rd(7);
}

void Psyz_GteStlvnl(VECTOR *out) {
    out->vx=(int)shim_rd(25);
    out->vy=(int)shim_rd(26);
    out->vz=(int)shim_rd(27);
}

void Psyz_GteStlvl(VECTOR *out) {
    out->vx=(int)shim_rd(9);
    out->vy=(int)shim_rd(10);
    out->vz=(int)shim_rd(11);
}

void Psyz_GteStsv(SVECTOR *out) {
    out->vx=(short)(uint16_t)shim_rd(9);
    out->vy=(short)(uint16_t)shim_rd(10);
    out->vz=(short)(uint16_t)shim_rd(11);
}

void Psyz_GteSttr(VECTOR *out) {
    out->vx=(int)shim_rd(37);
    out->vy=(int)shim_rd(38);
    out->vz=(int)shim_rd(39);
}

void Psyz_GteStsz(unsigned int *out) {
    *out=shim_rd(19);
}

void Psyz_GteStsz3(unsigned int *out0,unsigned int *out1,unsigned int *out2) {
    *out0=shim_rd(17);
    *out1=shim_rd(18);
    *out2=shim_rd(19);
}

void Psyz_GteStsz4(unsigned int *out0,unsigned int *out1,unsigned int *out2,unsigned int *out3) {
    *out0=shim_rd(16);
    *out1=shim_rd(17);
    *out2=shim_rd(18);
    *out3=shim_rd(19);
}

void Psyz_GteStsz3c(unsigned int *out) {
    out[0]=shim_rd(17);
    out[1]=shim_rd(18);
    out[2]=shim_rd(19);
}

void Psyz_GteStsz4c(unsigned int *out) {
    out[0]=shim_rd(16);
    out[1]=shim_rd(17);
    out[2]=shim_rd(18);
    out[3]=shim_rd(19);
}

int Psyz_GteReadflg(void) {
    return (int)shim_rd(63);
}

/* Commands. */
void Psyz_GteRtps(void) {
    shim_cop2(0x4A180001u,"RTPS");
}

void Psyz_GteRtpt(void) {
    shim_cop2(0x4A280030u,"RTPT");
}

void Psyz_GteNclip(void) {
    shim_cop2(0x4B400006u,"NCLIP");
}

void Psyz_GteAvsz3(void) {
    shim_cop2(0x4B58002Du,"AVSZ3");
}

void Psyz_GteAvsz4(void) {
    shim_cop2(0x4B68002Eu,"AVSZ4");
}

void Psyz_GteDpcs(void) {
    shim_cop2(0x4A780010u,"DPCS");
}

void Psyz_GteNccs(void) {
    shim_cop2(0x4B08041Bu,"NCCS");
}

void Psyz_GteRt(void) {
    shim_cop2(0x4A480012u,"MVMVA");
}

void Psyz_GteRtir(void) {
    shim_cop2(0x4A49E012u,"MVMVA");
}

void Psyz_GteRtv0(void) {
    shim_cop2(0x4A486012u,"MVMVA");
}

void Psyz_GteRtv1(void) {
    shim_cop2(0x4A48E012u,"MVMVA");
}

void Psyz_GteRtv2(void) {
    shim_cop2(0x4A496012u,"MVMVA");
}

void Psyz_GteRtv0tr(void) {
    shim_cop2(0x4A480012u,"MVMVA");
}

void Psyz_GteRtv1tr(void) {
    shim_cop2(0x4A488012u,"MVMVA");
}

void Psyz_GteRtv2tr(void) {
    shim_cop2(0x4A490012u,"MVMVA");
}

void Psyz_GteRtv0bk(void) {
    shim_cop2(0x4A482012u,"MVMVA");
}

void Psyz_GteRtv1bk(void) {
    shim_cop2(0x4A48A012u,"MVMVA");
}

void Psyz_GteRtv2bk(void) {
    shim_cop2(0x4A492012u,"MVMVA");
}

void Psyz_GteOp12(void) {
    shim_cop2(0x4B78000Cu,"OP");
}

void Psyz_GteGpl0(void) {
    shim_cop2(0x4BA0003Eu,"GPL");
}

void Psyz_GteGpl12(void) {
    shim_cop2(0x4BA8003Eu,"GPL");
}

/* Macro sequences. */
static void shim_stflg(int *out) {
    *out=(int)shim_rd(63);
}

static void shim_stszotz(int *out) {
    *out=(int)shim_rd(19)>>2;
}

static void shim_stdp(int *out) {
    *out=(int)shim_rd(8);
}

static void shim_ldlvl(VECTOR *v) {
    shim_wr(9,(uint32_t)v->vx);
    shim_wr(10,(uint32_t)v->vy);
    shim_wr(11,(uint32_t)v->vz);
}

static void shim_ldopv1sv(SVECTOR *v) {
    shim_wr(32,(uint32_t)(int32_t)v->vx);
    shim_wr(34,(uint32_t)(int32_t)v->vy);
    shim_wr(36,(uint32_t)(int32_t)v->vz);
}

static void shim_ldopv2sv(SVECTOR *v) {
    shim_wr(9,(uint32_t)(int32_t)v->vx);
    shim_wr(10,(uint32_t)(int32_t)v->vy);
    shim_wr(11,(uint32_t)(int32_t)v->vz);
}

void Psyz_GteRotNclip3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,unsigned int *sxy0,unsigned int *sxy1,
    unsigned int *sxy2,int *p,int *otz,int *flag,int *opz) {
    Psyz_GteLdv3(v0,v1,v2);
    Psyz_GteRtpt();
    shim_stflg(flag);
    Psyz_GteNclip();
    Psyz_GteStopz(opz);
    Psyz_GteStsxy3(sxy0,sxy1,sxy2);
    shim_stdp(p);
    shim_stszotz(otz);
}

void Psyz_GteRotAverageNclip3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,unsigned int *sxy0,unsigned int *sxy1,
    unsigned int *sxy2,int *p,int *otz,int *flag,int *opz) {
    Psyz_GteLdv3(v0,v1,v2);
    Psyz_GteRtpt();
    shim_stflg(flag);
    Psyz_GteNclip();
    Psyz_GteStopz(opz);
    Psyz_GteStsxy3(sxy0,sxy1,sxy2);
    shim_stdp(p);
    Psyz_GteAvsz3();
    Psyz_GteStotz((unsigned int *)otz);
}

void Psyz_GteRotColorDpq(SVECTOR *v0,SVECTOR *n0,CVECTOR *rgb,unsigned int *sxy,CVECTOR *out,int *flag,int *otz) {
    Psyz_GteLdv0(v0);
    Psyz_GteRtps();
    Psyz_GteStsxy(sxy);
    shim_stflg(flag);
    Psyz_GteLdv0(n0);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4AE80413u,"NCDS");
    Psyz_GteStRgb(out);
    shim_stszotz(otz);
}

void Psyz_GteRotColorDpq3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,CVECTOR *rgb,
    unsigned int *sxy0,unsigned int *sxy1,unsigned int *sxy2,CVECTOR *out0,CVECTOR *out1,CVECTOR *out2,int *flag,
    int *otz) {
    Psyz_GteLdv3(v0,v1,v2);
    Psyz_GteRtpt();
    Psyz_GteStsxy3(sxy0,sxy1,sxy2);
    shim_stflg(flag);
    Psyz_GteLdv3(n0,n1,n2);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4AF80416u,"NCDT");
    Psyz_GteStRgb3(out0,out1,out2);
    shim_stszotz(otz);
}

void Psyz_GteRotAverageNclipColorDpq3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,
    CVECTOR *rgb,unsigned int *sxy0,unsigned int *sxy1,unsigned int *sxy2,CVECTOR *out0,CVECTOR *out1,
    CVECTOR *out2,int *otz,int *flag,int *opz) {
    Psyz_GteLdv3(v0,v1,v2);
    Psyz_GteRtpt();
    shim_stflg(flag);
    Psyz_GteNclip();
    Psyz_GteStopz(opz);
    Psyz_GteLdv3(n0,n1,n2);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4AF80416u,"NCDT");
    Psyz_GteStsxy3(sxy0,sxy1,sxy2);
    Psyz_GteStRgb3(out0,out1,out2);
    Psyz_GteAvsz3();
    Psyz_GteStotz((unsigned int *)otz);
}

void Psyz_GteRotAverageNclipColorCol3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,
    CVECTOR *rgb,unsigned int *sxy0,unsigned int *sxy1,unsigned int *sxy2,CVECTOR *out0,CVECTOR *out1,
    CVECTOR *out2,int *otz,int *flag,int *opz) {
    Psyz_GteLdv3(v0,v1,v2);
    Psyz_GteRtpt();
    shim_stflg(flag);
    Psyz_GteNclip();
    Psyz_GteStopz(opz);
    Psyz_GteLdv3(n0,n1,n2);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4B18043Fu,"NCCT");
    Psyz_GteStsxy3(sxy0,sxy1,sxy2);
    Psyz_GteStRgb3(out0,out1,out2);
    Psyz_GteAvsz3();
    Psyz_GteStotz((unsigned int *)otz);
}

void Psyz_GteNormalColor(SVECTOR *n0,CVECTOR *out) {
    Psyz_GteLdv0(n0);
    shim_cop2(0x4AC8041Eu,"NCS");
    Psyz_GteStRgb(out);
}

void Psyz_GteNormalColor3(SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,CVECTOR *out0,CVECTOR *out1,CVECTOR *out2) {
    Psyz_GteLdv3(n0,n1,n2);
    shim_cop2(0x4AD80420u,"NCT");
    Psyz_GteStRgb3(out0,out1,out2);
}

void Psyz_GteNormalColorDpq(SVECTOR *n0,CVECTOR *rgb,long p,CVECTOR *out) {
    Psyz_GteLdv0(n0);
    Psyz_GteLdRgb(rgb);
    Psyz_GteLdDp(p);
    shim_cop2(0x4AE80413u,"NCDS");
    Psyz_GteStRgb(out);
}

void Psyz_GteNormalColorDpq3(SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,CVECTOR *rgb,long p,CVECTOR *out0,
    CVECTOR *out1,CVECTOR *out2) {
    Psyz_GteLdv3(n0,n1,n2);
    Psyz_GteLdRgb(rgb);
    Psyz_GteLdDp(p);
    shim_cop2(0x4AF80416u,"NCDT");
    Psyz_GteStRgb3(out0,out1,out2);
}

void Psyz_GteNormalColorCol(SVECTOR *n0,CVECTOR *rgb,CVECTOR *out) {
    Psyz_GteLdv0(n0);
    Psyz_GteLdRgb(rgb);
    Psyz_GteNccs();
    Psyz_GteStRgb(out);
}

void Psyz_GteNormalColorCol3(SVECTOR *n0,SVECTOR *n1,SVECTOR *n2,CVECTOR *rgb,CVECTOR *out0,CVECTOR *out1,
    CVECTOR *out2) {
    Psyz_GteLdv3(n0,n1,n2);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4B18043Fu,"NCCT");
    Psyz_GteStRgb3(out0,out1,out2);
}

void Psyz_GteColorDpq(VECTOR *ir,CVECTOR *rgb,long p,CVECTOR *out) {
    shim_ldlvl(ir);
    Psyz_GteLdRgb(rgb);
    Psyz_GteLdDp(p);
    shim_cop2(0x4B280414u,"CDP");
    Psyz_GteStRgb(out);
}

void Psyz_GteColorCol(VECTOR *ir,CVECTOR *rgb,CVECTOR *out) {
    shim_ldlvl(ir);
    Psyz_GteLdRgb(rgb);
    shim_cop2(0x4B38041Cu,"CC");
    Psyz_GteStRgb(out);
}

void Psyz_GteOuterProduct12(VECTOR *v0,VECTOR *v1,VECTOR *v2) {
    Psyz_GteLdopv1(v0);
    Psyz_GteLdopv2(v1);
    Psyz_GteOp12();
    Psyz_GteStlvnl(v2);
}

void Psyz_GteOuterProduct12SVL(SVECTOR *v0,SVECTOR *v1,VECTOR *v2) {
    shim_ldopv1sv(v0);
    shim_ldopv2sv(v1);
    Psyz_GteOp12();
    Psyz_GteStlvnl(v2);
}

void Psyz_GteOuterProduct0SVL(SVECTOR *v0,SVECTOR *v1,VECTOR *v2) {
    shim_ldopv1sv(v0);
    shim_ldopv2sv(v1);
    shim_cop2(0x4B70000Cu,"OP");
    Psyz_GteStlvnl(v2);
}

void Psyz_GteOuterProduct12SV(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2) {
    shim_ldopv1sv(v0);
    shim_ldopv2sv(v1);
    Psyz_GteOp12();
    Psyz_GteStsv(v2);
}

void Psyz_GteOuterProduct0SV(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2) {
    shim_ldopv1sv(v0);
    shim_ldopv2sv(v1);
    shim_cop2(0x4B70000Cu,"OP");
    Psyz_GteStsv(v2);
}

/* The rotation part: RT = m0; per column j of m1 (zero-extended halves to
 * IR1-3), MVMVA sf 1, RT, IR, no translation; IR1-3 low halves to column j of
 * m2. Then TR = m0->t; V0 = m1->t (low halves of t[0], t[1]; t[2]); MVMVA sf 1,
 * RT, V0, TR; MAC1-3 to m2->t. */
void Psyz_GteCompMatrix(MATRIX *m0,MATRIX *m1,MATRIX *m2) {
    int j;

    shim_wr(32,shim_pair(m0->m[0][0],m0->m[0][1]));
    shim_wr(33,shim_pair(m0->m[0][2],m0->m[1][0]));
    shim_wr(34,shim_pair(m0->m[1][1],m0->m[1][2]));
    shim_wr(35,shim_pair(m0->m[2][0],m0->m[2][1]));
    shim_wr(36,shim_pair(m0->m[2][2],0));
    for(j=0;j<3;j++) {
        shim_wr(9,(uint16_t)m1->m[0][j]);
        shim_wr(10,(uint16_t)m1->m[1][j]);
        shim_wr(11,(uint16_t)m1->m[2][j]);
        Psyz_GteRtir();
        m2->m[0][j]=(short)(uint16_t)shim_rd(9);
        m2->m[1][j]=(short)(uint16_t)shim_rd(10);
        m2->m[2][j]=(short)(uint16_t)shim_rd(11);
    }
    shim_wr(37,(uint32_t)m0->t[0]);
    shim_wr(38,(uint32_t)m0->t[1]);
    shim_wr(39,(uint32_t)m0->t[2]);
    shim_wr(0,shim_pair(m1->t[0],m1->t[1]));
    shim_wr(1,(uint32_t)m1->t[2]);
    Psyz_GteRt();
    m2->t[0]=(int)shim_rd(25);
    m2->t[1]=(int)shim_rd(26);
    m2->t[2]=(int)shim_rd(27);
}

/* (3) Names sh2o does not have: the case is not run. */
MATRIX *ReadRotMatrix(MATRIX *m) {
    ztest__skip(__LINE__,"sh2o lacks ReadRotMatrix");
    return m;
}

MATRIX *ReadLightMatrix(MATRIX *m) {
    ztest__skip(__LINE__,"sh2o lacks ReadLightMatrix");
    return m;
}

MATRIX *ReadColorMatrix(MATRIX *m) {
    ztest__skip(__LINE__,"sh2o lacks ReadColorMatrix");
    return m;
}

long ReadGeomScreen(void) {
    ztest__skip(__LINE__,"sh2o lacks ReadGeomScreen");
    return 0;
}

void ReadGeomOffset(int *x,int *y) {
    (void)x;
    (void)y;
    ztest__skip(__LINE__,"sh2o lacks ReadGeomOffset");
}

long RotTransPers(SVECTOR *v0,int *sxy,int *p,int *flag) {
    (void)v0;
    (void)sxy;
    (void)p;
    (void)flag;
    ztest__skip(__LINE__,"sh2o lacks RotTransPers");
    return 0;
}

long RotTransPers3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,int *sxy0,int *sxy1,int *sxy2,int *p,int *flag) {
    (void)v0;
    (void)v1;
    (void)v2;
    (void)sxy0;
    (void)sxy1;
    (void)sxy2;
    (void)p;
    (void)flag;
    ztest__skip(__LINE__,"sh2o lacks RotTransPers3");
    return 0;
}

long RotAverage3(SVECTOR *v0,SVECTOR *v1,SVECTOR *v2,int *sxy0,int *sxy1,int *sxy2,int *p,int *flag) {
    (void)v0;
    (void)v1;
    (void)v2;
    (void)sxy0;
    (void)sxy1;
    (void)sxy2;
    (void)p;
    (void)flag;
    ztest__skip(__LINE__,"sh2o lacks RotAverage3");
    return 0;
}

long NormalClip(long sxy0,long sxy1,long sxy2) {
    (void)sxy0;
    (void)sxy1;
    (void)sxy2;
    ztest__skip(__LINE__,"sh2o lacks NormalClip");
    return 0;
}

VECTOR *OuterProduct12(VECTOR *v0,VECTOR *v1,VECTOR *v2) {
    (void)v0;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks OuterProduct12");
    return v2;
}

VECTOR *OuterProduct0(VECTOR *v0,VECTOR *v1,VECTOR *v2) {
    (void)v0;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks OuterProduct0");
    return v2;
}

MATRIX *RotMatrix(SVECTOR *r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrix");
    return m;
}

MATRIX *RotMatrixX(long r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrixX");
    return m;
}

MATRIX *RotMatrixY(long r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrixY");
    return m;
}

MATRIX *RotMatrixZ(long r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrixZ");
    return m;
}

MATRIX *RotMatrixYXZ(SVECTOR *r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrixYXZ");
    return m;
}

MATRIX *RotMatrixZYX(SVECTOR *r,MATRIX *m) {
    (void)r;
    ztest__skip(__LINE__,"sh2o lacks RotMatrixZYX");
    return m;
}

MATRIX *ScaleMatrix(MATRIX *m,VECTOR *v) {
    (void)v;
    ztest__skip(__LINE__,"sh2o lacks ScaleMatrix");
    return m;
}

MATRIX *MulMatrix(MATRIX *m0,MATRIX *m1) {
    (void)m1;
    ztest__skip(__LINE__,"sh2o lacks MulMatrix");
    return m0;
}

MATRIX *SetMulMatrix(MATRIX *m0,MATRIX *m1) {
    (void)m1;
    ztest__skip(__LINE__,"sh2o lacks SetMulMatrix");
    return m0;
}

int rsin(int a) {
    (void)a;
    ztest__skip(__LINE__,"sh2o lacks rsin");
    return 0;
}

int rcos(int a) {
    (void)a;
    ztest__skip(__LINE__,"sh2o lacks rcos");
    return 0;
}

long AverageZ3(long sz0,long sz1,long sz2) {
    (void)sz0;
    (void)sz1;
    (void)sz2;
    ztest__skip(__LINE__,"sh2o lacks AverageZ3");
    return 0;
}

long AverageZ4(long sz0,long sz1,long sz2,long sz3) {
    (void)sz0;
    (void)sz1;
    (void)sz2;
    (void)sz3;
    ztest__skip(__LINE__,"sh2o lacks AverageZ4");
    return 0;
}

void VectorNormal(VECTOR *v0,VECTOR *v1) {
    (void)v0;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks VectorNormal");
}

long VectorNormalS(VECTOR *v0,SVECTOR *v1) {
    (void)v0;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks VectorNormalS");
    return 0;
}

long VectorNormalSS(SVECTOR *v0,SVECTOR *v1) {
    (void)v0;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks VectorNormalSS");
    return 0;
}

void SetFogNear(long a,long h) {
    (void)a;
    (void)h;
    ztest__skip(__LINE__,"sh2o lacks SetFogNear");
}

long SquareRoot0(long a) {
    (void)a;
    ztest__skip(__LINE__,"sh2o lacks SquareRoot0");
    return 0;
}

long SquareRoot12(long a) {
    (void)a;
    ztest__skip(__LINE__,"sh2o lacks SquareRoot12");
    return 0;
}

void NormalColorCol(SVECTOR *v0,CVECTOR *v1,CVECTOR *v2) {
    (void)v0;
    (void)v1;
    (void)v2;
    ztest__skip(__LINE__,"sh2o lacks NormalColorCol");
}

void NormalColorDpq(SVECTOR *v0,CVECTOR *v1,long p,CVECTOR *v2) {
    (void)v0;
    (void)v1;
    (void)p;
    (void)v2;
    ztest__skip(__LINE__,"sh2o lacks NormalColorDpq");
}

void DpqColor(CVECTOR *v0,long p,CVECTOR *v1) {
    (void)v0;
    (void)p;
    (void)v1;
    ztest__skip(__LINE__,"sh2o lacks DpqColor");
}

int main(int argc,char **argv) {
    return ztest_main(argc,argv);
}
