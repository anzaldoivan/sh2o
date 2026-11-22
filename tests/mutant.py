#!/usr/bin/env python3
"""Make a mutant copy of src/ and include/ for the oracle test of a family.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/mutant.py <family> <name> <root> <output folder>

The script copies the src and include folders of root to the output folder
and plants one known bug in the copy of one file. The old text must occur
exactly one time; otherwise the script stops with an error and the build
fails. CMakeLists.txt builds the copied src/*.c with tests/test_<family>.c and
its suite, the copied include/ first, and runs it with --mutant <name>: the
test passes only when an oracle vector differs (a psyq mutant: when the psyq
suite returns non-zero). A UNR mutant instead runs through
tests/unr_check.py mutant; a gte_entry mutant runs test_gte --mutant-entry. The source tree does not change.
"""
import os
import shutil
import sys

K_SW = "include/sh2o/inline/sw/k_sw.inl.h"
GTE_INTERNAL = "src/sh2o_gte_internal.h"
GTE_C = "src/sh2o_gte.c"
PSYQ_C = "src/sh2o_psyq.c"
LIST_C = "src/sh2o_list.c"
# family: {name: (path under root, old text, new text)}
MUTANTS = {
    "k": {
        "shift_11": (K_SW, "(uint32_t)tr << 12", "(uint32_t)tr << 11"),
        "shift_13": (K_SW, "(uint32_t)tr << 12", "(uint32_t)tr << 13"),
        "dropped_sign_extension": (K_SW, " | (sx << 12);", ";"),
        "swapped_index": (K_SW, "(int32_t)m[2] * v[2]", "(int32_t)m[2] * v[1]"),
        "lost_carry": (K_SW, "(uint32_t)(p >> 31) + c;", "(uint32_t)(p >> 31);"),
    },
    "gte": {
        "shift_11": (GTE_INTERNAL, "(uint32_t)t << 12);", "(uint32_t)t << 11);"),
        "shift_13": (GTE_INTERNAL, "(uint32_t)t << 12);", "(uint32_t)t << 13);"),
        "clamp_off_by_one": (GTE_C, "p = p < -0x400 ? -0x400 : 0x3FF;", "p = p < -0x400 ? -0x400 : 0x3FE;"),
        "unr_index_off_by_one": (K_SW, "unr[(d - 0x7FC0u) >> 7]", "unr[((d - 0x7FC0u) >> 7) - (d >= 0x8040u)]"),
        "swapped_flag_bit": (GTE_C, "cx->flag |= i ? 1u << 13 : 1u << 14;", "cx->flag |= i ? 1u << 14 : 1u << 13;"),
        "dropped_sxy_push": (GTE_C, "    g->r[12] = g->r[13];\n    g->r[13] = g->r[14];\n", ""),
        "dropped_sz_push": (GTE_INTERNAL, "    g->r[16] = g->r[17];\n", ""),
        "dropped_colour_push": (GTE_INTERNAL, "    g->r[20] = g->r[21];\n", ""),
        "dpct_reads_rgbc": (GTE_C, "        c = g->r[20];", "        c = g->r[6];"),
        "dpct_fifo_not_advanced": (GTE_C, "    for(n = 0; n < 3; n++)\n    {\n        c = g->r[20];",
                                   "    c = g->r[20];\n    for(n = 0; n < 3; n++)\n    {"),
    },
    # the psyq bodies: each mutant changes one body; the psyq suite must return non-zero
    "psyq": {
        "back_colour_shift": (PSYQ_C, "sh2o_gte_write(g,46,(uint32_t)gbk << 4);",
                              "sh2o_gte_write(g,46,(uint32_t)gbk << 3);"),
        "rot_matrix_swapped": (PSYQ_C, "r[1] = SH2OI_W(m->m[0][2],m->m[1][0]);",
                               "r[1] = SH2OI_W(m->m[1][0],m->m[0][2]);"),
        "transpose_dropped_store": (PSYQ_C, "    m1->m[1][1] = SH2OI_LO(r11);" + " " * 29 + "/* sh r11, 8 */\n", ""),
        "rot_trans_no_sf": (PSYQ_C, "0x0480012u); /* MVMVA sf 1, RT, V0, TR */", "0x0400012u);"),
        "pop_matrix_bit": (PSYQ_C, "sh2o_gte_write(g,32 + k,e[k]);", "sh2o_gte_write(g,32 + k,e[k] ^ 1u);"),
        "fog_no_break_hook": (PSYQ_C, "            sh2o_psyq_break(code);", "            (void)code;"),
        "ratan_index_off_by_one": (PSYQ_C, "t = 0x400 - sh2oi_ratan_at(q);", "t = 0x400 - sh2oi_ratan_at(q + 1);"),
        "init_geom_screen": (PSYQ_C, "sh2o_gte_write(g,58,0x3E8u);", "sh2o_gte_write(g,58,0x3E9u);"),
        "apply_matrix_swapped": (PSYQ_C, "v1->vy = y;", "v1->vy = x;"),
        "apply_matrix_lv_sign": (PSYQ_C, "v1->vz = (int32_t)(b3 + a3);", "v1->vz = (int32_t)(b3 - a3);"),
        "comp_matrix_sign": (PSYQ_C, "t1 += (uint32_t)m0->t[1]", "t1 -= (uint32_t)m0->t[1]"),
        "push_matrix_top": (PSYQ_C, "s->top = top + 32;", "s->top = top + 31;"),
        "colour_matrix_base": (PSYQ_C, "sh2oi_set3(g,48,m);", "sh2oi_set3(g,49,m);"),
        "far_colour_shift": (PSYQ_C, "sh2o_gte_write(g,55,(uint32_t)bfc << 4);",
                             "sh2o_gte_write(g,55,(uint32_t)bfc << 3);"),
        "geom_offset_shift": (PSYQ_C, "sh2o_gte_write(g,57,(uint32_t)ofy << 16);",
                              "sh2o_gte_write(g,57,(uint32_t)ofy << 15);"),
        "geom_screen_plus_one": (PSYQ_C, "sh2o_gte_write(g,58,(uint32_t)h);",
                                 "sh2o_gte_write(g,58,(uint32_t)h + 1u);"),
        "light_matrix_base": (PSYQ_C, "sh2oi_set3(g,40,m);", "sh2oi_set3(g,41,m);"),
        "trans_matrix_swapped": (PSYQ_C, "sh2o_gte_write(g,38,(uint32_t)m->t[1]);",
                                 "sh2o_gte_write(g,38,(uint32_t)m->t[2]);"),
        "trans_matrix_store": (PSYQ_C, "m->t[1] = y;", "m->t[1] = z;"),
    },
    # the MVMVA list: each mutant changes one line or block of the list; an oracle vector must differ
    "list": {
        "item_dropped": (LIST_C, "for(k = 0; k < n; k++)", "for(k = 0; k + 1 < n; k++)"),
        "ir_not_sign_extended": (LIST_C, "out[k][1] = (uint32_t)(int32_t)(int16_t)(uint16_t)(r[10] & 0xFFFFu);",
                                 "out[k][1] = r[10] & 0xFFFFu;"),
        "mac_wrong_register": (LIST_C, "out[k][4] = r[26];", "out[k][4] = r[25];"),
        "rows_swapped": (LIST_C, "    }\n}\n", "        if(k & 1u)\n            for(unsigned c = 0; c < 6; c++) {\n"
                         "                uint32_t t = out[k][c];\n                out[k][c] = out[k - 1][c];\n"
                         "                out[k - 1][c] = t;\n            }\n    }\n}\n"),
        "vz0_not_loaded": (LIST_C, "        r[1] = v[k][1];\n", ""),
    },
    # the UNR mutants: the gte program, but not in MUTANTS["gte"] (tests/gte_sources_check.py pins a count for each
    # of those); tests/unr_check.py mutant must catch each one
    "gte_unr": {
        "clz32_bound_off_by_one": (K_SW, "if(v < 0x1000000u)", "if(v <= 0x1000000u)"),
        "unr_recip_rounding": (K_SW, "return (0x80u + r * u) >> 8;", "return (0x7Fu + r * u) >> 8;"),
        "unr_div_clamp_bound": (K_SW, "q > 0x1FFFFu ? 0x1FFFFu : q", "q > 0xFFFFu ? 0xFFFFu : q"),
    },
    # the fixed-word entries: the gte program, not in MUTANTS["gte"]; test_gte --mutant-entry must find a difference
    "gte_entry": {
        "nrtps_sf0": (GTE_C, "sh2o_gte_rtps(g,0x4A180001u);", "sh2o_gte_rtps(g,0x4A100001u);"),
        "nrtpt_lm1": (GTE_C, "sh2o_gte_rtpt(g,0x4A280030u);", "sh2o_gte_rtpt(g,0x4A280430u);"),
    },
}


def main(argv):
    if len(argv) != 5 or argv[1] not in MUTANTS or argv[2] not in MUTANTS[argv[1]]:
        sys.exit(__doc__ + "\nmutants: " + "; ".join("%s: %s" % (f, " ".join(m)) for f, m in MUTANTS.items()))
    family, name, root, output = argv[1:]
    path, old, new = MUTANTS[family][name]
    for folder in ("src", "include"):
        # shutil.copy: new modification times, so the build sees the copy as new
        shutil.copytree(os.path.join(root, folder), os.path.join(output, folder), dirs_exist_ok=True,
                        copy_function=shutil.copy)
    with open(os.path.join(root, path), encoding="utf-8") as handle:
        text = handle.read()
    count = text.count(old)
    if count != 1:
        sys.exit("mutant: %s %s: the old text %r occurs %d times in %s, not 1"
                 % (family, name, old, count, os.path.join(root, path)))
    with open(os.path.join(output, path), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text.replace(old, new))


if __name__ == "__main__":
    main(sys.argv)
