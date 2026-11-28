#!/usr/bin/env python3
"""Check ratan2's arctangent table against the psyz table.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/psyz_table_check.py

The script parses `short ratan_tbl[] = {...};` from the psyz checkout
(external/psyz/decomp/src/libgte/ratan.c, read in place) and checks that it
has 1,028 entries at the pin, that entries 0..1024 equal the literal
sh2oi_ratan_tbl of src/sh2o_psyq.c and the formula of scripts/psyq_tables.py,
and that entries 1025..1027 are padding: 0, not the formula's value, and past
the largest index that ratan2 reads (1024). It prints the 1,025 entries as
little-endian int16 bytes and their sha256. It records the psyz tables SQRT
(sqrtbl.c, 192 entries) and rsin_tbl (sincos.c, 1,028 entries) as present in
psyz and absent from src/ and include/. Then the plants: one changed entry in
a copy of the psyz text and in a copy of the sh2o text must each make the
comparison fail. Exit code 0: all checks hold and each plant is caught; 77
when the psyz checkout is not there, but 1 when SH2O_REQUIRE_PSYZ is 1.
"""
import hashlib
import math
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
import psyq_tables  # noqa: E402
from psyq_table_check import LITERAL  # noqa: E402

LIBGTE = os.path.join(ROOT, "external", "psyz", "decomp", "src", "libgte")
COUNT = 1025
PIN_COUNT = 1028
OTHERS = (("SQRT", "sqrtbl.c", 192), ("rsin_tbl", "sincos.c", 1028))


def psyz_table(text, name):
    """The entries of `short <name>[] = {...};` in text, or None."""
    match = re.search(r"short %s\[\] = \{([^}]*)\};" % re.escape(name), text)
    if not match:
        return None
    return [int(v.strip(), 0) for v in match.group(1).split(",") if v.strip()]


def sh2o_table(text):
    """The entries of the literal sh2oi_ratan_tbl in text, or None."""
    match = LITERAL.search(text)
    if not match:
        return None
    return [int(v) for v in match.group(2).replace("\n", " ").split(",") if v.strip()]


def compare(psyz_text, sh2o_text, formula):
    """The problems of the psyz ratan_tbl against the sh2o literal and the formula."""
    psyz = psyz_table(psyz_text, "ratan_tbl")
    sh2o = sh2o_table(sh2o_text)
    if psyz is None:
        return ["no ratan_tbl in the psyz ratan.c"]
    if sh2o is None:
        return ["no sh2oi_ratan_tbl literal in src/sh2o_psyq.c"]
    problems = []
    if len(psyz) != PIN_COUNT:
        problems.append("psyz ratan_tbl has %d entries, not %d" % (len(psyz), PIN_COUNT))
    if len(sh2o) != COUNT or len(formula) != COUNT:
        problems.append("sh2o %d entries, formula %d, not %d" % (len(sh2o), len(formula), COUNT))
    for i in range(min(COUNT, len(psyz), len(sh2o), len(formula))):
        if not psyz[i] == sh2o[i] == formula[i]:
            problems.append("entry %d: psyz %d, sh2o %d, formula %d" % (i, psyz[i], sh2o[i], formula[i]))
    return problems


def plant(text, pattern):
    """text with the entry 512 of the table that pattern matches made one larger."""
    match = re.search(pattern, text)
    items = match.group(1).split(",")
    stripped = items[512].strip()
    items[512] = items[512].replace(stripped, str(int(stripped, 0) + 1))
    return text[:match.start(1)] + ",".join(items) + text[match.end(1):]


def main():
    ratan_c = os.path.join(LIBGTE, "ratan.c")
    if not os.path.exists(ratan_c):
        if os.environ.get("SH2O_REQUIRE_PSYZ") == "1":
            print("psyz_table: required (SH2O_REQUIRE_PSYZ=1), %s is not checked out" % ratan_c)
            return 1
        print("psyz_table: skipped, %s is not checked out" % ratan_c)
        return 77
    with open(ratan_c, encoding="utf-8") as handle:
        psyz_text = handle.read()
    with open(os.path.join(ROOT, "src", "sh2o_psyq.c"), encoding="utf-8") as handle:
        sh2o_text = handle.read()
    formula = psyq_tables.ratan_tbl()
    fail = 0
    problems = compare(psyz_text, sh2o_text, formula)
    for problem in problems:
        print("FAIL psyz_table: %s" % problem)
        fail = 1
    psyz = psyz_table(psyz_text, "ratan_tbl")
    if not problems:
        print("psyz_table: %d entries, 0..%d equal src/sh2o_psyq.c and the formula" % (len(psyz), COUNT - 1))
    for i in range(COUNT, len(psyz)):
        value = psyz[i]
        want = math.floor(math.atan((8 * i + 3) / 8192) * 2048 / math.pi + 0.5)
        ok = value == 0 and value != want and i > COUNT - 1
        print("psyz_table: padding entry %d = %d (formula %d)%s" % (i, value, want, "" if ok else " FAIL"))
        fail |= not ok
    data = b"".join(struct.pack("<h", v) for v in psyz[:COUNT])
    print("psyz_table: entries 0..%d as int16 LE: %s" % (COUNT - 1, data.hex()))
    print("psyz_table: sha256 %s" % hashlib.sha256(data).hexdigest())
    own = ""
    for top in ("src", "include"):
        for root, dirs, files in os.walk(os.path.join(ROOT, top)):
            dirs.sort()
            for name in sorted(files):
                with open(os.path.join(root, name), encoding="utf-8", errors="replace") as handle:
                    own += handle.read()
    for name, path, count in OTHERS:
        with open(os.path.join(LIBGTE, path), encoding="utf-8") as handle:
            table = psyz_table(handle.read(), name)
        found = len(table) if table is not None else 0
        absent = not re.search(r"\b%s\b" % name, own)
        ok = found == count and absent
        print("psyz_table: %s: %d entries in psyz %s, %s in src/ and include/%s"
              % (name, found, path, "absent" if absent else "PRESENT", "" if ok else " FAIL"))
        fail |= not ok
    plants = (("psyz", plant(psyz_text, r"short ratan_tbl\[\] = \{([^}]*)\};"), sh2o_text),
              ("sh2o", psyz_text, plant(sh2o_text, r"static const int16_t sh2oi_ratan_tbl\[\d+\](?: SH2OI_RODATA\(\w+\))? = \{([^}]*)\};")))
    for name, planted_psyz, planted_sh2o in plants:
        caught = bool(compare(planted_psyz, planted_sh2o, formula))
        print("psyz_table: plant %s entry 512: %s" % (name, "caught" if caught else "NOT caught"))
        fail |= not caught
    print("psyz_table: %s" % ("FAIL" if fail else "PASS"))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
