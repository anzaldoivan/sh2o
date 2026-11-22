#!/usr/bin/env python3
"""Compare the PsyQ libgte bodies with their own object bytes, run by scripts/psyq_model.py.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage:
  python3 tests/psyq_game_check.py identity
  python3 tests/psyq_game_check.py table
  python3 tests/psyq_game_check.py bodies <psyq program> <mutant program>...
  python3 tests/psyq_game_check.py suite
  python3 tests/psyq_game_check.py ratan2 <psyq program>

The inputs are private: SH2O_PS1_LIBGTE_OBJECTS, SH2O_PS1_LINK_MAP and SH2O_PS1_DATA (docs/EXACTNESS.md). Each mode
first runs the private input check of tests/private_inputs.py on the three: a path in the repository gives exit code
1; else no variable or an absent path gives exit code 77 (CTest shows the test as skipped). The digest of the files
read is pinned.

identity: the sha256 of each body's .text bytes and of ratan2's two table ranges equal the Identity lines of
include/sh2o/psyq.h. table: ratan2's table equals the formula of scripts/psyq_tables.py, and the halfword before it
is 0 (the value of the C outside its table). bodies: seeded cases of each body (one random.Random(0x5053) over the
bodies in sorted order) through the model; a case that traps is counted apart and not compared. The programs are
tests/test_psyq.c builds, run with --driver: the test program, then the psyq mutants in the order of
tests/mutant.py MUTANTS["psyq"]. The test program must give the model's full output line on every case; each mutant,
run on the cases of its body only, must give its pinned count of differing cases. suite: the case set of
tests/suite/sh2o_psyq_test_suite.c (SH2O_PSYQ_SEED) through the model gives SH2O_DIGEST_PSYQ, and no case ends in a
break; the plant (one SetFogNearFar case with h = 0) must end in one. ratan2: 12,169 pairs
(169 edges, 6,000 random.Random(0x5236) pairs in -2048..2048, 6,000 xorshift64 pairs) as ratan2 cases with zero
GTE words: the program gives the model's line on each, and the model breaks on 2.
The script exits 1 on the first failure.
"""
import os
import random
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import gte_model as gm  # noqa: E402
import psyq_model as pm  # noqa: E402
import psyq_tables  # noqa: E402
import private_inputs  # noqa: E402
from mutant import MUTANTS  # noqa: E402

M32 = 0xffffffff
VARIABLES = ("SH2O_PS1_LIBGTE_OBJECTS", "SH2O_PS1_LINK_MAP", "SH2O_PS1_DATA")
IDENTITY_DIGEST = "04941694cdaf592a49af7524bd32cf3f5c1d6d1c451fb526a64245148157ef48"
HEADER = ROOT / "include/sh2o/psyq.h"
SUITE_HEADER = ROOT / "tests/suite/sh2o_psyq_test.h"
# The order of test_psyq --driver's 61 register writes.
ORDER = (0, 1, 2, 3, 4, 5, 6, 7, 8, 28, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 30,
         *range(32, 64))
NWORDS = {"M": 8, "V": 4, "S": 2, "L": 1, "K": 161}
REGION = 0x80100000          # model address of region k: REGION + 0x1000 k
I32 = (-0x80000000, -0x7fffffff, -0x8000, -1, 0, 1, 0x7fff, 0x8000, 0x7fffffff)
I16 = (-0x8000, -1, 0, 1, 0x7fff)
# body -> (regions, pointer args, int args, return kind: void | ptr | int | stack)
BODIES = {"ApplyMatrix": ("MSV", 3, 0, "ptr"), "ApplyMatrixLV": ("MVV", 3, 0, "ptr"),
          "CompMatrix": ("MMM", 3, 0, "ptr"), "InitGeom": ("", 0, 0, "void"), "PopMatrix": ("K", 1, 0, "stack"),
          "PushMatrix": ("K", 1, 0, "stack"), "RotTrans": ("SVL", 3, 0, "void"),
          "SetBackColor": ("", 0, 3, "void"), "SetColorMatrix": ("M", 1, 0, "void"), "SetFarColor": ("", 0, 3, "void"),
          "SetFogNearFar": ("", 0, 3, "void"), "SetGeomOffset": ("", 0, 2, "void"),
          "SetGeomScreen": ("", 0, 1, "void"), "SetLightMatrix": ("M", 1, 0, "void"),
          "SetRotMatrix": ("M", 1, 0, "void"), "SetTransMatrix": ("M", 1, 0, "void"), "TransMatrix": ("MV", 2, 0, "ptr"),
          "TransposeMatrix": ("MM", 2, 0, "ptr"), "ratan2": ("", 0, 2, "int")}
