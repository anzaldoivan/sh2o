#!/usr/bin/env python3
"""Check the UNR divide of the k kernels against the Python GTE model.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/unr_check.py recip <test_gte>
       python3 tests/unr_check.py sweep <test_gte>
       python3 tests/unr_check.py mutant <name> <test_gte mutant>

- recip: each of the 65,535 rows of test_gte --unr (SZ 1..FFFFh) must equal model_recip(sz), and the unr[0x101]
  table of include/sh2o/inline/sw/k_sw.inl.h must equal UNR of scripts/gte_model.py.
- sweep: test_gte --unr-sweep must compare 3,221,192,704 pairs (each SZ 1..FFFFh, h 0..min(2 sz - 1, FFFFh))
  with no difference.
- mutant: the three checks in order (the oracle vectors of test_gte --mutant <name>, recip, sweep) on a mutant
  program. It stops at the first check that catches the mutant and prints it. It passes only when one catches it.
The script exits 1 on a failure.
"""
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
from gte_model import UNR  # noqa: E402

K_SW = ROOT / "include/sh2o/inline/sw/k_sw.inl.h"
ROWS = 0xFFFF
PAIRS = 3221192704  # sum of min(2 sz, 10000h) over SZ 1..FFFFh


def model_recip(sz):
    """(z, recip): the UNR reciprocal of sz << z, z the shift that makes it 8000h..FFFFh."""
    z = 16 - sz.bit_length()
    d = sz << z
    u = UNR[(d - 0x7FC0) >> 7] + 0x101
    d = (0x2000080 - d * u) >> 8
    return z, (0x80 + d * u) >> 8


def recip(exe):
    """Return None when every row equals the model, else the first problem."""
    found = re.search(r"static const uint8_t unr\[0x101\](?: SH2OI_RODATA\(\w+\))? = \{(.*?)\};", K_SW.read_text(encoding="utf-8"), re.DOTALL)
    if not found:
        return "no unr[0x101] table in %s" % K_SW
    if bytes(int(t, 16) for t in re.findall(r"0x[0-9A-F]{2}", found.group(1))) != UNR:
        return "the unr[0x101] table of %s differs from UNR of scripts/gte_model.py" % K_SW
    run = subprocess.run([exe, "--unr"], capture_output=True, text=True, check=False)
    if run.returncode != 0:
        return "%s --unr: exit code %d" % (exe, run.returncode)
    rows = run.stdout.splitlines()
    if len(rows) != ROWS:
        return "%s --unr: %d rows, expected %d" % (exe, len(rows), ROWS)
    for sz, row in enumerate(rows, 1):
        want = "%d %d %d" % ((sz,) + model_recip(sz))
        if row != want:
            return "%s --unr: row %r, expected %r" % (exe, row, want)
    print("unr: recip: %d rows equal the model; unr[0x101] equals UNR" % ROWS)
    return None


def sweep(exe):
    """Return None when the sweep has all pairs and no difference, else the problem."""
    assert PAIRS == sum(min(2 * sz, 0x10000) for sz in range(1, 0x10000))
    run = subprocess.run([exe, "--unr-sweep"], capture_output=True, text=True, check=False)
    sys.stderr.write(run.stderr)
    found = re.fullmatch(r"unr-sweep (\d+) (\d+)\n", run.stdout)
    if not found:
        return "%s --unr-sweep: exit code %d, output %r" % (exe, run.returncode, run.stdout)
    pairs, bad = int(found.group(1)), int(found.group(2))
    if bad != 0 or run.returncode != 0:
        return "%s --unr-sweep: %d differences after %d pairs" % (exe, bad, pairs)
    if pairs != PAIRS:
        return "%s --unr-sweep: %d pairs, expected %d" % (exe, pairs, PAIRS)
    print("unr: sweep: %d pairs, 0 differences" % pairs)
    return None


def vectors(name, exe):
    """Return None when no oracle vector differs, else the count."""
    run = subprocess.run([exe, "--mutant", name], capture_output=True, text=True, check=False)
    found = re.search(r": (\d+) differences in", run.stdout)
    if not found:
        return "%s --mutant %s: exit code %d, output %r" % (exe, name, run.returncode, run.stdout)
    return None if found.group(1) == "0" else "%s differing oracle vectors" % found.group(1)


def main(argv):
    if len(argv) == 3 and argv[1] in ("recip", "sweep"):
        start = time.monotonic()
        problem = (recip if argv[1] == "recip" else sweep)(argv[2])
        if problem:
            sys.exit("unr: FAIL: " + problem)
        print("unr: %s: %.1f s" % (argv[1], time.monotonic() - start))
        return
    if len(argv) == 4 and argv[1] == "mutant":
        name, exe = argv[2], argv[3]
        for check, run in (("vectors", lambda: vectors(name, exe)), ("recip", lambda: recip(exe)),
                           ("sweep", lambda: sweep(exe))):
            start = time.monotonic()
            problem = run()
            if problem:
                print("unr: mutant %s: caught by %s in %.1f s: %s" % (name, check, time.monotonic() - start, problem))
                return
            print("unr: mutant %s: not caught by %s (%.1f s)" % (name, check, time.monotonic() - start))
        sys.exit("unr: FAIL: no check catches the mutant %s" % name)
    sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
