/* MAC stage of the GTE core through SH2ZAM's prepared MAC executor.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Contracts in sh2o/zam_experimental.h.
 * The only sh2o file that includes SH2ZAM (pinned, unmodified, external/SH2ZAM). Compiled to an empty unit unless
 * SH2O_ZAM=1 (the CMake option SH2O_ZAM), so the core build never needs SH2ZAM's headers.
 *
 * Why the delegate equals the GTE core (zam-mac3-1). Row i, R = m[i], S = sum_j |R_j||v_j|, x = R.v, T = t[i].
 * SH2ZAM is handed the row R*16 with translation T and the point v (raw integers in its Q16.16 slots); its fast path
 * gives
 *   z = floor(16x / 65536) + T = floor(x / 4096) + T = floor((4096T + x) / 4096) = (4096T + x) SAR 12,
 * which is the GTE core's MAC with sf = 1 provided its 44-bit partials do not wrap. The gate 16S + 65536|T| <= 2^47-1
 * gives S + 4096|T| <= 2^43-1 (both sides of 16*(..) are multiples of 16), and every partial the GTE core forms
 * (4096T, then + R_0 v_0, then + R_1 v_1, then + R_2 v_2) is bounded in magnitude by S + 4096|T| < 2^43: no FLAG
 * 30-25 bit, no truncation, and the result fits int32. So the GTE core's tail, fed full = mac * 4096 (whose SAR 12
 * is mac), writes the same 64 words.
 * The left side is always a multiple of 16, so the largest admitted row is exactly 2^47-16; the next, 2^47, is refused.
 *
 * Mutants (-DSH2O_MUTANT=k, the delegate's suite tests/suite/sh2o_zam_test_suite.c): 1 rows shifted by 3, not 4;
 * 2 gate widened to 2^49-1 and the fast-path requirement dropped (the plan's own bound equals ours, so widening ours
 * alone changes nothing); 3 translation dropped. Each must make the zam suite fail.
 * zam-ratan2-1, zam-transposematrix-1, zam-setfognearfar-1 (contracts in sh2o/zam_experimental.h): sh2o_psyq
 * candidates. Mutants: 4 TransposeMatrix through SH2ZAM twice (no transpose); 5 SetFogNearFar's no-wrap gate
 * dropped (SH2ZAM's exact products then differ from the object's wrapped ones). Each must make the zam suite fail. */
#ifndef SH2O_ZAM
#define SH2O_ZAM 0
#endif

typedef int sh2oi_zam_unit; /* keeps the unit non-empty when SH2O_ZAM is 0 (ISO C) */

#if SH2O_ZAM
#include <stdint.h>

#include <sh2zam/sh2zam.h>
#include <sh2zam/batch_experimental.h>

#include "sh2o/zam_experimental.h"

#ifndef SH2O_MUTANT
#define SH2O_MUTANT 0
#endif

#if SH2O_MUTANT == 1
#define SH2OI_ZAM_ROW_SHIFT 3
#else
#define SH2OI_ZAM_ROW_SHIFT 4
#endif

#if SH2O_MUTANT == 2
#define SH2OI_ZAM_GATE ((UINT64_C(1) << 49) - 1u)
#else
#define SH2OI_ZAM_GATE ((UINT64_C(1) << 47) - 1u)
#endif

int sh2o_zam_bypass;
uint32_t sh2o_zam_delegated,sh2o_zam_refused;

static uint32_t sh2oi_zam_abs(int32_t a)
{
    return a < 0 ? 0u - (uint32_t)a : (uint32_t)a;
}

/* 1 when every entry is in s16 range and each row passes 16*S + 65536|t| <= gate */
static int sh2oi_zam_domain(const int32_t m[3][3],const int32_t v[3],const int32_t t[3])
{
    unsigned i,j;
    for(j = 0; j < 3; j++)
        if(sh2oi_zam_abs(v[j]) > 0x8000u)
            return 0;
    for(i = 0; i < 3; i++)
    {
        uint64_t s = 0;
        for(j = 0; j < 3; j++)
        {
            if(sh2oi_zam_abs(m[i][j]) > 0x8000u)
                return 0;
            s += (uint64_t)(sh2oi_zam_abs(m[i][j]) * sh2oi_zam_abs(v[j])); /* each <= 2^30 */
        }
        if((s << 4) + ((uint64_t)sh2oi_zam_abs(t[i]) << 16) > SH2OI_ZAM_GATE)
            return 0;
    }
    return 1;
}