SUITE_ORDER = sorted(BODIES, key=lambda n: (n[0].islower(), n))   # the suite's body order (ratan2 last)
# bodies mode: (cases, compared, trapped, with break) per body. SetFogNearFar's 109 breaks: the original port's
# record gives 124; its own test at its baseline, run on its build, gives 109, so that record is stale.
BODY_COUNTS = {"ApplyMatrix": (365, 365, 0, 0), "ApplyMatrixLV": (365, 365, 0, 0), "CompMatrix": (493, 491, 2, 0),
               "InitGeom": (64, 64, 0, 0), "PopMatrix": (42, 42, 0, 0), "PushMatrix": (42, 42, 0, 0),
               "RotTrans": (301, 301, 0, 0), "SetBackColor": (265, 265, 0, 0), "SetColorMatrix": (301, 301, 0, 0),
               "SetFarColor": (265, 265, 0, 0), "SetFogNearFar": (1212, 1212, 0, 109),
               "SetGeomOffset": (265, 265, 0, 0), "SetGeomScreen": (265, 265, 0, 0),
               "SetLightMatrix": (301, 301, 0, 0), "SetRotMatrix": (301, 301, 0, 0), "SetTransMatrix": (301, 301, 0, 0),
               "TransMatrix": (301, 301, 0, 0), "TransposeMatrix": (365, 365, 0, 0), "ratan2": (1465, 1465, 0, 2)}
# psyq mutant -> (the body it changes, differing cases over that body's cases)
MUTANT_COUNTS = {
    "back_colour_shift": ("SetBackColor", 263),
    "rot_matrix_swapped": ("SetRotMatrix", 256),
    "transpose_dropped_store": ("TransposeMatrix", 320),
    "rot_trans_no_sf": ("RotTrans", 301),
    "pop_matrix_bit": ("PopMatrix", 40),
    "fog_no_break_hook": ("SetFogNearFar", 109),             # record 124 is stale: the original test gives 109
    "ratan_index_off_by_one": ("ratan2", 894),               # record 923 is stale: the original test gives 894
    "init_geom_screen": ("InitGeom", 64),
    "apply_matrix_swapped": ("ApplyMatrix", 320),            # stores x as the output vy
    "apply_matrix_lv_sign": ("ApplyMatrixLV", 336),          # vz = b3 - a3, not b3 + a3
    "comp_matrix_sign": ("CompMatrix", 481),                 # subtracts m0's t[1] in the translation
    "push_matrix_top": ("PushMatrix", 40),                   # moves the stack top by 31 bytes, not 32
    "colour_matrix_base": ("SetColorMatrix", 301),           # writes the colour matrix from register 49, not 48
    "far_colour_shift": ("SetFarColor", 263),                # shifts the blue far colour by 3, not 4
    "geom_offset_shift": ("SetGeomOffset", 263),             # shifts the y offset by 15, not 16
    "geom_screen_plus_one": ("SetGeomScreen", 265),          # writes h + 1 to H
    "light_matrix_base": ("SetLightMatrix", 301),            # writes the light matrix from register 41, not 40
    "trans_matrix_swapped": ("SetTransMatrix", 256),         # writes tz as the translation's y
    "trans_matrix_store": ("TransMatrix", 256)}              # stores z as m's t[1]
EDGES = (0, 1, -1, (1 << 21) - 1, 1 << 21, (1 << 21) + 1, -(1 << 21) + 1, -(1 << 21), -(1 << 21) - 1, -0x80000000,
         -0x7fffffff, 0x7fffffff, 0x7ffffffe)
SAMPLE = 6000


def fail(text):
    print(f"psyq_game: FAIL: {text}")
    sys.exit(1)


def private_check():
    """The check of tests/private_inputs.py on the three variables: any failure first, then any skip."""
    status = [private_inputs.check(v, str(ROOT)) for v in VARIABLES]
    if private_inputs.FAIL in status:
        sys.exit(private_inputs.FAIL)
    if private_inputs.SKIP in status:
        sys.exit(private_inputs.SKIP)


