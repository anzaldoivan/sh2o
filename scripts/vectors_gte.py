#!/usr/bin/env python3
"""Write the vector file of the gte family from the Python model (scripts/gte_model.py).

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/vectors_gte.py tests/vectors/gte.vec

cases() -> [{"id", "cmd", "in"}]: "in" = 64 words in the MFC2/CFC2 read view (the read_view of the model of the words
set here). Groups (id prefix):
- b44: TR, BK and FC at the 44-bit boundary values B44 with extreme matrices and vectors, every opcode, sf x lm
  (MVMVA mx 0/3 x v 0/3 x cv 0-3).
- div: RTPS/RTPT with SZ3 at H/2-1, H/2, H/2+1, 0 for H in DIV_H (SZ3 = TRZ through a zero RT row 3).
- mvmva: every mx x v x cv x sf x lm (256 selectors) on 8 LCG states.
- gpl: GPL with MAC1-3 near the 44-bit bound (MAC SHL 12), IR1-3/IR0 extremes, sf x lm.
- clamp: SX2/SY2 (OFX/OFY with q = 0), IR0 (DQB), IR1-3 (MVMVA, MAC = TR), colour (GPF, MAC = IR), OTZ (AVSZ3), SZ3
  (RTPS TRZ), MAC0 (NCLIP), each at its bound +-1.
- dpct: DPCT on each colour FIFO state of DPCT_FIFO x IR0 of DPCT_IR0; FC of DPCT_FC and sf x lm in turn.
- lcg: LCG_PER_OP cases per opcode from LCG_SEED (x = x * 1664525 + 1013904223), alternately raw (64 random words)
  and tame (fields scaled down so fewer results saturate); sf, lm (and MVMVA mx, v, cv) random; opcodes in SUITE_OPS
  order, then DPCT (an opcode added later draws after them, so the cases of the others stay).

suite_cases() -> [(cmd, in)]: the case set of tests/suite/sh2o_gte_test_suite.c (its first comment), on SUITE_OPS.

The vector file holds two ops. cmd: the input registers are written in ORDER on a zeroed image, then the command
runs; the results are its return code and the 64 register reads. write: the same image, then one register write;
the results are the 64 reads and the read of the written register. Vectors, in order: the 14 known-answer cases of
the suite; each opcode with each sf x lm (MVMVA: all 64 mx x v x cv, sf and lm in turn; DPCT, not in the suite, on
the DPCS case); the dpct group; edge cases picked from the groups above (not lcg) until each pair (opcode, FLAG bit)
that any case sets is in a vector, then EXTRA_EDGES; the gate group (gate());
register writes; one opcode that the model does not have (0Ch). The header that scripts/oracle_header.py makes
from the file must stay at most HEADER_MAX bytes: the script stops before it writes a larger file.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gte_model import MODELLED_ALL, read_view, step  # noqa: E402
from oracle_header import c_bytes  # noqa: E402

M32 = 0xFFFFFFFF
B44 = (0x7FF3FFFF, 0x7FF40000, 0x7FF00000, -0x80000000, -0x7FF40000, -0x7FF40001, 0x7FFFFFFF)
DIV_H = (0, 1, 2, 3, 0x100, 0x101, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF)
LCG_SEED = 0x53484732
LCG_PER_OP = 4096
OPS = sorted(MODELLED_ALL)
# DPCT: colour FIFO RGB0-2 (three distinct colours, zero, FFh); FC far from every colour, near RGB0 of the first
# state ((10h, 20h, 30h) SHL 4, +1..3), at the negative 44-bit bound (MAC1-3 flags with a colour above 0); IR0 0,
# 1000h, above
DPCT_FIFO = ((0x00302010, 0x00000040, 0x00008000), (0, 0, 0), (M32, M32, M32))
DPCT_FC = ((0x7FFF0, -0x10000, 0x800), (0x101, 0x202, 0x303), (-0x80000000,) * 3)
DPCT_IR0 = (0, 0x1000, 0x7FFF)


def cmdw(op, sf=1, lm=0, mx=0, v=0, cv=0):
    """GTE command word: op 0-5, lm 10, cv 13-14, v 15-16, mx 17-18, sf 19."""
    return op | lm << 10 | cv << 13 | v << 15 | mx << 17 | sf << 19


def pack(lo, hi=0):
    return (lo & 0xFFFF) | (hi & 0xFFFF) << 16


def put_mat(s, base, m):
    """3x3 matrix m (row-major, 9 int16) into the five words at base."""
    s[base:base + 5] = [pack(m[0], m[1]), pack(m[2], m[3]), pack(m[4], m[5]), pack(m[6], m[7]), m[8] & 0xFFFF]


def put_vec(s, n, v):
    s[2 * n], s[2 * n + 1] = pack(v[0], v[1]), v[2] & 0xFFFF


def put3(s, base, t):
    s[base:base + 3] = [x & M32 for x in t]


def ops_cmds(sfs=(0, 1), lms=(0, 1)):
    """Every opcode x sf x lm; MVMVA with mx 0/3 x v 0/3 x cv 0-3."""
    for op in OPS:
        for sf in sfs:
            for lm in lms:
                if op == 0x12:
                    for mx in (0, 3):
                        for v in (0, 3):
                            for cv in range(4):
                                yield cmdw(op, sf, lm, mx, v, cv)
                else:
                    yield cmdw(op, sf, lm)


def b44():
    mats = ((0x7FFF,) * 9, (-0x8000,) * 9, (0x7FFF, -0x8000, 0x7FFF, -0x8000, 0x7FFF, -0x8000, 0x7FFF, -0x8000, 0x7FFF),
            (0x7FFF, 0x7FFF, 0x7FFF, -0x8000, -0x8000, -0x8000, 0x7FFF, 0x7FFF, 0x7FFF))
    vecs = ((0x7FFF,) * 3, (-0x8000,) * 3, (0x7FFF, -0x8000, 0x7FFF))
    for e in B44:
        for mi, m in enumerate(mats):
            for vi, v in enumerate(vecs):
                s = [0] * 64
                for base in (32, 40, 48):
                    put_mat(s, base, m)
                for n in range(3):
                    put_vec(s, n, v)
                for base in (37, 45, 53):
                    put3(s, base, (e, e, e))
                s[6] = 0xFFFFFFFF if vi != 1 else 0x80808080
                s[8] = 0x7FFF if vi != 1 else -0x8000
                s[9:12] = [x & M32 for x in v]
                s[25:28] = [(e if vi != 1 else -e) & M32] * 3
                s[58], s[59], s[60] = 0x100, 0x7FFF, e & M32
                for c in ops_cmds():
                    yield f"b44 e={e & M32:08x} m{mi} v{vi} cmd={c:x}", c, s


def div():
    for h in DIV_H:
        for sz in sorted({h // 2 - 1, h // 2, h // 2 + 1, (h + 1) // 2, 0}):
            if not 0 <= sz <= 0xFFFF:
                continue
            s = [0] * 64
            put_mat(s, 32, (0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0))
            put3(s, 37, (0x10, -0x20, sz))
            put_vec(s, 0, (0x10, -0x20, 0x30))
            put_vec(s, 1, (-0x7FFF, 0x7FFF, 1))
            put_vec(s, 2, (0x400, 0x300, -0x200))
            s[56], s[57], s[58], s[59], s[60] = 0x1000000, -0x800000 & M32, h, 0x1234, 0x10000
            for op in (0x01, 0x30):
                for sf in (0, 1):
                    for lm in (0, 1):
                        c = cmdw(op, sf, lm)
                        yield f"div h={h:x} sz={sz:x} cmd={c:x}", c, s


class Lcg:
    def __init__(self, seed):
        self.x = seed

    def __call__(self):
        self.x = (self.x * 1664525 + 1013904223) & M32
        return self.x

    def s(self, bits):
        """random signed value of `bits` bits"""
        v = self() >> (32 - bits)
        return v - (1 << bits) if v >> (bits - 1) else v


def tame(r):
    """a state with fields scaled down (matrices 1000h-ish, vectors, TR/BK/FC, offsets in a projection's range)"""
    s = [0] * 64
    for base in (32, 40, 48):
        put_mat(s, base, [r.s(14) for _ in range(9)])
    for n in range(3):
        put_vec(s, n, [r.s(12) for _ in range(3)])
    s[6] = r()
    s[8:12] = [r.s(13) & M32 for _ in range(4)]
    s[12:15] = [pack(r.s(11), r.s(11)) for _ in range(3)]
    s[16:20] = [r() >> 16 for _ in range(4)]
    s[20:23] = [r() for _ in range(3)]
    s[24:28] = [r.s(28) & M32 for _ in range(4)]
    s[30] = r()
    put3(s, 37, [r.s(16), r.s(16), r.s(20)])
    put3(s, 45, [r.s(14) for _ in range(3)])
    put3(s, 53, [r.s(14) for _ in range(3)])
    s[56], s[57] = r.s(27) & M32, r.s(27) & M32
    s[58], s[59], s[60] = r() >> 22, r.s(16) & M32, r.s(26) & M32
    s[61], s[62] = r() >> 20, r() >> 20
    return s


def mvmva():
    r = Lcg(LCG_SEED ^ 0x12)
    states = [tame(r) if k % 2 else [r() for _ in range(64)] for k in range(8)]
    for k, s in enumerate(states):
        for mx in range(4):
            for v in range(4):
                for cv in range(4):
                    for sf in (0, 1):
                        for lm in (0, 1):
                            c = cmdw(0x12, sf, lm, mx, v, cv)
                            yield f"mvmva s{k} cmd={c:x}", c, s


def gpl():
    for mac in (0x7FFFFFFF, 0x7FFFF000, 0x7FFFEFFF, 0x7FFF8000, -0x80000000, -0x7FFFF000, -0x7FFFF001, 0, 0x7FFFFFFE):
        for ir in (0x7FFF, -0x8000, 1, -1):
            for ir0 in (0x7FFF, -0x8000, 1, -1):
                s = [0] * 64
                s[6] = 0x12345678
                s[8] = ir0 & M32
                s[9:12] = [ir & M32, -ir & M32, ir & M32]
                s[25:28] = [mac & M32, mac & M32, -mac & M32]
                for sf in (0, 1):
                    for lm in (0, 1):
                        c = cmdw(0x3E, sf, lm)
                        yield f"gpl mac={mac & M32:08x} ir={ir} ir0={ir0} cmd={c:x}", c, s


def clamp():
    rtp = [0] * 64
    put_mat(rtp, 32, (0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000))
    put3(rtp, 37, (0, 0, 0x100))
    put_vec(rtp, 0, (0x10, 0x20, 0))
    for p in (0x3FE, 0x3FF, 0x400, 0x401, -0x3FF, -0x400, -0x401, -0x402):
        for low in (0, 0xFFFF):
            s = list(rtp)                           # H = 0: q = 0, SX2/SY2 = OFX/OFY SAR 16
            s[56], s[57] = ((p << 16) + low) & M32, ((-p << 16) + low) & M32
            yield f"clamp sxy p={p} low={low:x}", cmdw(0x01), s
    for e in (-1, 0, 1, 0xFFF, 0x1000, 0x1001):
        for low in (0, 0xFFF):
            s = list(rtp)                           # q = 0: IR0 = DQB SAR 12
            s[60] = ((e << 12) + low) & M32
            yield f"clamp ir0 e={e} low={low:x}", cmdw(0x01), s
    for z in (-1, 0, 1, 0xFFFF, 0x10000):
        for sf in (0, 1):
            s = list(rtp)
            s[39] = z & M32                         # SZ3 = TRZ (RT row 3 = 1000h on VZ0 = 0)
            yield f"clamp sz3 z={z} sf={sf}", cmdw(0x01, sf), s
    for t in (0x7FFE, 0x7FFF, 0x8000, -0x7FFF, -0x8000, -0x8001, -1, 0, 1):
        for lm in (0, 1):
            s = [0] * 64                            # MVMVA RT = 0, cv TR, sf 1: MAC = TR
            put3(s, 37, (t, -t, t))
            yield f"clamp ir t={t} lm={lm}", cmdw(0x12, 1, lm), s
    for ir in (-0x10, -0x11, -1, 0, 0xF, 0xFF0, 0xFFF, 0x1000, 0x100F, 0x7FFF, -0x8000):
        for lm in (0, 1):
            s = [0] * 64                            # GPF sf 0, IR0 = 1: MAC = IR, colour = MAC SAR 4
            s[6], s[8] = 0xAB000000, 1
            s[9:12] = [ir & M32, (ir + 1) & M32, (ir - 1) & M32]
            yield f"clamp colour ir={ir} lm={lm}", cmdw(0x3D, 0, lm), s
    for zsf in (0x1000, -0x1000, 0x7FFF, -0x8000, 1, -1):
        for t in (0, 1, 0xFFFE, 0xFFFF, 0x10000, 0x10001, 0x2FFFD):
            s = [0] * 64                            # AVSZ3: OTZ = ZSF3 * (SZ1+SZ2+SZ3) SAR 12
            a = min(t, 0xFFFF)
            b = min(t - a, 0xFFFF)
            s[17:20] = [a, b, t - a - b]
            s[61] = zsf & M32
            yield f"clamp otz zsf={zsf} t={t:x}", cmdw(0x2D), s
    ext = (-0x8000, 0x7FFF, 0)
    for k in range(3 ** 6):                         # NCLIP: MAC0 at the 32-bit bound
        d = [ext[k // 3 ** j % 3] for j in range(6)]
        s = [0] * 64
        s[12:15] = [pack(d[0], d[1]), pack(d[2], d[3]), pack(d[4], d[5])]
        yield f"clamp nclip {k}", cmdw(0x06), s


def lcg():
    r = Lcg(LCG_SEED)
    for op in SUITE_OPS + tuple(op for op in OPS if op not in SUITE_OPS):   # opcodes added later draw last
        for k in range(LCG_PER_OP):
            s = tame(r) if k % 2 else [r() for _ in range(64)]
            b = r()
            c = cmdw(op, b & 1, b >> 1 & 1, *((b >> 2 & 3, b >> 4 & 3, b >> 6 & 3) if op == 0x12 else (0, 0, 0)))
            yield f"lcg {MODELLED_ALL[op]} #{k} cmd={c:x}", c, s


def dpct():
    for f, fifo in enumerate(DPCT_FIFO):
        for k, ir0 in enumerate(DPCT_IR0):
            t = 3 * f + k
            s = [0] * 64
            s[6] = 0x2AE0C0A0                       # RGBC: CODE 2Ah, a colour that no FIFO state holds
            s[8] = ir0
            s[20:23] = fifo
            put3(s, 53, DPCT_FC[(f + k) % 3])
            c = cmdw(0x2A, t & 1, t >> 1 & 1)
            yield f"dpct fifo{f} ir0={ir0:x} fc{(f + k) % 3} cmd={c:x}", c, s


def cases():
    """Every group in order -> [{"id", "cmd", "in"}], "in" in the read view."""
    out = []
    for gen in (b44, div, mvmva, gpl, clamp, dpct, lcg):
        for cid, c, s in gen():
            out.append({"id": cid, "cmd": c, "in": read_view(s)})
    return out


# The suite case set (tests/suite/sh2o_gte_test_suite.c): its 14 opcodes (sh2o_gte_ops), no DPCT.
SUITE_OPS = (0x01, 0x06, 0x10, 0x12, 0x13, 0x16, 0x1B, 0x1E, 0x20, 0x28, 0x2D, 0x30, 0x3D, 0x3E)
SUITE_SEED = 0x53484733
SUITE_PER_OP = 256


def suite_cases():
    """-> [(cmd, read-view input)]: per opcode (ascending) SUITE_PER_OP LCG cases, then the 256 MVMVA selectors."""
    r = Lcg(SUITE_SEED)
    cmds = [op for op in SUITE_OPS for _ in range(SUITE_PER_OP)]
    cmds += [0x12 | (s & 1) << 10 | (s >> 1 & 1) << 19 | (s >> 2 & 3) << 13 | (s >> 4 & 3) << 15 | (s >> 6 & 3) << 17
             for s in range(256)]
    out = []
    for i, c in enumerate(cmds):
        if i < len(SUITE_OPS) * SUITE_PER_OP:
            d = r()
            if c == 0x12:
                c |= (d >> 18 & 3) << 17 | (d >> 20 & 3) << 15 | (d >> 22 & 3) << 13
            c |= (d >> 16 & 1) << 10 | (d >> 17 & 1) << 19
        w = [r() & (0x0FFF0FFF if i & 1 else M32) for _ in range(64)]
        w[63] &= 0x7FFFF000                          # a FLAG write keeps bits 12-30; 15, 29, 31 are never written
        out.append((c, read_view(w)))
    return out


# The write order of the suite and of tests/test_gte.c (sh2o_gte_order in tests/suite/sh2o_gte_test.h): no 15, 29,
# 31 (writes with side effects or no effect); 28 before IR1-3.
ORDER = tuple(range(9)) + (28,) + tuple(range(9, 15)) + tuple(range(16, 28)) + (30,) + tuple(range(32, 64))
UNMODELLED = 0x0C
# Edge cases added by id after the FLAG pairs: an RTPS divide with a normalised SZ at or above 8040h whose result
# changes when the UNR table index is one too low (no other vector catches that mutant).
EXTRA_EDGES = ("div h=100 sz=81 cmd=1",)
HEADER_MAX = 512 * 1024


def gate():
    """MVMVA sf 1, mx 0, v 0 at the fast-path gate of the MAC stage (every |T| < 7FF40000h): T (cv 0) or BK (cv 1)
    in all three places, every matrix and V0-V2 at R and v. T = 7FF3FFFFh with R = v = -8000h is inside (MAC =
    7FFFFFFFh); 7FF40000h is outside (the MAC flags its overflow); -7FF3FFFFh and -7FF40000h with R = -8000h,
    v = 7FFFh; 7FF40000h again in BK (cv 1) with TR 0."""
    for t, r, v, cv in ((0x7FF3FFFF, -0x8000, -0x8000, 0), (0x7FF40000, -0x8000, -0x8000, 0),
                        (-0x7FF3FFFF, -0x8000, 0x7FFF, 0), (-0x7FF40000, -0x8000, 0x7FFF, 0),
                        (0x7FF40000, -0x8000, -0x8000, 1)):
        s = [0] * 64
        for base in (32, 40, 48):
            put_mat(s, base, (r,) * 9)
        for n in range(3):
            put_vec(s, n, (v,) * 3)
        put3(s, 37 + 8 * cv, (t, t, t))
        c = cmdw(0x12, 1, 0, 0, 0, cv)
        yield f"gate t={t & M32:08x} cv={cv}", c, s


def write(s, reg, v):
    """MTC2/CTC2 of v to reg on a storage state s (psx-spx): 15 pushes the SXY FIFO; 28 sets IR1-3 from its 5-bit
    fields << 7; 29 and 31 are read-only; 63 keeps bits 12-30; reg > 63 has no effect."""
    v &= M32
    if reg == 15:
        s[12], s[13], s[14] = s[13], s[14], v
    elif reg == 28:
        s[9:12] = [(v >> 5 * i & 0x1F) << 7 for i in range(3)]
        s[28] = v
    elif reg in (29, 31) or reg > 63:
        return
    elif reg == 63:
        s[63] = v & 0x7FFFF000
    else:
        s[reg] = v


def image(regs):
    """The storage state after the writes of regs in ORDER on a zeroed image."""
    s = [0] * 64
    for reg in ORDER:
        write(s, reg, regs[reg])
    return s


def le(values):
    return b"".join((v & M32).to_bytes(4, "little") for v in values)


def flags(op, out):
    return {(op, b) for b in range(32) if out[63] >> b & 1}


def cmd_vector(c, regs):
    """-> (op, in bytes, out bytes) of op cmd; the image of regs must read back as regs."""
    regs = list(regs)
    if read_view(image(regs)) != regs:
        sys.exit("vectors_gte: an input does not read back through the write order")
    out = step(regs, c)
    rc = 0 if out is not None else -1
    return "cmd", le(regs + [c]), le([rc] + (out if out is not None else regs))


def write_vector(regs, reg, v):
    s = image(regs)
    write(s, reg, v)
    out = read_view(s)
    return "write", le(list(regs) + [reg, v]), le(out + [out[reg] if reg < 64 else 0])


def write_cases(base):
    """(reg, value) register writes on base: extension reads, SXYP push, IRGB/ORGB, LZCS/LZCR, FLAG bit 31,
    read-only registers, registers above 63."""
    yield from ((1, 0x00018000), (5, 0x12347FFF), (7, 0xFFFF8001), (9, 0x00008000), (11, 0xFFFF7FFF),
                (16, 0x8000FFFF), (19, 0xFFFFFFFF), (36, 0x00008000), (58, 0x0000FFFF), (59, 0x12348000),
                (61, 0x00008001), (62, 0xFFFF7FFF))
    yield from ((15, 0x12345678), (15, 0xFC00FC00))
    yield from ((28, 0x7FFF), (28, 0x1234), (28, 0xFFFF8000))
    yield from ((30, 0), (30, M32), (30, 0x00012345), (30, 0x80000000), (30, 0xFFFF0000))
    yield from ((63, M32), (63, 0x00001FFF), (63, 0x00400000), (63, 0x00002000))
    yield from ((29, 0x7FFF), (31, 5), (64, base[0] ^ M32), (255, M32))


def vectors():
    """-> (vector list, ids) in the order of the module docstring."""
    out, ids, covered = [], [], set()

    def add(cid, c, regs):
        vec = cmd_vector(c, regs)
        out.append(vec)
        ids.append(cid)
        o = step(list(regs), c)
        if o is not None:
            covered.update(flags(c & 0x3F, o))
    suite = suite_cases()
    for k, op in enumerate(SUITE_OPS):
        c, regs = suite[k * SUITE_PER_OP + 1]
        add(f"suite {MODELLED_ALL[op]}", c, regs)
    for op in OPS:
        regs = suite[SUITE_OPS.index(op if op in SUITE_OPS else 0x10) * SUITE_PER_OP + 1][1]
        if op == 0x12:
            for sel in range(64):                   # sel = mx:v:cv; sf, lm take each pair in turn
                t = (sel + (sel >> 2)) & 3
                add(f"selector {sel}", cmdw(op, t & 1, t >> 1, sel >> 4, sel >> 2 & 3, sel & 3), regs)
        else:
            for sf in (0, 1):
                for lm in (0, 1):
                    add(f"sf lm {MODELLED_ALL[op]}", cmdw(op, sf, lm), regs)
    for cid, c, s in dpct():
        add(cid, c, read_view(s))
    every, edges = set(), []
    for gen in (b44, div, mvmva, gpl, clamp, dpct, lcg):
        for cid, c, s in gen():
            regs = read_view(s)
            f = flags(c & 0x3F, step(regs, c))
            every |= f
            if gen is not lcg:
                edges.append((cid, c, regs, f))
    while every - covered:
        best = max(edges, key=lambda e: len(e[3] - covered))
        if not best[3] - covered:
            break
        add(best[0], best[1], best[2])
    for cid in EXTRA_EDGES:
        c, regs = next((e[1], e[2]) for e in edges if e[0] == cid)
        add(cid, c, regs)
    for cid, c, s in gate():
        add(cid, c, read_view(s))
    missing = every - covered
    if missing:
        sys.exit(f"vectors_gte: no edge case sets {sorted(missing)}")
    base = suite[1][1]
    for reg, v in write_cases(base):
        out.append(write_vector(base, reg, v))
        ids.append(f"write {reg}")
    out.append(cmd_vector(UNMODELLED, base))
    ids.append("unmodelled")
    return out, ids


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    vecs, ids = vectors()
    size = 4096 + sum(len("    {%d,%d,%d,%s,%s}," % (0, len(i), len(o), c_bytes(i), c_bytes(o))) + 1
                      for _, i, o in vecs)
    if size > HEADER_MAX:
        sys.exit(f"vectors_gte: the header would have about {size} bytes; the maximum is {HEADER_MAX}")
    lines = [
        "# Vectors for the gte family, written by scripts/vectors_gte.py.",
        "# Reference: the Python model scripts/gte_model.py. Registers are the 64 MFC2/CFC2 reads, register 0",
        "# first; cmd writes the input registers in the suite order on a zeroed image.",
        "sh2o-vec 1",
        "family gte",
        "op cmd in regs:u32[64] cmd:u32 out rc:i32 regs:u32[64]",
        "op write in regs:u32[64] reg:u32 val:u32 out regs:u32[64] rd:u32",
    ]
    for (op, i, o), cid in zip(vecs, ids):
        lines.append(f"# {cid}")
        lines.append(f"v {op} {i.hex()} {o.hex()}")
    with open(argv[1], "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print(f"vectors_gte: {len(vecs)} vectors, header about {size} bytes")


if __name__ == "__main__":
    main(sys.argv)
