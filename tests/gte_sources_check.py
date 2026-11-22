#!/usr/bin/env python3
"""Compare the GTE machine with the Python model over every case source, and the gte mutants.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage:
  python3 tests/gte_sources_check.py <gte program> <mutant program>...
  python3 tests/gte_sources_check.py --recordings <gte program> <mutant program>...

The programs are tests/test_gte.c builds, run with --driver: the test program, then the ten gte mutants in the
order of tests/mutant.py MUTANTS["gte"]. The model (scripts/gte_model.py) gives the expected 64 registers of each
case one time; the script then compares each program with it, one stdin line for each case.

Public sources: the edge cases and the LCG cases of scripts/vectors_gte.py (cases(); an id that starts with "lcg" is
an LCG case), written in the order ORDER of that script; the ps1-tests cases of tests/ps1-tests/gte_cases.txt, written
as ps1-tests writes them. The model input of a ps1-tests case is the read view of the storage state after the same
writes (vectors_gte.write). The counts, the sha256 of the case file, and the model against the NCCS and DPCT expected
values of ps1-tests are pinned. The test program must give 0 differences on all 64 registers. Each mutant must give
its pinned count of differing cases.

--recordings adds the recorded PS1 GTE command pairs of SH2O_PS1_GTE_RECORDINGS (docs/ops/INDEX.md gives the
layout). The private input check of tests/private_inputs.py comes first: a path in the repository gives exit code 1;
no variable or an absent path gives exit code 77 (CTest shows the test as skipped). The digest of the files read is
pinned, and the counts of the four sources and of each mutant.
The script exits 1 on the first failure.
"""
import hashlib
import json
import os
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import gte_model as gm  # noqa: E402
import vectors_gte as vg  # noqa: E402
import private_inputs  # noqa: E402
from mutant import MUTANTS  # noqa: E402

M32 = 0xFFFFFFFF
NCCS = 0x1B
DPCT = 0x2A
CASE_FILE = ROOT / "tests/ps1-tests/gte_cases.txt"
CASE_FILE_SHA256 = "069f396598fa0e7d3d8cd17b1128bc78ced2fa1714cd40ba07053a61b58249c1"
COUNTS = {"edges": 13898, "lcg": 61440, "ps1-tests": 1600}
# Expected-value checks of the model over ps1-tests: {name: (opcode, cases)}.
PS1_TESTS_CHECKED = {"NCCS": (NCCS, 100), "DPCT": (DPCT, 100)}
# Differing cases of each gte mutant over the public sources, in the order of MUTANTS["gte"].
PUBLIC_MUTANT_COUNTS = (13337, 12852, 5808, 31, 1088, 8833, 8372, 38781, 4540, 4198)
# ps1-tests write orders: test-all writes 0 to each register, then each value in register order; gte-fuzz writes
# each value in register order.
PS1_TESTS_ORDER = {"test-all": [(r, None) for r in range(64)] + list(range(64)), "gte-fuzz": list(range(64))}

VARIABLE = "SH2O_PS1_GTE_RECORDINGS"
RECORDINGS_DIGEST = "e5579fca72b121a3f8d4d0aee0e1c25d99c14850f1b9ecb850879cfd9e0f4eca"
CLEAN_PAIRS = 2864
ALL_CASES = 79802  # was 75261 before DPCT (the 4,541 public DPCT cases; the recordings hold no DPCT pair)
# Differing cases of each gte mutant over all 79,802 cases, in the order of MUTANTS["gte"]. shift_11 and shift_13
# differ from the figures that the record of the original port gives (35796 and 35314): a build of that port's own
# code at its baseline, with its own driver and flags and the same planted shift, also gives 10618 and 10463, and the
# recorded pairs add no differing case to either mutant. The other six counts equal that record. DPCT added
# differing cases only on DPCT cases: the non-DPCT part of each count is the pin before DPCT (10618, 10463, 5808, 85,
# 1088, 9057, 8591, 35204).
RECORDINGS_MUTANT_COUNTS = (13337, 12852, 5808, 85, 1088, 9057, 8591, 39745, 4540, 4198)
# A recorded pair: SXYP 15 first (its FIFO push is overwritten by the 12-14 writes after it), then ORDER.
PAIR_ORDER = (15,) + vg.ORDER


def fail(text):
    print(f"gte_sources: FAIL: {text}")
    sys.exit(1)


def ps1_tests_cases():
    """tests/ps1-tests/gte_cases.txt -> cases {source, id, cmd, writes, model_in, out}."""
    data = CASE_FILE.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != CASE_FILE_SHA256:
        fail(f"{CASE_FILE.relative_to(ROOT)}: sha256 {digest}, pinned {CASE_FILE_SHA256}")
    out = []
    for line in data.decode("ascii").splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        src, cmd, words = f[0], None if f[1] == "-" else int(f[1], 16), [int(w, 16) for w in f[2:]]
        inp = words[:64]
        writes = [(e[0], 0) if isinstance(e, tuple) else (e, inp[e]) for e in PS1_TESTS_ORDER[src]]
        s = [0] * 64
        for r, v in writes:
            vg.write(s, r, v)
        out.append({"source": "ps1-tests", "id": f"{src} line {len(out) + 1}", "cmd": cmd, "writes": writes,
                    "model_in": gm.read_view(s), "out": words[64:128] or None})
    return out


