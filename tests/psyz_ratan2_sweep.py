#!/usr/bin/env python3
"""Sweep sh2o_psyq_ratan2 against the psyz ratan2.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/psyz_ratan2_sweep.py <cc> <source dir> <work dir>

CTest gives the arguments (CMakeLists.txt, test psyz_ratan2_sweep, label
proof). The script compiles the psyz ratan.c in place, unchanged, with
-Dratan2=p_ratan2, then links tests/psyz_ratan2_driver.c with each src/ C file
and runs `sweep`. Each of the sets grid, xorshift and edges must have 0
differences and compared + excluded = pairs; grid has 4097 x 4097 pairs and 0
excluded, xorshift 2^20 pairs, edges 169 pairs, 144 compared, 25 excluded and
2 breaks. Then the mutant: a copy of include/ and src/ in the work dir where
the mask 0x7FE00000 of the y branch is 0x7FF00000 must give grid 0, xorshift
0 and edges 6 differences. The work dir is removed at the end.
Exit code 0: all counts hold; 77 when the psyz checkout is not there, but 1
when SH2O_REQUIRE_PSYZ is 1.
"""
import glob
import os
import shutil
import subprocess
import sys
import time

OLD = "(uint32_t)y & 0x7FE00000u ?"
NEW = "(uint32_t)y & 0x7FF00000u ?"
WANT = {"grid": {"pairs": 4097 * 4097, "excluded": 0, "differences": 0},
        "xorshift": {"pairs": 1 << 20, "differences": 0},
        "edges": {"pairs": 169, "compared": 144, "excluded": 25, "breaks": 2, "differences": 0}}
MUTANT = {"grid": 0, "xorshift": 0, "edges": 6}
TOTAL = 17834154


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("psyz_ratan2_sweep: %s failed:\n%s%s" % (command[0], result.stdout, result.stderr))
    return result.stdout


def build(cc, root, driver, reference, exe):
    """The driver linked with the src/ C files of root and the reference object."""
    run([cc, "-std=c11", "-Wall", "-Wextra", "-pedantic", "-O2", "-Werror",
         "-I", os.path.join(root, "include"), "-I", os.path.join(root, "src"),
         *sorted(glob.glob(os.path.join(root, "src", "*.c"))), driver, reference, "-o", exe])


def sweep(exe):
    """{set: {key: count}} of one sweep run, and its time in seconds."""
    start = time.monotonic()
    out = run([exe, "sweep"])
    seconds = time.monotonic() - start
    sets = {}
    for line in out.splitlines():
        words = line.split()
        sets[words[0]] = {words[i + 1]: int(words[i]) for i in range(1, len(words) - 1, 2)}
    return sets, seconds


def main(argv):
    if len(argv) != 4:
        sys.exit(__doc__)
    cc, source, work = argv[1:]
    psyz = os.path.join(source, "external", "psyz")
    ratan_c = os.path.join(psyz, "decomp", "src", "libgte", "ratan.c")
    if not os.path.exists(ratan_c):
        if os.environ.get("SH2O_REQUIRE_PSYZ") == "1":
            print("psyz_ratan2_sweep: required (SH2O_REQUIRE_PSYZ=1), %s is not checked out" % ratan_c)
            return 1
        print("psyz_ratan2_sweep: skipped, %s is not checked out" % ratan_c)
        return 77
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    try:
        return check(cc, source, work, psyz, ratan_c)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def check(cc, source, work, psyz, ratan_c):
    driver = os.path.join(source, "tests", "psyz_ratan2_driver.c")
    reference = os.path.join(work, "p_ratan.o")
    run([cc, "-std=c11", "-O2", "-w", "-Dratan2=p_ratan2", "-I", os.path.join(psyz, "psyz", "include"),
         "-c", ratan_c, "-o", reference])
    fail = 0
    exe = os.path.join(work, "drv")
    build(cc, source, driver, reference, exe)
    sets, seconds = sweep(exe)
    total = 0
    for name in WANT:
        got = sets.get(name, {})
        print("psyz_ratan2_sweep: %s %s" % (name, " ".join("%s %d" % (k, v) for k, v in got.items())))
        total += got.get("pairs", 0)
        for key, want in WANT[name].items():
            if got.get(key) != want:
                print("FAIL %s %s: got %s, want %d" % (name, key, got.get(key), want))
                fail = 1
        if got.get("compared", 0) + got.get("excluded", 0) != got.get("pairs"):
            print("FAIL %s: compared + excluded != pairs" % name)
            fail = 1
    if total != TOTAL:
        print("FAIL total pairs: got %d, want %d" % (total, TOTAL))
        fail = 1
    print("psyz_ratan2_sweep: %d pairs, sweep time %.2f s" % (total, seconds))
    mutant = os.path.join(work, "mutant")
    shutil.copytree(os.path.join(source, "include"), os.path.join(mutant, "include"))
    shutil.copytree(os.path.join(source, "src"), os.path.join(mutant, "src"))
    site = os.path.join(mutant, "src", "sh2o_psyq.c")
    with open(site, encoding="utf-8") as handle:
        text = handle.read()
    if text.count(OLD) != 1:
        print("FAIL mutant site: %d occurrences of '%s', not 1" % (text.count(OLD), OLD))
        return 1
    with open(site, "w", encoding="utf-8") as handle:
        handle.write(text.replace(OLD, NEW))
    build(cc, mutant, driver, reference, os.path.join(work, "drv_mutant"))
    sets, seconds = sweep(os.path.join(work, "drv_mutant"))
    got = {name: sets.get(name, {}).get("differences") for name in MUTANT}
    print("psyz_ratan2_sweep: mutant mask 0x7FF00000: %s differences (sweep time %.2f s)"
          % (" ".join("%s %s" % item for item in got.items()), seconds))
    if got != MUTANT:
        print("FAIL mutant: want %s" % " ".join("%s %d" % item for item in MUTANT.items()))
        fail = 1
    print("psyz_ratan2_sweep: %s" % ("FAIL" if fail else "PASS"))
    return fail


if __name__ == "__main__":
    sys.exit(main(sys.argv))