def machine():
    m = pm.Machine(*(os.environ[v] for v in VARIABLES), sorted(BODIES))
    d = m.identity_digest()
    print(f"psyq_game: {len(m.files)} files read, identity digest {d}")
    if d != IDENTITY_DIGEST:
        fail(f"identity digest {d}, pinned {IDENTITY_DIGEST}")
    return m


def header_ids():
    """{body: (text sha256, [table sha256])} from the Identity lines of include/sh2o/psyq.h."""
    out = {}
    for m in re.finditer(r"Identity: \.text sha256 ([0-9a-f]{64})(.*?)\bsh2o_psyq_(\w+)\(", HEADER.read_text(), re.S):
        out[m.group(3)] = (m.group(1), re.findall(r"table sha256 ([0-9a-f]{64})", m.group(2)))
    return out


# --- cases
def gte_words(rng, tame):
    mask = 0x0fff0fff if tame else M32
    return [rng.getrandbits(32) & mask for _ in range(64)]


def rand_region(rng, kind, mask=M32, top=None):
    if kind == "K":
        return [(top if top is not None else rng.randrange(21) * 32)] + [rng.getrandbits(32) for _ in range(160)]
    return [rng.getrandbits(32) & mask for _ in range(NWORDS[kind])]


def edge_matrix(e16, e32):
    h = e16 & 0xffff
    return [h | h << 16] * 5 + [e32 & M32] * 3


def cases(name, rng):
    """[(gte words, [(kind, words)], [args: ('r', k) | int])] for body name: edges then seeded."""
    kinds, nptr, nint, _ = BODIES[name]
    out = []

    def add(regions, ints=(), args=None, gte=None, tame=False):
        a = args if args is not None else [("r", k) for k in range(nptr)] + [i & M32 for i in ints]
        out.append((gte or gte_words(rng, tame), [(k, w) for k, w in zip(kinds, regions)], a))

    if name == "ratan2":
        edge = (-0x80000000, -0x7fffffff, -0x200001, -0x200000, -0x1fffff, -1025, -1024, -1023, -2, -1, 0, 1, 2,
                1023, 1024, 1025, 0x1fffff, 0x200000, 0x200001, 0x7ffffffe, 0x7fffffff)
        for y in edge:
            for x in edge:
                add([], (y, x))
        for _ in range(512):
            x = rng.getrandbits(32) >> rng.randrange(32)
            y = rng.getrandbits(32) >> rng.randrange(32)
            x, y = x * rng.choice((1, -1)), y * rng.choice((1, -1))
            add([], (y, x))
            add([], (x * rng.choice((1, -1)), x))             # the diagonals
    elif name == "SetFogNearFar":
        ab = (-0x80000000, -1000, -1, 0, 1, 99, 100, 101, 1000, 0x7fffffff)
        for a in ab:
            for b in ab:
                for h in (-0x80000000, -1, 0, 1, 2, 1000, 0x7fffffff):
                    add([], (a, b, h))
        for _ in range(256):
            a = rng.randrange(-0x8000, 0x8000)
            add([], (a, a + rng.randrange(-200, 0x10000), rng.choice((0, -1, rng.randrange(-0x400, 0x400)))))
            add([], (rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)))
    elif name in ("PushMatrix", "PopMatrix"):
        for top in range(0, 0x281, 32):
            for t in (False, True):
                add([rand_region(rng, "K", top=top)], tame=t)
    elif nint:
        for e in I32:
            add([], [e] * nint)
        for _ in range(256):
            add([], [rng.getrandbits(32) for _ in range(nint)])
    if kinds and kinds != "K":
        for e16 in I16:
            for e32 in I32:
                add([edge_matrix(e16, e32) if k == "M" else [e32 & M32] * NWORDS[k] for k in kinds])
        for k in range(256):
            mask0 = 0x0fff0fff if name == "CompMatrix" and k % 2 else M32
            add([rand_region(rng, kk, mask0 if i == 0 else M32) for i, kk in enumerate(kinds)], tame=k % 4 == 1)
        alias = {"TransposeMatrix": [("r", 0), ("r", 0)], "CompMatrix": None,
                 "ApplyMatrixLV": [("r", 0), ("r", 1), ("r", 1)], "ApplyMatrix": [("r", 0), ("r", 1), ("r", 0)]}
        if name in alias:
            for _ in range(64):
                regs = [rand_region(rng, kk, 0x0fff0fff) for kk in kinds]
                if name == "CompMatrix":
                    for a in ([("r", 0), ("r", 1), ("r", 0)], [("r", 0), ("r", 1), ("r", 1)],
                              [("r", 0), ("r", 0), ("r", 0)]):
                        add(regs, args=a)
                else:
                    add(regs, args=alias[name])
    if name == "InitGeom":
        for k in range(64):
            add([], tame=k % 2 == 1)
    return out