def public_sources():
    """{source: [case]} for edges, lcg and ps1-tests."""
    src = {"edges": [], "lcg": []}
    for c in vg.cases():
        src["lcg" if c["id"].startswith("lcg") else "edges"].append(
            {"source": "lcg" if c["id"].startswith("lcg") else "edges", "id": c["id"], "cmd": c["cmd"],
             "writes": [(r, c["in"][r]) for r in vg.ORDER], "model_in": c["in"]})
    src["ps1-tests"] = ps1_tests_cases()
    return src


def ps1_view(d, c):
    """MFC2/CFC2 view of a recorded raw CP2D/CP2C: sign-extend 1, 3, 5, 8, 9-11, zero-extend 7, 16-19, 15 = SXY2,
    28/29 from IR1-3; CP2C as recorded (the recorder stores the sign-extended 16-bit control registers)."""
    o = [w & M32 for w in list(d) + list(c)]
    for r in (1, 3, 5, 8, 9, 10, 11):
        o[r] = gm.s16(o[r]) & M32
    for r in (7, 16, 17, 18, 19):
        o[r] &= 0xFFFF
    o[15] = o[14]
    o[28] = o[29] = gm.orgb(o[9], o[10], o[11])
    return o


def cop2_write(w):
    """lwc2 rt / mtc2 rd / ctc2 rd -> GTE register 0-63, else None."""
    op, rs = w >> 26, w >> 21 & 31
    if op == 0x32:
        return w >> 16 & 31
    if op == 0x12 and rs in (4, 6):
        return (w >> 11 & 31) + (32 if rs == 6 else 0)
    return None


def recordings(base):
    """Every direct subfolder of base with sites.json and run/gte_states.jsonl -> (clean pair cases, identity digest).
    A pair is clean when no COP2 write is at site-8 or site-4 (the before state does not have that write) and its
    opcode is in MODELLED_ALL."""
    read = {}

    def load(path):
        data = path.read_bytes()
        read[str(path)] = hashlib.sha256(data).hexdigest()
        return data

    cases, folders = [], sorted(p for p in base.iterdir() if p.is_dir() and (p / "sites.json").is_file()
                                and (p / "run/gte_states.jsonl").is_file())
    if not folders:
        fail(f"{VARIABLE}: no recording folder")
    for folder in folders:
        sj = json.loads(load(folder / "sites.json"))
        exe = load(base / Path(sj["exe"]).name)
        if hashlib.sha1(exe).hexdigest() != sj["exe_sha1"]:
            fail(f"{folder.name}: the program file sha1 is not the sha1 of sites.json")
        t_addr = int.from_bytes(exe[0x18:0x1C], "little")         # PS-X EXE header; the text is at file 0x800

        def word(a):
            return int.from_bytes(exe[0x800 + a - t_addr:0x804 + a - t_addr], "little")
        sites = {int(s["addr"], 16): s for s in sj["sites"]}
        k = Counter()
        for line in load(folder / "run/gte_states.jsonl").decode("utf-8").splitlines():
            p = json.loads(line)
            s = sites[p["site"]]
            if word(p["site"]) != int(s["word"], 16):
                fail(f"{folder.name}: program word at {p['site']:#x} is not the word of sites.json")
            k[p["site"]] += 1
            cmd = int(s["word"], 16) & 0x1FFFFFF
            if cop2_write(word(p["site"] - 8)) is not None or cop2_write(word(p["site"] - 4)) is not None:
                continue
            if cmd & 0x3F not in gm.MODELLED_ALL:
                continue
            inp = ps1_view(p["before"]["CP2D"], p["before"]["CP2C"])
            cases.append({"source": "ps1-pairs", "id": f"{folder.name} {p['site']:08x} #{k[p['site']]}", "cmd": cmd,
                          "writes": [(r, inp[r]) for r in PAIR_ORDER], "model_in": inp,
                          "out": ps1_view(p["after"]["CP2D"], p["after"]["CP2C"])})
    digest = hashlib.sha256("".join(h + "\n" for h in sorted(read.values())).encode("ascii")).hexdigest()
    return cases, digest


def run(exe, stdin, n):
    """-> the output lines of exe --driver."""
    r = subprocess.run([exe, "--driver"], input=stdin, capture_output=True, text=True)
    if r.returncode != 0:
        fail(f"{exe} --driver: exit code {r.returncode}")
    lines = r.stdout.splitlines()
    if len(lines) != n:
        fail(f"{exe} --driver: {len(lines)} lines for {n} cases")
    return lines


def differences(lines, want):
    return [i for i, (got, w) in enumerate(zip(lines, want)) if got != w]


