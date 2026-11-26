/* The GTE commands.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* New code from psx-spx's formulas, one function per command. Order: MVMVA;
 * NCLIP, AVSZ3, SQR; GPF, GPL, DPCS; RTPS, RTPT; NCS, NCT, NCDS, NCDT, NCCS. Products are 16x16 in 32 bits or
 * 32x32 widening to 64; 64-bit sums only added, compared and split into words (sh2o_gte_internal.h). */
#include <stdint.h>

#include "sh2o_gte_internal.h"
#include "sh2o_count_internal.h"
#if SH2O_ZAM
#include "sh2o/zam_experimental.h"
#endif

static void sh2oi_begin(sh2oi_cx *cx,sh2o_gte_t *g,uint32_t cmd) {
    cx->g = g;
    cx->flag = 0; /* FLAG reset at each command start */
    cx->sf = cmd >> 19 & 1u;
    cx->lm = cmd >> 10 & 1u;
}

static void sh2oi_end(const sh2oi_cx *cx) {
    cx->g->r[63] = cx->flag;
}

static int32_t sh2oi_ir(const sh2o_gte_t *g,unsigned n) { /* IR0-3 */
    return sh2oi_s16(g->r[8 + n]);
}

#if SH2O_MVB3_SWITCH
/* test-only (the SH-2 bench, -DSH2O_MVB3_SWITCH=1): sh2oi_mac3's path, 0 the dispatch line (sh2o_k_mvb3),
 * 1 the flagged path forced, 2 sh2o_k_mvb3_c, 3 sh2o_k_mvb3_w2 (sh2o_k_mvb3_c off the SH-2); counts of each path */
int sh2o_gte_mvb3_mode;
uint32_t sh2o_gte_mvb3_fast,sh2o_gte_mvb3_flagged;
#endif

/* T*1000h + M*V, M the five words from base (32 RT, 40 light matrix, 48 LCM) or m when base is 0 (MVMVA mx = 3).
 * Fast path (sh2o_k_mvb3) when every |t[i]| < 7FF40000h: no partial can then pass 43 bits, no flag and no truncation
 * (the 43-bit bound); else each partial flagged and truncated to 44 bits. */
static void sh2oi_mac3(sh2oi_cx *cx,unsigned base,int32_t m[3][3],const int32_t v[3],const int32_t t[3],
                        int64_t a[3]) {
    int32_t mm[3][3];
    unsigned i;
    if(base && (uint32_t)t[0] + 0x7FF3FFFFu < 0xFFE7FFFFu && (uint32_t)t[1] + 0x7FF3FFFFu < 0xFFE7FFFFu &&
        (uint32_t)t[2] + 0x7FF3FFFFu < 0xFFE7FFFFu
#if SH2O_MVB3_SWITCH
        && sh2o_gte_mvb3_mode != 1
#endif
    ) {
        int16_t v16[3];
        v16[0] = (int16_t)v[0];
        v16[1] = (int16_t)v[1];
        v16[2] = (int16_t)v[2];
#if SH2O_MVB3_SWITCH
        sh2o_gte_mvb3_fast++;
        if(sh2o_gte_mvb3_mode == 2) {
            sh2o_k_mvb3_c(&cx->g->r[base],v16,t,a);
            return;
        }
        /* mode 3 (sh2o_k_mvb3_w2) is the dispatch line below on the SH-2: one kernel copy at every level */
#endif
        sh2o_k_mvb3(&cx->g->r[base],v16,t,a);
        return;
    }
#if SH2O_MVB3_SWITCH
    sh2o_gte_mvb3_flagged++;
#endif
    if(base) {
        sh2oi_mat(cx->g,base,mm);
        m = mm;
    }
    for(i = 0; i < 3; i++) {
        int64_t s = sh2oi_mac44(cx,i,sh2oi_shl12(t[i]) + m[i][0] * v[0]);
        s = sh2oi_mac44(cx,i,s + m[i][1] * v[1]);
        a[i] = sh2oi_mac44(cx,i,s + m[i][2] * v[2]);
    }
}

/* [MAC1-3] = (T*1000h + M*V) SAR sf (sh2oi_mac3: M from base, or m when base is 0); IR1-3 (lm) */
static void sh2oi_mat_vec(sh2oi_cx *cx,unsigned base,int32_t m[3][3],const int32_t v[3],const int32_t t[3],
                            unsigned lm) {
    int64_t a[3];
    sh2oi_mac3(cx,base,m,v,t,a);
    sh2oi_set_mac_ir(cx,a,lm);
}

