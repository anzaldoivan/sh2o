#!/usr/bin/env python3
"""Check ratan2's arctangent table against its formula.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/psyq_table_check.py

The script computes the table with scripts/psyq_tables.py, checks that it has
1,025 entries, each floor(atan((8i + 3) / 8192) * 2048 / pi + 1/2), and that
it equals, entry for entry, the literal sh2oi_ratan_tbl parsed from
src/sh2o_psyq.c. Exit code 0: the same 1,025 entries.
"""
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import psyq_tables  # noqa: E402

COUNT = 1025
LITERAL = re.compile(r"static const int16_t sh2oi_ratan_tbl\[(\d+)\](?: SH2OI_RODATA\(\w+\))? = \{([^}]*)\};")


def main():
    table = psyq_tables.ratan_tbl()
    if len(table) != COUNT:
        sys.exit("psyq_table: the formula gives %d entries, not %d" % (len(table), COUNT))
    for i, value in enumerate(table):
        if value != math.floor(math.atan((8 * i + 3) / 8192) * 2048 / math.pi + 0.5):
            sys.exit("psyq_table: entry %d is %d, not the formula's value" % (i, value))
    with open(os.path.join(ROOT, "src", "sh2o_psyq.c"), encoding="utf-8") as handle:
        match = LITERAL.search(handle.read())
    if not match:
        sys.exit("psyq_table: no sh2oi_ratan_tbl literal in src/sh2o_psyq.c")
    source = [int(v) for v in match.group(2).replace("\n", " ").split(",") if v.strip()]
    if int(match.group(1)) != COUNT or len(source) != COUNT:
        sys.exit("psyq_table: the literal declares %s and holds %d entries, not %d"
                 % (match.group(1), len(source), COUNT))
    for i, (want, got) in enumerate(zip(table, source)):
        if want != got:
            sys.exit("psyq_table: entry %d: src/sh2o_psyq.c %d, formula %d" % (i, got, want))
    print("psyq_table: %d entries equal the formula and src/sh2o_psyq.c" % COUNT)


if __name__ == "__main__":
    main()