def case_line(name, c):
    gte, regions, args = c
    parts = [name, *(f"{w:x}" for w in gte), str(len(regions))]
    for kind, words in regions:
        parts += [kind, *(f"{w:x}" for w in words)]
    parts.append(str(len(args)))
    parts += [f"r{a[1]}" if isinstance(a, tuple) else f"{a:x}" for a in args]
    return " ".join(parts) + "\n"


class Model:
    """The machine; model(name, case) -> (the driver's output tokens, events), or 'trap'."""
    def __init__(self, m):
        self.m = m

    def __call__(self, name, c):
        gte, regions, args = c
        mc = self.m
        mc.gte = [0] * 64
        for r in ORDER:
            mc.gte_write(r, gte[r])
        addr = []
        for k, (kind, words) in enumerate(regions):
            if kind == "K":
                mc.wr(mc.stack_top, 4, words[0])
                for i, w in enumerate(words[1:]):
                    mc.wr(mc.stack + 4 * i, 4, w)
                addr.append(None)
            else:
                a = REGION + 0x1000 * k
                for i, w in enumerate(words):
                    mc.wr(a + 4 * i, 4, w)
                addr.append(a)
        av = [(addr[a[1]] or 0) if isinstance(a, tuple) else a for a in args]
        brk = None
        try:
            v0 = mc.run(name, av)
        except pm.Trap:
            return "trap"
        except pm.Break as e:
            brk = e.code
        kind = BODIES[name][3]
        if brk is not None:
            ret = f"brk{brk:x}"
        elif kind == "void":
            ret = "-"
        elif kind == "int":
            ret = f"{v0:x}"
        elif kind == "stack":
            ret = "ffffffff" if ("printf",) in mc.events else "0"
        else:
            ret = next((f"r{k}" for k, a in enumerate(addr) if a == v0), "r?")
        toks = [ret]
        for k, (kind, words) in enumerate(regions):
            if kind == "K":
                toks.append(f"{mc.rd(mc.stack_top, 4):x}")
                toks += [f"{mc.rd(mc.stack + 4 * i, 4):x}" for i in range(160)]
            else:
                toks += [f"{mc.rd(addr[k] + 4 * i, 4):x}" for i in range(len(words))]
        toks += [f"{w:x}" for w in gm.read_view(mc.gte)]
        return toks, list(mc.events) + ([("break", brk)] if brk is not None else [])


def run(exe, lines):
    """-> the output lines of exe --driver, split into tokens."""
    r = subprocess.run([exe, "--driver"], input="".join(lines), capture_output=True, text=True)
    if r.returncode != 0:
        fail(f"{exe} --driver: exit code {r.returncode}")
    out = [ln.split() for ln in r.stdout.splitlines()]
    if len(out) != len(lines):
        fail(f"{exe} --driver: {len(out)} lines for {len(lines)} cases")
    return out


def differences(got, want):
    return sum(1 for g, w in zip(got, want) if g != w)


def identity(m):
    hdr = header_ids()
    if sorted(hdr) != sorted(BODIES):
        fail(f"{HEADER.relative_to(ROOT)}: Identity lines for {sorted(hdr)}")
    table, before = m.table_bytes()
    for name in sorted(BODIES):
        got = (m.text_sha256(name), [pm.sha256(table), pm.sha256(before)] if name == "ratan2" else [])
        print(f"psyq_game: {name} text {got[0][:8]}" + "".join(f" table {t[:8]}" for t in got[1]))
        if got != hdr[name]:
            fail(f"{name}: identity {got}, header {hdr[name]}")
    print(f"psyq_game: identity of {len(BODIES)} bodies and 2 tables equal the header")


def table(m):
    t, before = m.table_bytes()
    want = b"".join((v & 0xffff).to_bytes(2, "little") for v in psyq_tables.ratan_tbl())
    if len(want) != 2 * pm.TABLE_ENTRIES or t != want:
        fail("ratan2's table differs from the formula of scripts/psyq_tables.py")
    if before != b"\0\0":
        fail(f"the halfword before ratan2's table is {before.hex()}, not 0")
    print(f"psyq_game: ratan2's table ({pm.TABLE_ENTRIES} entries) equals the formula; the halfword before it is 0")