#if SH2O_ZAM
/* zam-1 delegate for the MAC stage when sf = 1: 1 with full[i] = MAC * 1000h (its SAR 12 is the MAC; no MAC flag
 * inside the delegate's domain), 0 when refused (the caller runs its own kernel) */
static int sh2oi_zam_try(const sh2oi_cx *cx,int32_t m[3][3],const int32_t v[3],const int32_t t[3],int64_t full[3]) {
    int32_t mac[3];
    unsigned i;
    if(!cx->sf || !sh2o_zam_mac3((const int32_t (*)[3])m,v,t,mac))
        return 0;
    for(i = 0; i < 3; i++)
        full[i] = sh2oi_shl12(mac[i]); /* (int64_t)mac * 4096 by shifts */
    return 1;
}
#endif

/* MAC+(FC-MAC)*IR0 on base = [MAC1-3] before the shift: IR = ((FC SHL 12) - base) SAR sf (lm=0);
 * MAC = (IR * IR0 + base) SAR sf; IR (lm); colour FIFO */
static void sh2oi_interp_fc(sh2oi_cx *cx,const int32_t base[3]) {
    int32_t fc[3];
    int64_t a[3];
    unsigned i;
    sh2oi_cvec(cx->g,53,fc);
    for(i = 0; i < 3; i++)
        a[i] = sh2oi_mac44(cx,i,sh2oi_shl12(fc[i]) - base[i]);
    sh2oi_set_mac_ir(cx,a,0);
    for(i = 0; i < 3; i++)
        a[i] = sh2oi_mac44(cx,i,(int64_t)(sh2oi_ir(cx->g,1 + i) * sh2oi_ir(cx->g,0)) + base[i]);
    sh2oi_set_mac_ir(cx,a,cx->lm);
    sh2oi_push_rgb(cx);
}

/* (light matrix * Vn) SAR sf -> IR; (BK*1000h + LCM*IR) SAR sf -> IR */
static void sh2oi_light(sh2oi_cx *cx,unsigned n) {
    static const int32_t zero[3] = {0,0,0};
    int32_t v[3],bk[3];
    sh2oi_vec(cx->g,n,v);
    sh2oi_mat_vec(cx,40,0,v,zero,cx->lm);
    v[0] = sh2oi_ir(cx->g,1);
    v[1] = sh2oi_ir(cx->g,2);
    v[2] = sh2oi_ir(cx->g,3);
    sh2oi_cvec(cx->g,45,bk);
    sh2oi_mat_vec(cx,48,0,v,bk,cx->lm);
}

/* [R*IR1, G*IR2, B*IR3] SHL 4 (RGBC colour bytes) */
static void sh2oi_rgb_ir(const sh2o_gte_t *g,int32_t base[3]) {
    unsigned i;
    sh2oi_rgb(g,base);
    for(i = 0; i < 3; i++)
        base[i] = base[i] * sh2oi_ir(g,1 + i) * 16;
}

