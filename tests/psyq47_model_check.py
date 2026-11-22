#!/usr/bin/env python3
"""Run the checks of tests/psyq_game_check.py on the PsyQ 4.7 libgte bodies, linked from the archive.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage:
  python3 tests/psyq47_model_check.py table
  python3 tests/psyq47_model_check.py bodies <psyq program> <mutant program>...
  python3 tests/psyq47_model_check.py suite
  python3 tests/psyq47_model_check.py ratan2 <psyq program>
  python3 tests/psyq47_model_check.py same

The machine is scripts/psyq_model.py LinkedMachine on SH2O_PSYQ47_ARCHIVE (the archive fetched by
scripts/psyq_lnk.py fetch): no map and no game address. table, bodies, suite and ratan2 are the modes of
tests/psyq_game_check.py with the same pins. An unset or absent archive skips (exit code 77); with
SH2O_REQUIRE_PSYQ47=1 it fails (exit code 1) instead.
same: first the private input check of tests/psyq_game_check.py (SH2O_PS1_LIBGTE_OBJECTS, SH2O_PS1_LINK_MAP,
SH2O_PS1_DATA), then the game machine against the 4.7 one: the same routines loaded, the same call-stub names, the
same ratan2 table bytes, and the same model output (tokens and events, a trap too) on every case of bodies, of the
suite and its plant, and on the 12,169 ratan2 pairs. The script exits 1 on the first failure.
"""
import os
import random
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import psyq_model as pm  # noqa: E402
import psyq_game_check as gc  # noqa: E402

ARCHIVE = "SH2O_PSYQ47_ARCHIVE"
REQUIRE = "SH2O_REQUIRE_PSYQ47"


def fail(text):
    print(f"psyq47_model: FAIL: {text}")
    sys.exit(1)


def linked():
    zp = os.environ.get(ARCHIVE)
    if not zp or not Path(zp).is_file():
        if os.environ.get(REQUIRE) == "1":
            print(f"psyq47_model: required ({REQUIRE}=1), {ARCHIVE} is not set or not there")
            sys.exit(1)
        print(f"psyq47_model: {ARCHIVE} is not set or not there; skipped")
        sys.exit(77)
    m = pm.LinkedMachine(zp, sorted(gc.BODIES))
    print(f"psyq47_model: {len(m.entry)} routines linked, {len(m.stubs)} call stubs")
    return m


def compare(what, game, linked_m, cases):
    """Model output of both machines on [(body, case)]; the first difference fails."""
    a, b = gc.Model(game), gc.Model(linked_m)
    for name, c in cases:
        if a(name, c) != b(name, c):
            fail(f"{what}: the 4.7 machine differs from the game's, first line {gc.case_line(name, c)[:60]}")
    print(f"psyq47_model: same: {what}: {len(cases)} cases equal")


def reach(m):
    """(routines, call-stub names) reachable from the bodies by j/jal targets in the routines' extents."""
    addr = {a: n for n, a in m.entry.items()}
    seen, stubs, todo = set(), set(), sorted(gc.BODIES)
    while todo:
        n = todo.pop()
        if n in seen:
            continue
        seen.add(n)
        a, z = m.ranges[n]
        for p in range(m.entry[n], m.entry[n] + z - a, 4):
            w = m.rd(p, 4)
            if w >> 26 in (2, 3):
                t = ((p + 4) & 0xf0000000) | (w & 0x3ffffff) << 2
                if t in m.stubs:
                    stubs.add(m.stubs[t])
                elif t in addr:
                    todo.append(addr[t])
    return sorted(seen), sorted(stubs)


def same(game, m):
    # Reachable, not loaded: the game machine loads whole objects (with symbols no body reaches).
    (got, got_stubs), (want, want_stubs) = reach(m), reach(game)
    print(f"psyq47_model: same: reachable routines {len(want)} game, {len(got)} 4.7")
    if got != want:
        fail(f"routines only in the game: {sorted(set(want) - set(got))}, only in 4.7: {sorted(set(got) - set(want))}")
    got, want = got_stubs, want_stubs
    print(f"psyq47_model: same: reachable call stubs {len(want)} game, {len(got)} 4.7")
    if got != want:
        fail(f"call stubs only in the game: {sorted(set(want) - set(got))}, only in 4.7: {sorted(set(got) - set(want))}")
    if m.table_bytes() != game.table_bytes():
        fail("ratan2's table bytes differ")
    print(f"psyq47_model: same: ratan2's table and the 2 bytes before it equal")
    rng = random.Random(0x5053)
    compare("bodies", game, m, [(name, c) for name in sorted(gc.BODIES) for c in gc.cases(name, rng)])
    src = gc.SUITE_HEADER.read_text()
    seed = int(re.search(r"#define SH2O_PSYQ_SEED 0x([0-9A-Fa-f]{8})u", src).group(1), 16)
    cs = gc.suite_cases(seed)
    k = next(i for i, (name, c) in enumerate(cs) if name == "SetFogNearFar" and
             ((c[2][-2] ^ 0x80000000) - (c[2][-3] ^ 0x80000000)) >= 100)
    gte, regions, args = cs[k][1]
    plant = ("SetFogNearFar", (gte, regions, list(args[:-1]) + [0]))
    compare("suite and its plant", game, m, cs + [plant])
    r = random.Random(0x5236)
    ps = [(y, x) for y in gc.EDGES for x in gc.EDGES]
    ps += [(r.randint(-2048, 2048), r.randint(-2048, 2048)) for _ in range(gc.SAMPLE)]
    ps += gc.xorshift(gc.SAMPLE)
    compare("ratan2 pairs", game, m, [("ratan2", ([0] * 64, [], [y & gc.M32, x & gc.M32])) for y, x in ps])


def main(argv):
    modes = {"table": 0, "bodies": None, "suite": 0, "ratan2": 1, "same": 0}
    if len(argv) < 2 or argv[1] not in modes:
        sys.exit(__doc__)
    mode, progs = argv[1], argv[2:]
    if modes[mode] is not None and len(progs) != modes[mode]:
        sys.exit(__doc__)
    t0 = time.monotonic()
    if mode == "same":
        gc.private_check()
        game = gc.machine()
        same(game, linked())
    else:
        m = linked()
        if mode == "table":
            gc.table(m)
        elif mode == "bodies":
            gc.bodies(m, progs)
        elif mode == "suite":
            gc.suite(m)
        else:
            gc.ratan2(m, progs)
    print(f"psyq47_model: {mode} passed, {time.monotonic() - t0:.1f} s")


if __name__ == "__main__":
    main(sys.argv)
