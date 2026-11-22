#!/usr/bin/env python3
"""Compare the bulk dump of the k test with the Python model.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/k_bulk_check.py <test_k executable>

The script makes the 113,471 bulk vectors itself, computes each result with
the model in scripts/vectors_k.py, and compares each line of
'<test_k> --dump' with the line it expects. Then it computes the six pinned
digests from the model and finds each one in tests/suite/sh2o_k_test.h.
Exit code 0: zero differences and the same six digests.
"""
import itertools
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import vectors_k as model  # noqa: E402

COUNT = 113471
PINS = {"DOT3B": 0x61683DE4, "MVB3": 0x9D49BB58, "GPF3": 0x61BC75C3, "GPL3": 0xCDB04460,
        "GPF3_EDGE": 0x7C2BE3B8, "GPL3_EDGE": 0xAACA9F5F}


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    vectors = list(model.bulk_vectors())
    if len(vectors) != COUNT or model.EDGE_COUNT != 109375 or model.RANDOM_COUNT != 4096:
        sys.exit("k_bulk_check: the model makes %d vectors, not %d" % (len(vectors), COUNT))
    results = []
    for i, (m, v, tr) in enumerate(vectors):
        w, mv, t = model.bulk_mvb3_inputs(m, v, tr)
        g, mac, sf = model.bulk_gp3_inputs(i, m, v, tr)
        results.append((model.dot3b(m, v, tr), model.mvb3(w, mv, t), model.gpf3(g), model.gpl3(g, mac, sf)))
    dumped = subprocess.run([argv[1], "--dump"], capture_output=True, text=True, check=True).stdout.splitlines()
    first, bad = None, 0
    for i in range(max(len(dumped), COUNT)):
        if i < COUNT:
            m, v, tr = vectors[i]
            d, mv, gf, gl = results[i]
            want = " ".join(str(x) for x in (i, *m, *v, tr, d, *mv, *gf, *gl))
        else:
            want = None
        got = dumped[i] if i < len(dumped) else None
        if got != want:
            bad += 1
            if first is None:
                first = (i, want, got)
    print("k_bulk_check: %d vectors, %d dump lines, %d differences" % (COUNT, len(dumped), bad))
    if first:
        print("  first difference, vector %d\n  expected %s\n  got      %s" % first)
    digests = {
        "DOT3B": model.digest(r[0] for r in results),
        "MVB3": model.digest(itertools.chain.from_iterable(r[1] for r in results)),
        "GPF3": model.digest(itertools.chain.from_iterable(r[2] for r in results)),
        "GPL3": model.digest(itertools.chain.from_iterable(r[3] for r in results)),
        "GPF3_EDGE": model.digest(itertools.chain.from_iterable(r[2] for r in results[:model.EDGE_TR0_COUNT])),
        "GPL3_EDGE": model.digest(itertools.chain.from_iterable(r[3] for r in results[:model.EDGE_TR0_COUNT])),
    }
    with open(os.path.join(ROOT, "tests", "suite", "sh2o_k_test.h"), encoding="utf-8") as handle:
        header = handle.read()
    failed = bad != 0
    for name, value in digests.items():
        pinned = "#define SH2O_DIGEST_%s 0x%08Xu" % (name, PINS[name])
        ok = value == PINS[name] and pinned in header
        failed = failed or not ok
        print("k_bulk_check: %s digest 0x%08X, pinned 0x%08X, in the header: %s, %s"
              % (name, value, PINS[name], "yes" if pinned in header else "no", "ok" if ok else "FAIL"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