/* MVMVA */
/* Contract tag gte-mvmva-1. Contract in sh2o/gte.h. */
void sh2o_gte_mvmva(sh2o_gte_t *g,uint32_t cmd) {
    static const int32_t zero[3] = {0,0,0};
    static const unsigned mbase[3] = {32,40,48};
    sh2oi_cx cx;
    unsigned mx = cmd >> 17 & 3u,vx = cmd >> 15 & 3u,cv = cmd >> 13 & 3u,i,base = 0;
    int32_t m[3][3],v[3],t[3];
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_MVMVA,0x12u);
    sh2oi_begin(&cx,g,cmd);
    if(mx == 3) { /* hw bug: [-R*10h, R*10h, IR0; RT13 x3; RT22 x3] */
        int32_t r16 = (int32_t)(g->r[6] & 0xFFu) * 16;
        int32_t rt13 = sh2oi_s16(g->r[33]),rt22 = sh2oi_s16(g->r[34]);
        m[0][0] = -r16;
        m[0][1] = r16;
        m[0][2] = sh2oi_ir(g,0);
        for(i = 0; i < 3; i++) {
            m[1][i] = rt13;
            m[2][i] = rt22;
        }
    } else if(cv == 2 || SH2O_ZAM) {
        sh2oi_mat(g,mbase[mx],m);
        base = mbase[mx];
    } else {
        base = mbase[mx]; /* sh2oi_mac3 reads the words (or extracts on its flagged path) */
    }
    if(vx == 3) {
        for(i = 0; i < 3; i++)
            v[i] = sh2oi_ir(g,1 + i);
    } else {
        sh2oi_vec(g,vx,v);
    }
    if(cv == 2) { /* FC bug: partial flagged, result = last two portions */
        int64_t a[3];
        sh2oi_cvec(g,53,t);
        for(i = 0; i < 3; i++)
            (void)sh2oi_sat_ir(&cx,i,sh2oi_mac_sf(&cx,sh2oi_mac44(&cx,i,sh2oi_shl12(t[i]) + m[i][0] * v[0])),0);
        for(i = 0; i < 3; i++)
            a[i] = sh2oi_mac44(&cx,i,sh2oi_mac44(&cx,i,(int64_t)(m[i][1] * v[1])) + m[i][2] * v[2]);
        sh2oi_set_mac_ir(&cx,a,cx.lm);
    } else {
        if(cv == 0) {
            sh2oi_cvec(g,37,t);
        } else if(cv == 1) {
            sh2oi_cvec(g,45,t);
        }
#if SH2O_ZAM
        { int64_t full[3];
            if(mx < 3 && sh2oi_zam_try(&cx,m,v,cv == 3 ? zero : t,full)) {
                sh2oi_set_mac_ir(&cx,full,cx.lm);
                sh2oi_end(&cx);
                return;
            }
        }
#endif
        sh2oi_mat_vec(&cx,base,m,v,cv == 3 ? zero : t,cx.lm);
    }
    sh2oi_end(&cx);
}

/* NCLIP, AVSZ3, SQR */
/* Contract tag gte-nclip-1. Contract in sh2o/gte.h. */
void sh2o_gte_nclip(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t sx[3],sy[3];
    int64_t s;
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCLIP,0x06u);
    sh2oi_begin(&cx,g,cmd);
    for(i = 0; i < 3; i++) {
        sx[i] = sh2oi_s16(g->r[12 + i]);
        sy[i] = sh2oi_h16(g->r[12 + i]);
    }
    s = (int64_t)(sx[0] * sy[1]) + sx[1] * sy[2];
    s += sx[2] * sy[0];
    s -= sx[0] * sy[2];
    s -= sx[1] * sy[0];
    s -= sx[2] * sy[1];
    g->r[24] = (uint32_t)sh2oi_mac0(&cx,s);
    sh2oi_end(&cx);
}

/* Contract tag gte-avsz3-1. Contract in sh2o/gte.h. */
void sh2o_gte_avsz3(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t sum = (int32_t)((g->r[17] & 0xFFFFu) + (g->r[18] & 0xFFFFu) + (g->r[19] & 0xFFFFu));
    int64_t a;
    int32_t z;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_AVSZ3,0x2Du);
    sh2oi_begin(&cx,g,cmd);
    a = sh2oi_mac0(&cx,(int64_t)sh2oi_s16(g->r[61]) * sum);
    g->r[24] = (uint32_t)a;
    z = sh2oi_sar12(a); /* OTZ from the unwrapped sum (model docstring) */
    if(z < 0 || z > 0xFFFF) {
        cx.flag |= 1u << 18;
        z = z < 0 ? 0 : 0xFFFF;
    }
    g->r[7] = (uint32_t)z;
    sh2oi_end(&cx);
}

/* Contract tag gte-sqr-1. Contract in sh2o/gte.h. */
void sh2o_gte_sqr(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int64_t a[3];
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_SQR,0x28u);
    sh2oi_begin(&cx,g,cmd);
    for(i = 0; i < 3; i++) {
        int32_t x = sh2oi_ir(g,1 + i);
        a[i] = sh2oi_mac44(&cx,i,(int64_t)(x * x));
    }
    sh2oi_set_mac_ir(&cx,a,cx.lm);
    sh2oi_end(&cx);
}

/* GPF, GPL, DPCS */
/* GPF and GPL's MAC stage on the fast path: sh2o_k_gpf3 (gpl 0) or sh2o_k_gpl3 (gpl 1); under the switch
 * the path as sh2oi_mac3's (mode 2 the C, 3 `_w2`, else the dispatch line); counted as fast */
