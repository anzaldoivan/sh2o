#!/usr/bin/env python3
"""Write the vector file of the list family from the Python model (scripts/gte_model.py).

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/vectors_list.py tests/vectors/list.vec

One op, list: the input registers (64 words in the MFC2/CFC2 read view) are written in ORDER (scripts/vectors_gte.py)
on a zeroed image. Then for each of the n items in order: MTC2 0 (VXY0) and MTC2 1 (VZ0) of its two words by the
write rule, then one MVMVA cmd (gte_model.step); its row is IR1, IR2, IR3 (the MFC2 view) and MAC1, MAC2, MAC3.
Results: 16 rows of 6 words (rows n..15 are 0) and the 64 register reads after the last item. Items n..15 of the
input are 0. Vectors, in order:
- sel: each mx x v x cv (64) once, sf and lm in turn, on an LCG state (raw or tame in turn), 1..16 LCG items.
- n: n = 0, 1 and 16, each sf x lm, RT x V0 + TR, items with negative fields (negative IR).
- bug: cv = 2 (the FC bug) with each mx, and mx = 3 (the garbage matrix) with each cv, sf x lm in turn.
- sat: matrices and vectors at the 16-bit bounds, TR at the 32-bit bounds, sf x lm: IR and MAC saturate.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gte_model import read_view, step  # noqa: E402
from vectors_gte import Lcg, cmdw, image, le, pack, put3, put_mat, tame, write  # noqa: E402

M32 = 0xFFFFFFFF
LCG_SEED = 0x4C495354
MAX_ITEMS = 16


def run(regs, cmd, items):
    """-> (16 rows of 6 words, flat; the 64 reads after the last item)"""
    s = image(regs)
    rows = []
    for vxy, vz in items:
        write(s, 0, vxy)
        write(s, 1, vz)
        s = step(read_view(s), cmd)
        rows += s[9:12] + s[25:28]
    return rows + [0] * (6 * MAX_ITEMS - len(rows)), read_view(s)


def vector(cmd, regs, items):
    regs = list(regs)
    if read_view(image(regs)) != regs:
        sys.exit("vectors_list: an input does not read back through the write order")
    if len(items) > MAX_ITEMS:
        sys.exit("vectors_list: more than 16 items")
    rows, out = run(regs, cmd, items)
    flat = [w for item in items for w in item] + [0] * (2 * (MAX_ITEMS - len(items)))
    return le(regs + [cmd, len(items)] + flat), le(rows + out)


def state(raw):
    raw = list(raw)
    raw[63] &= 0x7FFFF000
    return read_view(image(raw))


def cases():
    r = Lcg(LCG_SEED)
    out = []
    for i in range(64):
        mx, v, cv = i >> 4, i >> 2 & 3, i & 3
        sf, lm = (i ^ i >> 2) & 1, (i >> 1 ^ i >> 4) & 1
        regs = state(tame(r) if i & 1 else [r() for _ in range(64)])
        n = 1 + (r() >> 28)
        if i & 1:
            items = [(pack(r.s(12), r.s(12)), r.s(12) & 0xFFFF) for _ in range(n)]
        else:
            items = [(r(), r()) for _ in range(n)]
        out.append((f"sel mx={mx} v={v} cv={cv} sf={sf} lm={lm} n={n}", cmdw(0x12, sf, lm, mx, v, cv), regs, items))
    for n in (0, 1, 16):
        for sf in (0, 1):
            for lm in (0, 1):
                regs = state(tame(r))
                items = [(pack(-1 - (r() >> 21), r.s(12)), (-1 - (r() >> 21)) & 0xFFFF) for _ in range(n)]
                out.append((f"n n={n} sf={sf} lm={lm}", cmdw(0x12, sf, lm), regs, items))
    for k in range(8):
        mx, cv = (k, 2) if k < 4 else (3, k - 4)
        sf, lm = k & 1, k >> 1 & 1
        regs = state(tame(r))
        items = [(pack(r.s(12), r.s(12)), r.s(12) & 0xFFFF) for _ in range(4)]
        out.append((f"bug mx={mx} cv={cv} sf={sf} lm={lm}", cmdw(0x12, sf, lm, mx, 0, cv), regs, items))
    for k in range(4):
        sf, lm = k & 1, k >> 1
        s = [0] * 64
        put_mat(s, 32, (0x7FFF, -0x8000, 0x7FFF, -0x8000, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, -0x8000))
        put3(s, 37, (0x7FFFFFFF, -0x80000000, 0x1000))
        items = [(pack(0x7FFF, -0x8000), 0x7FFF), (pack(-0x8000, 0x7FFF), 0x8000), (pack(0x7FFF, 0x7FFF), 0x7FFF),
                 (pack(-0x8000, -0x8000), 0x8000), (pack(0, 0), 0), (pack(0x100, -0x100), 0x0100)]
        out.append((f"sat sf={sf} lm={lm}", cmdw(0x12, sf, lm), state(s), items))
    return out


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    lines = [
        "# Vectors for the list family, written by scripts/vectors_list.py.",
        "# Reference: the Python model scripts/gte_model.py. Registers are the 64 MFC2/CFC2 reads, register 0",
        "# first; the input registers are written in the suite order on a zeroed image; items n..15 are 0.",
        "sh2o-vec 1",
        "family list",
        "op list in regs:u32[64] cmd:u32 n:u32 items:u32[32] out rows:u32[96] regs:u32[64]",
    ]
    vecs = cases()
    for cid, cmd, regs, items in vecs:
        i, o = vector(cmd, regs, items)
        lines.append(f"# {cid}")
        lines.append(f"v list {i.hex()} {o.hex()}")
    with open(argv[1], "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print(f"vectors_list: {len(vecs)} vectors")


if __name__ == "__main__":
    main(sys.argv)