/* Contract tag zam-mac3-1. Contract in sh2o/zam_experimental.h. */
int sh2o_zam_mac3(const int32_t m[3][3],const int32_t v[3],const int32_t t[3],int32_t mac[3])
{
    sh2z_mat34_t rows;
    sh2z_vec3_t point,out;
    sh2z_mesh_view_t view;
    sh2z_transform_plan_t plan;
    unsigned i,j;
    if(sh2o_zam_bypass)
        return 0;
    if(!sh2oi_zam_domain(m,v,t))
    {
        sh2o_zam_refused++;
        return 0;
    }
    for(i = 0; i < 3; i++)
    {
        for(j = 0; j < 3; j++)
            rows.m[i][j] = (int32_t)((uint32_t)m[i][j] << SH2OI_ZAM_ROW_SHIFT);
#if SH2O_MUTANT == 3
        rows.m[i][3] = 0;
#else
        rows.m[i][3] = t[i];
#endif
    }
    point.x = v[0];
    point.y = v[1];
    point.z = v[2];
    if(!sh2z_mesh_view_init(&view,&point,1) || !sh2z_transform_plan_init(&plan,&view,&rows)
#if SH2O_MUTANT != 2
        || !plan.fast_path
#endif
        || !sh2z_transform_plan_execute_mac(&out,&plan,0,1))
    {
        sh2o_zam_refused++;
        return 0;
    }
    mac[0] = out.x;
    mac[1] = out.y;
    mac[2] = out.z;
    sh2o_zam_delegated++;
    return 1;
}

static int32_t sh2oi_zam_s16(uint32_t w)
{
    return (int32_t)((w & 0xFFFFu) ^ 0x8000u) - 0x8000;
}

/* The list through one prepared plan. Each item is zam-mac3-1's case with the same m and t; the gate runs once on
 * the per-axis largest |v| (every item's row sum is at most that one's), so the plan's own bound holds for every
 * point and SH2ZAM's MAC executor gives every item's MAC as zam-mac3-1 does. The GTE core's tail per item: MAC1-3,
 * IR1-3 saturated (lm) with FLAG 24-22, FLAG the last item's (sh2oi_set_mac_ir, sh2oi_end). */
/* Contract tag zam-mvmva-list-1. Contract in sh2o/zam_experimental.h. */
int sh2o_zam_mvmva_list(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6])
{
    static const unsigned mbase[3] = {32,40,48};
    static const uint32_t irbit[3] = {1u << 24,1u << 23,1u << 22};
    sh2z_vec3_t pts[SH2O_ZAM_LIST_MAX],res[SH2O_ZAM_LIST_MAX];
    sh2z_mat34_t rows;
    sh2z_mesh_view_t view;
    sh2z_transform_plan_t plan;
    int32_t m[3][3],t[3] = {0,0,0},vmax[3] = {0,0,0};
    const uint32_t *w;
    uint32_t *r = g->r,flag = 0;
    unsigned mx = cmd >> 17 & 3u,vx = cmd >> 15 & 3u,cv = cmd >> 13 & 3u,lm = cmd >> 10 & 1u,k,i,j;
    if(sh2o_zam_bypass)
        return 0;
    if(!(cmd >> 19 & 1u) || (cmd & 0x3Fu) != 0x12u || mx == 3u || vx != 0u || cv == 2u || n > SH2O_ZAM_LIST_MAX)
    {
        sh2o_zam_refused++;
        return 0;
    }
    if(n == 0)
        return 1;
    w = &r[mbase[mx]];
    m[0][0] = sh2oi_zam_s16(w[0]);
    m[0][1] = sh2oi_zam_s16(w[0] >> 16);
    m[0][2] = sh2oi_zam_s16(w[1]);
    m[1][0] = sh2oi_zam_s16(w[1] >> 16);
    m[1][1] = sh2oi_zam_s16(w[2]);
    m[1][2] = sh2oi_zam_s16(w[2] >> 16);
    m[2][0] = sh2oi_zam_s16(w[3]);
    m[2][1] = sh2oi_zam_s16(w[3] >> 16);
    m[2][2] = sh2oi_zam_s16(w[4]);
    if(cv < 2u)
        for(i = 0; i < 3; i++)
            t[i] = (int32_t)r[(cv ? 45u : 37u) + i];
    for(k = 0; k < n; k++)
    {
        int32_t p[3];
        p[0] = sh2oi_zam_s16(v[k][0]);
        p[1] = sh2oi_zam_s16(v[k][0] >> 16);
        p[2] = sh2oi_zam_s16(v[k][1]);
        pts[k].x = p[0];
        pts[k].y = p[1];
        pts[k].z = p[2];
        for(j = 0; j < 3; j++)
            if((int32_t)sh2oi_zam_abs(p[j]) > vmax[j])
                vmax[j] = (int32_t)sh2oi_zam_abs(p[j]);
    }
    if(!sh2oi_zam_domain((const int32_t (*)[3])m,vmax,t))
    {
        sh2o_zam_refused++;
        return 0;
    }
    for(i = 0; i < 3; i++)
    {
        for(j = 0; j < 3; j++)
            rows.m[i][j] = (int32_t)((uint32_t)m[i][j] << SH2OI_ZAM_ROW_SHIFT);
        rows.m[i][3] = t[i];
    }
    if(!sh2z_mesh_view_init(&view,pts,n) || !sh2z_transform_plan_init(&plan,&view,&rows) || !plan.fast_path ||
        !sh2z_transform_plan_execute_mac(res,&plan,0,n))
    {
        sh2o_zam_refused++;
        return 0;
    }
    for(k = 0; k < n; k++)
    {
        int32_t mac[3];
        mac[0] = res[k].x;
        mac[1] = res[k].y;
        mac[2] = res[k].z;
        flag = 0;
        for(i = 0; i < 3; i++)
        {
            int32_t ir = mac[i];
            if(ir < (lm ? 0 : -0x8000))
            {
                flag |= irbit[i];
                ir = lm ? 0 : -0x8000;
            }
            else if(ir > 0x7FFF)
            {
                flag |= irbit[i];
                ir = 0x7FFF;
            }
            out[k][i] = (uint32_t)ir;
            out[k][3 + i] = (uint32_t)mac[i];
        }
    }
    k = n - 1u;
    r[0] = v[k][0];
    r[1] = v[k][1];
    for(i = 0; i < 3; i++)
    {
        r[9 + i] = out[k][i];
        r[25 + i] = out[k][3 + i];
    }
    r[63] = flag;
    sh2o_zam_delegated++;
    return 1;
}