static void sh2oi_gp_fast(sh2oi_cx *cx,int gpl,int64_t a[3]) {
    const uint32_t *w = &cx->g->r[8];
    const uint32_t *mac = &cx->g->r[25];
#if SH2O_MVB3_SWITCH
    sh2o_gte_mvb3_fast++;
    if(sh2o_gte_mvb3_mode == 2) {
        if(gpl) {
            sh2o_k_gpl3_c(w,mac,cx->sf,a);
        } else {
            sh2o_k_gpf3_c(w,a);
        }
        return;
    }
#if SH2OI_NATIVE
    if(sh2o_gte_mvb3_mode == 3) {
        if(gpl) {
            sh2o_k_gpl3_w2(w,mac,cx->sf,a);
        } else {
            sh2o_k_gpf3_w2(w,a);
        }
        return;
    }
#endif
#endif
    if(gpl) {
        sh2o_k_gpl3(w,mac,cx->sf,a);
    } else {
        sh2o_k_gpf3(w,a);
    }
}

/* GPF: no partial can pass 43 bits (|IR * IR0| <= 2^30), so the fast path always (the flagged one only when the
 * switch forces it, mode 1) */
/* Contract tag gte-gpf-1. Contract in sh2o/gte.h. */
void sh2o_gte_gpf(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int64_t a[3];
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_GPF,0x3Du);
    sh2oi_begin(&cx,g,cmd);
#if SH2O_MVB3_SWITCH
    if(sh2o_gte_mvb3_mode == 1) {
        sh2o_gte_mvb3_flagged++;
        for(i = 0; i < 3; i++)
            a[i] = sh2oi_mac44(&cx,i,(int64_t)(sh2oi_ir(g,1 + i) * sh2oi_ir(g,0)));
    } else {
#endif
        (void)i;
        sh2oi_gp_fast(&cx,0,a);
#if SH2O_MVB3_SWITCH
    }
#endif
    sh2oi_set_mac_ir(&cx,a,cx.lm);
    sh2oi_push_rgb(&cx);
    sh2oi_end(&cx);
}

/* Contract tag gte-gpl-1. Contract in sh2o/gte.h. */
void sh2o_gte_gpl(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int64_t a[3];
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_GPL,0x3Eu);
    sh2oi_begin(&cx,g,cmd);
    /* fast when no sum can pass 43 bits: sf 0 (|MAC| <= 2^31, |IR * IR0| <= 2^30), or every |MAC| < 7FFC0000h
     * (|MAC * 1000h| <= 2^43 - 2^30 - 1000h, plus 2^30 stays under 2^43; the 43-bit bound
     * with one product) */
    if((!cx.sf || (g->r[25] + 0x7FFBFFFFu < 0xFFF7FFFFu && g->r[26] + 0x7FFBFFFFu < 0xFFF7FFFFu &&
                    g->r[27] + 0x7FFBFFFFu < 0xFFF7FFFFu))
#if SH2O_MVB3_SWITCH
        && sh2o_gte_mvb3_mode != 1
#endif
    ) {
        sh2oi_gp_fast(&cx,1,a);
        sh2oi_set_mac_ir(&cx,a,cx.lm);
        sh2oi_push_rgb(&cx);
        sh2oi_end(&cx);
        return;
    }
#if SH2O_MVB3_SWITCH
    sh2o_gte_mvb3_flagged++;
#endif
    for(i = 0; i < 3; i++) {
        int32_t mac = (int32_t)g->r[25 + i];
        a[i] = sh2oi_mac44(&cx,i,(cx.sf ? sh2oi_shl12(mac) : (int64_t)mac) + sh2oi_ir(g,1 + i) * sh2oi_ir(g,0));
    }
    sh2oi_set_mac_ir(&cx,a,cx.lm);
    sh2oi_push_rgb(&cx);
    sh2oi_end(&cx);
}

/* Contract tag gte-dpcs-1. Contract in sh2o/gte.h. */
void sh2o_gte_dpcs(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t base[3];
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_DPCS,0x10u);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_rgb(g,base);
    for(i = 0; i < 3; i++)
        base[i] <<= 16;
    sh2oi_interp_fc(&cx,base);
    sh2oi_end(&cx);
}

/* Contract tag gte-dpct-1. Contract in sh2o/gte.h. */
void sh2o_gte_dpct(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t base[3];
    uint32_t c;
    unsigned i,n;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_DPCT,0x2Au);
#if SH2OI_NATIVE
    sh2o_gte_dpct32(g,cmd);                     /* gte-dpct32-1: the same image in 32-bit words */
    return;