def bodies(m, progs):
    names = list(MUTANTS["psyq"])
    if len(progs) != 1 + len(names):
        sys.exit(__doc__)
    for name, exe in zip(names, progs[1:]):
        if not Path(exe).stem.endswith(name):
            sys.exit(f"psyq_game: {exe} is not the mutant {name}")
    model, rng = Model(m), random.Random(0x5053)
    kept, want, counts = {}, {}, {}
    for name in sorted(BODIES):
        cs = cases(name, rng)
        res = [model(name, c) for c in cs]
        keep = [k for k, r in enumerate(res) if r != "trap"]
        kept[name] = [cs[k] for k in keep]
        want[name] = [res[k][0] for k in keep]
        counts[name] = (len(cs), len(keep), len(cs) - len(keep),
                        sum(1 for k in keep if any(e[0] == "break" for e in res[k][1])))
        print(f"psyq_game: {name}: {counts[name][0]} cases, {counts[name][1]} compared, {counts[name][2]} trapped "
              f"(apart), {counts[name][3]} with break")
    total = [sum(c[i] for c in counts.values()) for i in range(4)]
    print(f"psyq_game: {total[0]} cases, {total[1]} compared, {total[2]} trapped, {total[3]} with break")
    for name in sorted(BODIES):
        pin = BODY_COUNTS[name]
        if any(p is not None and p != c for p, c in zip(pin, counts[name])) or counts[name][3] == 0 and pin[3] is None:
            fail(f"{name}: counts {counts[name]}, pinned {pin}")
    lines = [case_line(name, c) for name in sorted(BODIES) for c in kept[name]]
    flat = [w for name in sorted(BODIES) for w in want[name]]
    got = run(progs[0], lines)
    bad = [k for k, (g, w) in enumerate(zip(got, flat)) if g != w]
    print(f"psyq_game: psyq: {len(bad)} differences")
    if bad:
        fail(f"psyq differs from the model, first line {lines[bad[0]][:60]}")
    for name, exe in zip(names, progs[1:]):
        body, pin = MUTANT_COUNTS[name]
        n = differences(run(exe, [case_line(body, c) for c in kept[body]]), want[body])
        print(f"psyq_game: mutant {name} ({body}): {n} differences")
        if n == 0 or pin is not None and n != pin:
            fail(f"mutant {name}: {n} differences, pinned {pin if pin is not None else 'more than 0'}")


def suite_cases(seed):
    """The suite's case set (tests/suite/sh2o_psyq_test_suite.c) -> [(body, case)]."""
    x = seed
    out = []

    def draw():
        nonlocal x
        x = (x * 1664525 + 1013904223) & M32
        return x

    def mag(v, s, neg):
        v >>= s & 31
        return (-v if neg else v) & M32

    for i in range(len(SUITE_ORDER) * 64):
        name = SUITE_ORDER[i >> 6]
        kinds, nptr, nint, _ = BODIES[name]
        mask = 0x0fff0fff if i & 1 else M32
        gte = [draw() & mask for _ in range(64)]
        regions = []
        for n, k in enumerate(kinds):
            if k == "K":
                top = min(draw() >> 27, 20) * 32
                regions.append((k, [top] + [draw() for _ in range(160)]))
            else:
                mm = 0x0fff0fff if name == "CompMatrix" and n == 0 else M32
                regions.append((k, [draw() & mm for _ in range(NWORDS[k])]))
        if name == "SetFogNearFar":
            ints = [((draw() & 0xffff) - 0x8000) & M32, ((draw() & 0xffff) - 0x8000) & M32, draw() & 0x3ff]
        elif name == "ratan2":
            d1, d2, d3 = draw(), draw(), draw()
            ints = [mag(d1, d3, d3 & 0x20), mag(d2, d3 >> 8, d3 & 0x2000)]
        else:
            ints = [draw() for _ in range(nint)]
        out.append((name, (gte, regions, [("r", k) for k in range(nptr)] + ints)))
    return out


def fnv(h, w):
    for b in (w & M32).to_bytes(4, "big"):
        h = ((h ^ b) * 0x01000193) & M32
    return h