/* SH2ZAM's angle (65536 per turn) to ratan2's 4096 per turn, signed as ratan2 */
/* Contract tag zam-ratan2-1. Contract in sh2o/zam_experimental.h. */
int32_t sh2o_zam_ratan2(int32_t y,int32_t x)
{
    int32_t t = (int32_t)(((uint32_t)sh2z_atan2(y,x) + 8u) >> 4 & 4095u);
    return t > 0x800 || (t == 0x800 && y < 0) ? t - 0x1000 : t;
}

/* Contract tag zam-transposematrix-1. Contract in sh2o/zam_experimental.h. */
sh2o_psyq_matrix_t *sh2o_zam_TransposeMatrix(const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1)
{
    sh2z_mat33_t a,b;
    unsigned i,j;
    for(i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
            a.m[i][j] = m0->m[i][j];
    sh2z_mat33_transpose(&b,&a);
#if SH2O_MUTANT == 4
    sh2z_mat33_transpose(&b,&b);
#endif
    for(i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
            m1->m[i][j] = (int16_t)b.m[i][j]; /* an int16 copied, never changed */
    return m1;
}

static int sh2oi_zam_fits(int64_t v)
{
#if SH2O_MUTANT == 5
    (void)v;
    return 1;
#else
    return v >= INT32_MIN && v <= INT32_MAX;
#endif
}

/* Contract tag zam-setfognearfar-1. Contract in sh2o/zam_experimental.h. */
int sh2o_zam_SetFogNearFar(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h)
{
    int32_t d = (int32_t)((uint32_t)b - (uint32_t)a),q1,q2,q3;
    if(d < 100 || h == 0 || !sh2oi_zam_fits(-(int64_t)a * b) || !sh2oi_zam_fits((int64_t)b * 4096))
        return 0;
    q1 = sh2z_muldiv(a,b,-d); /* trunc(-ab / d); -d fits: d >= 100 */
    if(!sh2oi_zam_fits((int64_t)q1 * 256) || (h == -1 && (int64_t)q1 * 256 == INT32_MIN))
        return 0;
    q2 = sh2z_muldiv(b,4096,d);
    q3 = sh2z_muldiv(q1,256,h);
    if(q3 < -0x8000)
        q3 = -0x8000;
    if(q3 > 0x7FFF)
        q3 = 0x7FFF;
    sh2o_gte_write(g,59,(uint32_t)q3);
    sh2o_gte_write(g,60,(uint32_t)q2 << 12);
    return 1;
}
#endif /* SH2O_ZAM */