#endif
    sh2oi_begin(&cx,g,cmd);
    for(n = 0; n < 3; n++) {
        c = g->r[20];                           /* RGB0: each step's push moves the next colour down to it */
        for(i = 0; i < 3; i++)
            base[i] = (int32_t)(c >> 8 * i & 0xFFu) << 16;
        sh2oi_interp_fc(&cx,base);
    }
    sh2oi_end(&cx);
}

/* RTPS, RTPT */
/* vertex n: MAC1-3/IR1-3, SZ3, SX2/SY2; IR0/MAC0 depth cue when last */
static void sh2oi_rtp(sh2oi_cx *cx,unsigned n,int last) {
    sh2o_gte_t *g = cx->g;
    int32_t m[3][3],v[3],t[3],sxy[2],mac,z;
    int64_t full[3];
    uint32_t h,sz,q;
    unsigned i;
    sh2oi_vec(g,n,v);
    sh2oi_cvec(g,37,t);
#if SH2O_ZAM
    sh2oi_mat(g,32,m);
    if(!sh2oi_zam_try(cx,m,v,t,full))
#endif
        sh2oi_mac3(cx,32,m,v,t,full);
    for(i = 0; i < 2; i++) {
        mac = sh2oi_mac_sf(cx,full[i]);
        g->r[25 + i] = (uint32_t)mac;
        g->r[9 + i] = (uint32_t)sh2oi_sat_ir(cx,i,mac,cx->lm);
    }
    mac = sh2oi_mac_sf(cx,full[2]);
    g->r[27] = (uint32_t)mac;
    z = sh2oi_sar12(full[2]); /* IR3 flag as if lm=0, from MAC3 SAR 12 regardless of sf */
    if(z < -0x8000 || z > 0x7FFF)
        cx->flag |= 1u << 22;
    g->r[11] = (uint32_t)(mac < (cx->lm ? 0 : -0x8000) ? (cx->lm ? 0 : -0x8000) : mac > 0x7FFF ? 0x7FFF : mac);
    sh2oi_push_sz(cx,z);   /* SZ3 = MAC3 SAR ((1-sf)*12) */
    h = g->r[58] & 0xFFFFu; /* H unsigned in the calculation */
    sz = g->r[19] & 0xFFFFu;
    if(h < sz * 2) {
        q = sh2o_k_unr_div(h,sz);
    } else {
        q = 0x1FFFFu;
        cx->flag |= 1u << 17 | 1u << 31;
    }
    for(i = 0; i < 2; i++) {
        int64_t s = (int64_t)(int32_t)q * sh2oi_ir(g,1 + i) + (int32_t)g->r[56 + i];
        int32_t p = sh2oi_sar16(sh2oi_mac0(cx,s));
        if(p < -0x400 || p > 0x3FF) {
            cx->flag |= i ? 1u << 13 : 1u << 14;
            p = p < -0x400 ? -0x400 : 0x3FF;
        }
        sxy[i] = p;
        g->r[24] = (uint32_t)s;
    }
    g->r[12] = g->r[13];
    g->r[13] = g->r[14];
    g->r[14] = ((uint32_t)sxy[0] & 0xFFFFu) | ((uint32_t)sxy[1] & 0xFFFFu) << 16;
    if(last) {
        int64_t d = sh2oi_mac0(cx,(int64_t)(int32_t)q * sh2oi_s16(g->r[59]) + (int32_t)g->r[60]);
        int32_t ir0 = sh2oi_sar12(d);
        g->r[24] = (uint32_t)d;
        if(ir0 < 0 || ir0 > 0x1000) {
            cx->flag |= 1u << 12;
            ir0 = ir0 < 0 ? 0 : 0x1000;
        }
        g->r[8] = (uint32_t)ir0;
    }
}

/* Contract tag gte-rtps-1. Contract in sh2o/gte.h. */
void sh2o_gte_rtps(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_RTPS,0x01u);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_rtp(&cx,0,1);
    sh2oi_end(&cx);
}

/* Contract tag gte-rtpt-1. Contract in sh2o/gte.h. */
void sh2o_gte_rtpt(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_RTPT,0x30u);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_rtp(&cx,0,0);
    sh2oi_rtp(&cx,1,0);
    sh2oi_rtp(&cx,2,1);
    sh2oi_end(&cx);
}