def main(argv):
    rec = len(argv) > 1 and argv[1] == "--recordings"
    progs = argv[2:] if rec else argv[1:]
    names = list(MUTANTS["gte"])
    if len(progs) != 1 + len(names):
        sys.exit(__doc__)
    for name, exe in zip(names, progs[1:]):
        if not Path(exe).stem.endswith(name):
            sys.exit(f"gte_sources: {exe} is not the mutant {name}")
    t0 = time.monotonic()
    if rec:
        status = private_inputs.check(VARIABLE, str(ROOT))
        if status:
            sys.exit(status)
    src = public_sources()
    for s, n in COUNTS.items():
        if len(src[s]) != n:
            fail(f"{s}: {len(src[s])} cases, expected {n}")
    for name, (op, n) in PS1_TESTS_CHECKED.items():
        sel = [c for c in src["ps1-tests"] if c["cmd"] is not None and c["cmd"] & 0x3F == op]
        bad = [c["id"] for c in sel if gm.step(c["model_in"], c["cmd"]) != [w & M32 for w in c["out"]]]
        print(f"gte_sources: model against ps1-tests {name}: {len(sel)} cases, {len(bad)} differences")
        if len(sel) != n or bad:
            fail(f"model against ps1-tests {name}: {len(sel)} cases, first differences {bad[:5]}")
    counts = PUBLIC_MUTANT_COUNTS
    if rec:
        cases, digest = recordings(Path(os.environ[VARIABLE]))
        print(f"gte_sources: recordings: {len(cases)} clean pairs, identity digest {digest}")
        if digest != RECORDINGS_DIGEST:
            fail(f"recordings identity digest {digest}, pinned {RECORDINGS_DIGEST}")
        if len(cases) != CLEAN_PAIRS:
            fail(f"recordings: {len(cases)} clean pairs, expected {CLEAN_PAIRS}")
        src["ps1-pairs"] = cases
        counts = RECORDINGS_MUTANT_COUNTS
    flat = [c for s in src.values() for c in s]
    if rec and len(flat) != ALL_CASES:
        fail(f"all sources: {len(flat)} cases, expected {ALL_CASES}")
    want = [" ".join(f"{w:08x}" for w in gm.step(c["model_in"], c["cmd"])) for c in flat]
    stdin = "".join(f"{'-' if c['cmd'] is None else format(c['cmd'], 'x')} {len(c['writes'])} "
                    + " ".join(f"{r} {v & M32:x}" for r, v in c["writes"]) + "\n" for c in flat)
    print(f"gte_sources: {len(flat)} cases ({', '.join(f'{s} {len(v)}' for s, v in src.items())}), model "
          f"{time.monotonic() - t0:.1f} s")
    bad = differences(run(progs[0], stdin, len(flat)), want)
    per = Counter(flat[i]["source"] for i in bad)
    print(f"gte_sources: gte: {len(bad)} differences {dict(per)}")
    if bad:
        c = flat[bad[0]]
        fail(f"gte differs from the model, first case {c['source']} {c['id']} cmd "
             f"{'-' if c['cmd'] is None else format(c['cmd'], 'x')}")
    got = []
    for name, exe in zip(names, progs[1:]):
        bad = differences(run(exe, stdin, len(flat)), want)
        got.append(len(bad))
        print(f"gte_sources: mutant {name}: {len(bad)} differences {dict(Counter(flat[i]['source'] for i in bad))}")
    if tuple(got) != counts:
        fail(f"mutant counts {tuple(got)}, pinned {counts}")
    fixed_words(progs[0])
    print(f"gte_sources: passed, {time.monotonic() - t0:.1f} s")


def fixed_words(exe):
    """Every RTPS and RTPT cmd vector of tests/vectors/gte.vec, run with the fixed word of the entry
    (sh2o_gte_nrtps/nrtpt) through exe --driver and through the model: the 64 registers must be equal."""
    words = {0x01: 0x4A180001, 0x30: 0x4A280030}
    counts = {0x01: 0, 0x30: 0}
    stdin, want = "", []
    for line in (ROOT / "tests/vectors/gte.vec").read_text(encoding="ascii").splitlines():
        if not line.startswith("v cmd "):
            continue
        i = bytes.fromhex(line.split()[2])
        w = [int.from_bytes(i[4 * k:4 * k + 4], "little") for k in range(65)]
        regs, op = w[:64], w[64] & 0x3F
        if op not in words:
            continue
        counts[op] += 1
        stdin += f"{words[op]:x} {len(vg.ORDER)} " + " ".join(f"{r} {regs[r]:x}" for r in vg.ORDER) + "\n"
        want.append(" ".join(f"{x:08x}" for x in gm.step(regs, words[op])))
    n = len(want)
    bad = differences(run(exe, stdin, n), want)
    print(f"gte_sources: fixed words: {n} cases (RTPS {counts[0x01]}, RTPT {counts[0x30]}), {len(bad)} differences")
    if n != 13 or bad:
        fail(f"fixed words: {n} cases, expected 13; differing cases {bad[:5]}")


if __name__ == "__main__":
    main(sys.argv)
