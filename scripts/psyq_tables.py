#!/usr/bin/env python3
"""ratan2's arctangent table of src/sh2o_psyq.c, from its formula.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/psyq_tables.py

ratan_tbl() -> 1025 ints: tbl[i] = floor(atan((i + 3/8) / 1024) * 2048 / pi + 1/2), i 0..1024 (4096 units per
turn: tbl[1024] = 512 = 45 degrees). Every value lies within 0.4992 of its rounding point (margin 0.0008 from the
boundary), so any double-precision atan gives the same table. tests/psyq_table_check.py proves it equal to the
literal sh2oi_ratan_tbl in src/sh2o_psyq.c. Run as a script, it prints the body of that C literal (its source).
"""
import math


def ratan_tbl():
    return [math.floor(math.atan((8 * i + 3) / 8192) * 2048 / math.pi + 0.5) for i in range(1025)]


def c_body(vals, per=16):
    rows = [",".join(str(v) for v in vals[k:k + per]) for k in range(0, len(vals), per)]
    return ",\n".join("    " + r for r in rows)


if __name__ == "__main__":
    print(c_body(ratan_tbl()))