def suite(m):
    src = SUITE_HEADER.read_text()
    seed = int(re.search(r"#define SH2O_PSYQ_SEED 0x([0-9A-Fa-f]{8})u", src).group(1), 16)
    pinned = int(re.search(r"#define SH2O_DIGEST_PSYQ 0x([0-9A-Fa-f]{8})u", src).group(1), 16)
    model, h, cs = Model(m), 0x811c9dc5, suite_cases(seed)
    brks = 0
    for name, c in cs:
        r = model(name, c)
        if r == "trap":
            fail(f"suite case of {name} traps")
        toks = r[0]
        ret = toks[0]
        brks += ret.startswith("brk")
        if ret == "-" or ret.startswith("brk"):
            rw = 0
        elif ret.startswith("r"):
            rw = int(ret[1:]) + 1 if ret != "r?" else int(ret, 16)
        else:
            rw = int(ret, 16)
        for w in [rw, *(int(t, 16) for t in toks[1:])]:
            h = fnv(h, w)
    print(f"psyq_game: suite {len(cs)} cases, model digest 0x{h:08x}, SH2O_DIGEST_PSYQ 0x{pinned:08x}")
    if len(cs) != 1216 or h != pinned:
        fail("the model's suite digest is not SH2O_DIGEST_PSYQ")
    print(f"psyq_game: suite: {brks} cases end in a break")
    if brks:
        fail(f"suite: {brks} cases end in a break, expected 0")
    # the plant: a copy of the case list where the first SetFogNearFar case with far - near >= 100 (the body returns
    # early below 100) has h = 0, its last divisor
    planted = list(cs)
    k = next(i for i, (name, c) in enumerate(planted) if name == "SetFogNearFar" and
             ((c[2][-2] ^ 0x80000000) - (c[2][-3] ^ 0x80000000)) >= 100)
    gte, regions, args = planted[k][1]
    args = list(args)
    args[-1] = 0
    planted[k] = ("SetFogNearFar", (gte, regions, args))
    nplant = 0
    for name, c in planted:
        r = model(name, c)
        nplant += r != "trap" and r[0][0].startswith("brk")
    print(f"psyq_game: suite plant (SetFogNearFar case {k}, h = 0): {nplant} cases end in a break")
    if nplant < 1:
        fail("suite: plant not caught")


def xorshift(n):
    s, out = 0x5348325a414d5330, []
    for _ in range(n):
        pair = []
        for _ in range(2):
            s ^= (s << 13) & 0xffffffffffffffff
            s ^= s >> 7
            s ^= (s << 17) & 0xffffffffffffffff
            v = s >> 32
            pair.append(v - (1 << 32) if v & 0x80000000 else v)
        out.append(tuple(pair))
    return out


def ratan2(m, progs):
    if len(progs) != 1:
        sys.exit(__doc__)
    rng = random.Random(0x5236)
    ps = [(y, x) for y in EDGES for x in EDGES]
    ps += [(rng.randint(-2048, 2048), rng.randint(-2048, 2048)) for _ in range(SAMPLE)]
    ps += xorshift(SAMPLE)
    model = Model(m)
    cs = [([0] * 64, [], [y & M32, x & M32]) for y, x in ps]
    res = [model("ratan2", c) for c in cs]
    if "trap" in res:
        fail("a ratan2 pair traps")
    want = [r[0] for r in res]
    nbrk = sum(1 for r in res if any(e[0] == "break" for e in r[1]))
    n = differences(run(progs[0], [case_line("ratan2", c) for c in cs]), want)
    print(f"psyq_game: ratan2: {len(ps)} pairs, {n} differences, {nbrk} with break")
    if len(ps) != 12169 or n or nbrk != 2:
        fail(f"ratan2: {len(ps)} pairs, {n} differences, {nbrk} with break (expected 12169, 0, 2)")


def main(argv):
    modes = {"identity": 0, "table": 0, "bodies": None, "suite": 0, "ratan2": 1}
    if len(argv) < 2 or argv[1] not in modes:
        sys.exit(__doc__)
    mode, progs = argv[1], argv[2:]
    if modes[mode] is not None and len(progs) != modes[mode]:
        sys.exit(__doc__)
    t0 = time.monotonic()
    private_check()
    m = machine()
    if mode == "identity":
        identity(m)
    elif mode == "table":
        table(m)
    elif mode == "bodies":
        bodies(m, progs)
    elif mode == "suite":
        suite(m)
    else:
        ratan2(m, progs)
    print(f"psyq_game: {mode} passed, {time.monotonic() - t0:.1f} s")


if __name__ == "__main__":
    main(sys.argv)
