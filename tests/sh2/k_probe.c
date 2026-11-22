/* SH-2 static probe of the k family, built by scripts/sh2_cross.sh (make cross).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
/* Each probe function wraps one kernel, the C reference (_c), the MAC.W form (_w) or its SR.S-checked form (_w2);
 * tests/sh2/k_static.txt checks its code. */
#include "sh2o/sh2o.h"
int64_t probe_c(const int16_t *m,const int16_t *v,int32_t tr);
int64_t probe_w(const int16_t *m,const int16_t *v,int32_t tr);
int64_t probe_c(const int16_t *m,const int16_t *v,int32_t tr) { return sh2o_k_dot3b(m,v,tr); }
int64_t probe_w(const int16_t *m,const int16_t *v,int32_t tr) { return sh2o_k_dot3b_w(m,v,tr); }
void probe_mvb3_c(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o);
void probe_mvb3_w(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o);
void probe_mvb3_c(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o) { sh2o_k_mvb3_c(w,v,t,o); }
void probe_mvb3_w(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o) { sh2o_k_mvb3_w(w,v,t,o); }
void probe_gpf3_c(const uint32_t *w,int64_t *o);
void probe_gpf3_w(const uint32_t *w,int64_t *o);
void probe_gpf3_c(const uint32_t *w,int64_t *o) { sh2o_k_gpf3_c(w,o); }
void probe_gpf3_w(const uint32_t *w,int64_t *o) { sh2o_k_gpf3_w(w,o); }
void probe_gpl3_c(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o);
void probe_gpl3_w(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o);
void probe_gpl3_c(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o) { sh2o_k_gpl3_c(w,m,sf,o); }
void probe_gpl3_w(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o) { sh2o_k_gpl3_w(w,m,sf,o); }
int64_t probe_w2(const int16_t *m,const int16_t *v,int32_t tr);
int64_t probe_w2(const int16_t *m,const int16_t *v,int32_t tr) { return sh2o_k_dot3b_w2(m,v,tr); }
void probe_mvb3_w2(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o);
void probe_mvb3_w2(const uint32_t *w,const int16_t *v,const int32_t *t,int64_t *o) { sh2o_k_mvb3_w2(w,v,t,o); }
void probe_gpf3_w2(const uint32_t *w,int64_t *o);
void probe_gpf3_w2(const uint32_t *w,int64_t *o) { sh2o_k_gpf3_w2(w,o); }
void probe_gpl3_w2(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o);
void probe_gpl3_w2(const uint32_t *w,const uint32_t *m,unsigned sf,int64_t *o) { sh2o_k_gpl3_w2(w,m,sf,o); }
