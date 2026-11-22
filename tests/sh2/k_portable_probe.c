/* SH-2 static probe of the k family's portable build, built by scripts/sh2_cross.sh (make cross).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* One probe function per public k kernel; tests/sh2/k_portable_static.txt checks its code. */
#include "sh2o/sh2o.h"
int64_t probe_dot3b(const int16_t *m,const int16_t *v,int32_t tr);
void probe_mvb3(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o);
void probe_gpf3(const uint32_t *w,int64_t *o);
void probe_gpl3(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o);
unsigned probe_clz32(uint32_t v);
uint32_t probe_unr_recip(uint32_t d);
uint32_t probe_unr_div(uint32_t h,uint32_t sz);
int64_t probe_dot3b(const int16_t *m,const int16_t *v,int32_t tr) { return sh2o_k_dot3b(m,v,tr); }
void probe_mvb3(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o) { sh2o_k_mvb3(w,v,t,o); }
void probe_gpf3(const uint32_t *w,int64_t *o) { sh2o_k_gpf3(w,o); }
void probe_gpl3(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o) { sh2o_k_gpl3(w,m,sf,o); }
unsigned probe_clz32(uint32_t v) { return sh2o_k_clz32(v); }
uint32_t probe_unr_recip(uint32_t d) { return sh2o_k_unr_recip(d); }
uint32_t probe_unr_div(uint32_t h,uint32_t sz) { return sh2o_k_unr_div(h,sz); }