/* NCS, NCT, NCDS, NCDT, NCCS */
/* Contract tag gte-ncs-1. Contract in sh2o/gte.h. */
void sh2o_gte_ncs(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCS,0x1Eu);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_light(&cx,0);
    sh2oi_push_rgb(&cx);
    sh2oi_end(&cx);
}

/* Contract tag gte-nct-1. Contract in sh2o/gte.h. */
void sh2o_gte_nct(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    unsigned n;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCT,0x20u);
    sh2oi_begin(&cx,g,cmd);
    for(n = 0; n < 3; n++) {
        sh2oi_light(&cx,n);
        sh2oi_push_rgb(&cx);
    }
    sh2oi_end(&cx);
}

/* Contract tag gte-ncds-1. Contract in sh2o/gte.h. */
void sh2o_gte_ncds(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t base[3];
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCDS,0x13u);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_light(&cx,0);
    sh2oi_rgb_ir(g,base);
    sh2oi_interp_fc(&cx,base);
    sh2oi_end(&cx);
}

/* Contract tag gte-ncdt-1. Contract in sh2o/gte.h. */
void sh2o_gte_ncdt(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t base[3];
    unsigned n;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCDT,0x16u);
    sh2oi_begin(&cx,g,cmd);
    for(n = 0; n < 3; n++) {
        sh2oi_light(&cx,n);
        sh2oi_rgb_ir(g,base);
        sh2oi_interp_fc(&cx,base);
    }
    sh2oi_end(&cx);
}

/* Contract tag gte-nccs-1. Contract in sh2o/gte.h. */
void sh2o_gte_nccs(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cx cx;
    int32_t base[3];
    int64_t a[3];
    unsigned i;
    SH2OI_COUNT_CMD(SH2O_COUNT_GTE_NCCS,0x1Bu);
    sh2oi_begin(&cx,g,cmd);
    sh2oi_light(&cx,0);
    sh2oi_rgb_ir(g,base);
    for(i = 0; i < 3; i++)
        a[i] = sh2oi_mac44(&cx,i,(int64_t)base[i]);
    sh2oi_set_mac_ir(&cx,a,cx.lm);
    sh2oi_push_rgb(&cx);
    sh2oi_end(&cx);
}

/* op -> command; a table in .rodata, not a switch (a jump table in .text disassembles as stray instructions) */
typedef void (*sh2oi_cmd_fn)(sh2o_gte_t *g,uint32_t cmd);
static const sh2oi_cmd_fn sh2oi_cmds[64] SH2OI_RODATA(sh2oi_cmds) = {
    [0x12] = sh2o_gte_mvmva,[0x06] = sh2o_gte_nclip,[0x2D] = sh2o_gte_avsz3,[0x28] = sh2o_gte_sqr,
    [0x3D] = sh2o_gte_gpf,[0x3E] = sh2o_gte_gpl,[0x10] = sh2o_gte_dpcs,[0x01] = sh2o_gte_rtps,
    [0x30] = sh2o_gte_rtpt,[0x1E] = sh2o_gte_ncs,[0x20] = sh2o_gte_nct,[0x13] = sh2o_gte_ncds,
    [0x16] = sh2o_gte_ncdt,[0x1B] = sh2o_gte_nccs,
    [0x2A] = sh2o_gte_dpct,
};

/* Contract tag gte-command-1. Contract in sh2o/gte.h. */
int sh2o_gte_command(sh2o_gte_t *g,uint32_t cmd) {
    sh2oi_cmd_fn f = sh2oi_cmds[cmd & 0x3Fu];
    SH2OI_COUNT_COMMAND(f,cmd);
    if(!f)
        return -1;
    SH2OI_COUNT_IN();
    f(g,cmd);
    SH2OI_COUNT_OUT();
    return 0;
}

/* Contract tag gte-nrtps-1. Contract in sh2o/gte.h. */
void sh2o_gte_nrtps(sh2o_gte_t *g) {
    SH2OI_COUNT_NAME(SH2O_COUNT_GTE_NRTPS);
    SH2OI_COUNT_IN();
    sh2o_gte_rtps(g,0x4A180001u);
    SH2OI_COUNT_OUT();
}

/* Contract tag gte-nrtpt-1. Contract in sh2o/gte.h. */
void sh2o_gte_nrtpt(sh2o_gte_t *g) {
    SH2OI_COUNT_NAME(SH2O_COUNT_GTE_NRTPT);
    SH2OI_COUNT_IN();
    sh2o_gte_rtpt(g,0x4A280030u);
    SH2OI_COUNT_OUT();
}
