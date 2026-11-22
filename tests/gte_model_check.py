#!/usr/bin/env python3
"""Check the Python GTE model (scripts/gte_model.py) and the case set of the gte suite.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/gte_model_check.py

- Hand cases: the model on cases worked by hand from the formulas of psx-spx; all 64 registers must equal the
  hand values, and each of the 14 opcodes of MODELLED has one. Negatives: another opcode gives another result, and
  an opcode that the model does not have gives None.
- Suite: the case set of tests/suite/sh2o_gte_test_suite.c, made again in Python and run through the model: its
  digest must equal SH2O_DIGEST_GTE of tests/suite/sh2o_gte_test.h, and each of the 14 known-answer literals of
  the suite file must equal the word of the model.
- Entries: each RTPS/RTPT oracle vector (tests/vectors/gte.vec) with sf 1 and lm 0 gives its out through the model
  with the fixed word of sh2o_gte_nrtps (4A180001h) or sh2o_gte_nrtpt (4A280030h).
The script exits 1 on the first failure.
"""
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
import gte_model as gm  # noqa: E402
from vectors_gte import SUITE_OPS, SUITE_PER_OP, suite_cases  # noqa: E402

M32 = 0xFFFFFFFF
# the main result register of each known answer, by opcode
SUITE_KA_REG = {0x01: 14, 0x06: 24, 0x10: 22, 0x12: 25, 0x13: 22, 0x16: 22, 0x1B: 22, 0x1E: 22, 0x20: 22, 0x28: 25,
                0x2D: 7, 0x30: 14, 0x3D: 22, 0x3E: 22}


def cmdw(op, sf=1, lm=0, mx=0, v=0, cv=0):
    """GTE command word (low 25 bits): op 0-5, lm 10, cv 13-14, v 15-16, mx 17-18, sf 19."""
    return op | lm << 10 | cv << 13 | v << 15 | mx << 17 | sf << 19


# Hand cases for the psx-spx model (scripts/gte_model.py), worked from the formulas of psx-spx (and its UNR divide) by
# hand. "in": registers (MFC2/CFC2 view) on an all-zero state, which hand_case() completes through the read view
# (SXYP = SXY2, IRGB/ORGB from IR1-3, LZCR of LZCS 0 = 32); "out": every register the command changes (FLAG, MAC,
# IR, FIFOs, IRGB/ORGB), the rest must equal the input. sf=1 unless named; FLAG bit 31 = OR of bits 30-23, 18-13.
HAND_CASES = [
    # RTPS: RT = identity (RT11 = RT22 = RT33 = 1000h), TR = (0, 0, 100h), V0 = (10h, 20h, 100h), H = 100h.
    # MAC1 = (0 + 1000h*10h) >> 12 = 10h; MAC2 = 20h; MAC3 = (100h << 12 + 1000h*100h) >> 12 = 200h; IR = MAC.
    # SZ3 = 200h (SZ FIFO shifts zeros). UNR: z = 16 - 10 = 6, n = 4000h, d = 8000h, u = unr[0] + 101h = 200h,
    # d = (2000080h - 8000h*200h) >> 8 = 10000h, d = (80h + 10000h*200h) >> 8 = 20000h, q = (4000h*20000h + 8000h)
    # >> 16 = 8000h. SX2 = 10h*8000h >> 16 = 8, SY2 = 20h*8000h >> 16 = 10h. MAC0 = DQA*q + DQB = 0, IR0 = 0.
    # ORGB: 10h >> 7 = 0, 20h >> 7 = 0, 200h >> 7 = 4 -> 4 << 10 = 1000h.
    {"name": "RTPS identity", "cmd": cmdw(0x01),
     "in": {0: 0x00200010, 1: 0x100, 32: 0x1000, 34: 0x1000, 36: 0x1000, 39: 0x100, 58: 0x100},
     "out": {9: 0x10, 10: 0x20, 11: 0x200, 25: 0x10, 26: 0x20, 27: 0x200, 19: 0x200, 14: 0x00100008,
             15: 0x00100008, 28: 0x1000, 29: 0x1000}},
    # RTPS overflow/saturation: V0 = (7FFFh, 7FFFh, 10h), TRY = 1000h, RT identity, H = 100h.
    # MAC1 = 7FFFh; MAC2 = (1000h << 12 + 1000h*7FFFh) >> 12 = 8FFFh -> IR2 = 7FFFh (FLAG 23); MAC3 = 10h.
    # SZ3 = 10h; H < 2*SZ3 false -> q = 1FFFFh, FLAG 17 (+31). SX: 7FFFh*1FFFFh = FFFD8001h > 7FFFFFFFh -> MAC0+
    # (16); >> 16 = FFFDh > 3FFh -> SX2 = 3FFh (14). SY the same (13). MAC0 = DQA*q + DQB = 0, IR0 = 0.
    # FLAG = 31|23|17|16|14|13 = 80836000h. ORGB: 1Fh, 1Fh, 0 -> 3FFh.
    {"name": "RTPS overflow", "cmd": cmdw(0x01),
     "in": {0: 0x7FFF7FFF, 1: 0x10, 32: 0x1000, 34: 0x1000, 36: 0x1000, 38: 0x1000, 58: 0x100},
     "out": {9: 0x7FFF, 10: 0x7FFF, 11: 0x10, 25: 0x7FFF, 26: 0x8FFF, 27: 0x10, 19: 0x10, 14: 0x03FF03FF,
             15: 0x03FF03FF, 28: 0x3FF, 29: 0x3FF, 63: 0x80836000}},
    # RTPT: RT identity, TR 0, H = 100h, DQA = 100h, DQB = 0. V0 = (0, 0, 100h): SZ 100h, q = 10000h (z = 7,
    # n = d = 8000h, u = 200h, d -> 20000h, q = (8000h*20000h + 8000h) >> 16 = 10000h), SXY = (0, 0).
    # V1 = (10h, 0, 100h): q = 10000h, SX = 10h. V2 = (0, 20h, 200h): q = 8000h, SY = 10h.
    # Last: MAC0 = 100h*8000h = 800000h, IR0 = 800000h >> 12 = 800h. MAC/IR from V2 = (0, 20h, 200h).
    {"name": "RTPT", "cmd": cmdw(0x30),
     "in": {1: 0x100, 2: 0x10, 3: 0x100, 4: 0x00200000, 5: 0x200, 32: 0x1000, 34: 0x1000, 36: 0x1000, 58: 0x100,
            59: 0x100},
     "out": {9: 0, 10: 0x20, 11: 0x200, 25: 0, 26: 0x20, 27: 0x200, 17: 0x100, 18: 0x100, 19: 0x200, 12: 0,
             13: 0x10, 14: 0x00100000, 15: 0x00100000, 8: 0x800, 24: 0x800000, 28: 0x1000, 29: 0x1000}},
    # NCLIP: SXY0 = (0, 0), SXY1 = (10h, 0), SXY2 = (0, 10h): MAC0 = SX1*SY2 = 100h, other products 0.
    {"name": "NCLIP", "cmd": cmdw(0x06), "in": {13: 0x10, 14: 0x00100000}, "out": {24: 0x100}},
    # AVSZ3: ZSF3 = 155h, SZ1-3 = 300h: MAC0 = 155h*900h = BFD00h, OTZ = BFD00h >> 12 = BFh.
    {"name": "AVSZ3", "cmd": cmdw(0x2D), "in": {61: 0x155, 17: 0x300, 18: 0x300, 19: 0x300},
     "out": {24: 0xBFD00, 7: 0xBF}},
    # AVSZ3 overflow: ZSF3 = -8000h, SZ1 = SZ2 = FFFFh, SZ3 = 1: sum 1FFFFh, MAC0 = -FFFF8000h < -80000000h ->
    # MAC0- (15); MAC0 wrapped = 00008000h; OTZ from the sum (< 0) = 0, FLAG 18 (+31): 80048000h.
    # (From the wrapped MAC0 OTZ would be 8: the hardware's reading per ps1-tests, gte_model docstring.)
    {"name": "AVSZ3 overflow", "cmd": cmdw(0x2D), "in": {61: 0xFFFF8000, 17: 0xFFFF, 18: 0xFFFF, 19: 1},
     "out": {24: 0x8000, 7: 0, 63: 0x80048000}},
    # SQR sf=1: IR = (100h, -200h, 4000h): MAC1 = 10000h >> 12 = 10h, MAC2 = 40000h >> 12 = 40h,
    # MAC3 = 10000000h >> 12 = 10000h -> IR3 = 7FFFh (FLAG 22; bit 22 is not in bit 31's set). ORGB: 7C00h.
    {"name": "SQR", "cmd": cmdw(0x28), "in": {9: 0x100, 10: 0xFFFFFE00, 11: 0x4000},
     "out": {25: 0x10, 26: 0x40, 27: 0x10000, 9: 0x10, 10: 0x40, 11: 0x7FFF, 28: 0x7C00, 29: 0x7C00,
             63: 0x00400000}},
    # GPF: IR0 = 800h, IR = (1000h, 2000h, -1000h): MAC = IR*IR0 >> 12 = (800h, 1000h, -800h) = IR.
    # Colour: MAC >> 4 = 80h, 100h -> FFh (FLAG 20), -80h -> 0 (FLAG 19); CODE 2Ch: RGB2 = 2C00FF80h, RGB0/1 <-
    # RGB1/2 (111h, 222h). FLAG 20|19 = 180000h (not in bit 31's set). ORGB: 10h, 1Fh, 0 -> 3F0h.
    {"name": "GPF", "cmd": cmdw(0x3D),
     "in": {6: 0x2C000000, 8: 0x800, 9: 0x1000, 10: 0x2000, 11: 0xFFFFF000, 21: 0x111, 22: 0x222},
     "out": {25: 0x800, 26: 0x1000, 27: 0xFFFFF800, 9: 0x800, 10: 0x1000, 11: 0xFFFFF800, 20: 0x111, 21: 0x222,
             22: 0x2C00FF80, 28: 0x3F0, 29: 0x3F0, 63: 0x180000}},
    # GPL: MAC1 = 100h, IR0 = 1000h, IR = (10h, 20h, 0): MAC1 = (100h << 12 + 1000h*10h) >> 12 = 110h,
    # MAC2 = 20h, MAC3 = 0. RGB2 = (11h, 2, 0) = 211h. ORGB: 110h >> 7 = 2.
    {"name": "GPL", "cmd": cmdw(0x3E), "in": {25: 0x100, 8: 0x1000, 9: 0x10, 10: 0x20},
     "out": {25: 0x110, 26: 0x20, 9: 0x110, 10: 0x20, 22: 0x211, 28: 2, 29: 2}},
    # DPCS: RGBC = (80h, 40h, 0, CODE 30h), FC = (1000h, 400h, 0), IR0 = 800h.
    # IR = ((FC << 12) - (RGB << 16)) >> 12 = (1000000h - 800000h) >> 12 = 800h, 0, 0.
    # MAC = (IR*IR0 + RGB << 16) >> 12: (400000h + 800000h) >> 12 = C00h, 400000h >> 12 = 400h, 0.
    # RGB2 = (C0h, 40h, 0, 30h) = 300040C0h. ORGB: 18h, 8, 0 -> 118h.
    {"name": "DPCS", "cmd": cmdw(0x10), "in": {6: 0x30004080, 8: 0x800, 53: 0x1000, 54: 0x400},
     "out": {25: 0xC00, 26: 0x400, 9: 0xC00, 10: 0x400, 22: 0x300040C0, 28: 0x118, 29: 0x118}},
    # DPCT: RGBC = (0, 0, 0, CODE 30h), FC = 0, IR0 = 800h; RGB0 = (10h, 20h, 30h), RGB1 = (40h, 0, 0),
    # RGB2 = (0, 80h, 0). Each step on RGB0 = c: IR = (0 - (c << 16)) >> 12 = -(c << 4) (lm=0 range);
    # MAC = (-(c << 4)*800h + (c << 16)) >> 12 = c << 3; FIFO push c << 3 >> 4 = c / 2, CODE 30h.
    # Step 1: (10h, 20h, 30h) -> push (8, 10h, 18h); RGB0 = (40h, 0, 0). Step 2: push (20h, 0, 0);
    # RGB0 = (0, 80h, 0). Step 3: MAC = IR = (0, 400h, 0), push (0, 40h, 0). RGB0-2 = 30181008h, 30000020h,
    # 30004000h. ORGB: 8 << 5 = 100h.
    {"name": "DPCT", "cmd": cmdw(0x2A), "in": {6: 0x30000000, 8: 0x800, 20: 0x00302010, 21: 0x40, 22: 0x8000},
     "out": {26: 0x400, 10: 0x400, 20: 0x30181008, 21: 0x30000020, 22: 0x30004000, 28: 0x100, 29: 0x100}},
    # MVMVA mx=0 v=0 cv=0: RT11 = RT12 = 1000h, RT22 = 800h, RT33 = 1000h, V0 = (10h, 20h, 30h), TR = (1, 2, 3):
    # MAC1 = 1 + 10h + 20h = 31h, MAC2 = 2 + 800h*20h >> 12 = 12h, MAC3 = 3 + 30h = 33h; IR = MAC; ORGB 0.
    {"name": "MVMVA RT V0 TR", "cmd": cmdw(0x12),
     "in": {0: 0x00200010, 1: 0x30, 32: 0x10001000, 34: 0x800, 36: 0x1000, 37: 1, 38: 2, 39: 3},
     "out": {25: 0x31, 26: 0x12, 27: 0x33, 9: 0x31, 10: 0x12, 11: 0x33}},
    # MVMVA overflow, mx=1 (L) v=3 (IR) cv=1 (BK): BK1 = 7FFFFFFFh, L11 = 7FFFh, L12 = -8000h, IR = (7FFFh, 7FFFh, 0).
    # BK1 << 12 = 7FFFFFFF000h; + 7FFFh*7FFFh (3FFF0001h) = 8003FFEF001h >= 2^43 -> MAC1+ (30), truncated to 44 bits
    # = -7FFC0010FFFh; + (-8000h*7FFFh = -3FFF8000h) = -80000008FFFh < -2^43 -> MAC1- (27), truncated =
    # 7FFFFFF7001h; + 0. MAC1 = 7FFFFFF7001h >> 12 = 7FFFFFF7h -> IR1 = 7FFFh (24). MAC2 = MAC3 = 0.
    # FLAG = 31|30|27|24 = C9000000h. ORGB: 1Fh.
    {"name": "MVMVA 44-bit overflow", "cmd": cmdw(0x12, mx=1, v=3, cv=1),
     "in": {9: 0x7FFF, 10: 0x7FFF, 40: 0x80007FFF, 45: 0x7FFFFFFF},
     "out": {25: 0x7FFFFFF7, 9: 0x7FFF, 10: 0, 28: 0x1F, 29: 0x1F, 63: 0xC9000000}},
    # MVMVA cv=2 (FC) bug: RFC = 10000h, RT11 = RT12 = 1000h, V0 = (10h, 20h, 0). Partial (10000h << 12 +
    # 1000h*10h) >> 12 = 10010h > 7FFFh -> FLAG 24 (+31); result only the last two portions: MAC1 = 1000h*20h >> 12
    # = 20h; MAC2 = MAC3 = 0.
    {"name": "MVMVA cv=2 FC bug", "cmd": cmdw(0x12, cv=2),
     "in": {0: 0x00200010, 32: 0x10001000, 53: 0x10000},
     "out": {25: 0x20, 9: 0x20, 63: 0x81000000}},
    # NCS lm=1: L = LCM = identity, BK = (10h, 0, 0), V0 = (800h, 400h, -100h), CODE 20h.
    # L*V0 = (800h, 400h, -100h) -> IR3 = 0 (lm, FLAG 22, not in bit 31's set). BK*1000h + LCM*IR: MAC1 = 10h + 800h = 810h,
    # MAC2 = 400h, MAC3 = 0. RGB2 = (81h, 40h, 0, 20h) = 20004081h. ORGB: 10h, 8, 0 -> 110h.
    {"name": "NCS", "cmd": cmdw(0x1E, lm=1),
     "in": {0: 0x04000800, 1: 0xFFFFFF00, 6: 0x20000000, 40: 0x1000, 42: 0x1000, 44: 0x1000, 48: 0x1000,
            50: 0x1000, 52: 0x1000, 45: 0x10},
     "out": {25: 0x810, 26: 0x400, 27: 0, 9: 0x810, 10: 0x400, 11: 0, 22: 0x20004081, 28: 0x110, 29: 0x110,
             63: 0x00400000}},
    # NCT: L = LCM = identity, BK 0, V0 = (100h, 0, 0), V1 = (0, 200h, 0), V2 = (0, 0, 300h): MAC = V each,
    # colour MAC >> 4: RGB0 = 10h, RGB1 = 20h << 8, RGB2 = 30h << 16. MAC/IR from V2; ORGB: 300h >> 7 = 6 -> 1800h.
    {"name": "NCT", "cmd": cmdw(0x20),
     "in": {0: 0x100, 2: 0x02000000, 5: 0x300, 40: 0x1000, 42: 0x1000, 44: 0x1000, 48: 0x1000, 50: 0x1000,
            52: 0x1000},
     "out": {20: 0x10, 21: 0x2000, 22: 0x300000, 27: 0x300, 11: 0x300, 28: 0x1800, 29: 0x1800}},
    # NCDS: L = LCM = identity, BK 0, V0 = (1000h, 800h, 0), RGBC = (80h, 80h, 80h, 34h), FC 0, IR0 = 800h.
    # IR = (1000h, 800h, 0); base = (R*IR1) << 4 = 800000h, 400000h, 0. IR = (0 - base) >> 12 = -800h, -400h, 0.
    # MAC = (IR*IR0 + base) >> 12 = (-400000h + 800000h) >> 12 = 400h, (-200000h + 400000h) >> 12 = 200h, 0.
    # RGB2 = (40h, 20h, 0, 34h) = 34002040h. ORGB: 8, 4 -> 88h.
    {"name": "NCDS", "cmd": cmdw(0x13),
     "in": {0: 0x08001000, 6: 0x34808080, 8: 0x800, 40: 0x1000, 42: 0x1000, 44: 0x1000, 48: 0x1000, 50: 0x1000,
            52: 0x1000},
     "out": {25: 0x400, 26: 0x200, 27: 0, 9: 0x400, 10: 0x200, 11: 0, 22: 0x34002040, 28: 0x88, 29: 0x88}},
    # NCDT: identity L/LCM, BK 0, FC 0, IR0 0, RGBC = (40h, 40h, 40h, 0); V0 = (1000h, 0, 0), V1 = (0, 1000h, 0),
    # V2 = (0, 0, 1000h): base = 40h*1000h << 4 = 400000h; MAC = (IR*0 + base) >> 12 = 400h -> colour 40h.
    # RGB0 = 40h, RGB1 = 4000h, RGB2 = 400000h; MAC/IR from V2 = (0, 0, 400h); ORGB: 8 << 10 = 2000h.
    {"name": "NCDT", "cmd": cmdw(0x16),
     "in": {0: 0x1000, 2: 0x10000000, 5: 0x1000, 6: 0x00404040, 40: 0x1000, 42: 0x1000, 44: 0x1000, 48: 0x1000,
            50: 0x1000, 52: 0x1000},
     "out": {20: 0x40, 21: 0x4000, 22: 0x400000, 27: 0x400, 11: 0x400, 28: 0x2000, 29: 0x2000}},
]



def hand_case(h):
    """A hand case -> (cmd, in, out): in = the read view of the zero state + its "in", out = in + its "out"."""
    s = [0] * 64
    for r, v in h["in"].items():
        s[r] = v
    inp = gm.read_view(s)
    out = list(inp)
    for r, v in h["out"].items():
        out[r] = v
    return h["cmd"], inp, out


def fail(text):
    sys.exit("gte_model: FAIL: " + text)


def check_hand():
    for h in HAND_CASES:
        cmd, inp, want = hand_case(h)
        got = gm.step(inp, cmd)
        diff = {r: f"{got[r]:08x}" for r in range(64) if got[r] != want[r]}
        if diff:
            fail(f"hand case {h['name']}: registers {diff}")
    names = {gm.MODELLED_ALL[h["cmd"] & 0x3F] for h in HAND_CASES}
    if names != set(gm.MODELLED.values()):
        fail(f"the hand cases cover {sorted(names)}, not each opcode of MODELLED")
    cmd, inp, want = hand_case(HAND_CASES[0])
    if gm.step(inp, cmd & ~0x3F | 0x30) == want:
        fail("negative: RTPT gives the result of RTPS")
    if gm.step(inp, 0x0C) is not None:
        fail("negative: opcode 0Ch gives a result")
    print(f"gte_model: {len(HAND_CASES)} hand cases equal, 2 negatives rejected")


def digest(outs):
    h = 0x811C9DC5
    for o in outs:
        for v in o:
            for b in (v & M32).to_bytes(4, "big"):
                h = ((h ^ b) * 0x01000193) & M32
    return h


def check_suite():
    header = (ROOT / "tests/suite/sh2o_gte_test.h").read_text(encoding="utf-8")
    pin = int(re.search(r"#define SH2O_DIGEST_GTE (0x[0-9A-F]{8})u", header).group(1), 16)
    cases = suite_cases()
    outs = [gm.step(i, c) for c, i in cases]
    h = digest(outs)
    print(f"gte_model: suite: {len(cases)} cases, model digest {h:#010x}, pinned {pin:#010x}")
    if len(cases) != 14 * SUITE_PER_OP + 256:
        fail(f"the suite has {len(cases)} cases")
    if sorted({c & 0x7E03F for c, _ in cases if c & 0x3F == 0x12}) != sorted({0x12 | k << 13 for k in range(64)}):
        fail("the suite does not have each MVMVA mx x v x cv")
    if h != pin:
        fail("the model digest differs from the pin")
    text = (ROOT / "tests/suite/sh2o_gte_test_suite.c").read_text(encoding="utf-8")
    for k, op in enumerate(SUITE_OPS):
        r = SUITE_KA_REG[op]
        literal = f"{{{k}*SH2O_GTE_PER_OP+1,{r},0x{outs[k * SUITE_PER_OP + 1][r]:08X}u}}"
        if literal not in text:
            fail(f"known answer {gm.MODELLED_ALL[op]}: the suite file has no {literal}")
    print(f"gte_model: suite: {len(SUITE_OPS)} known answers equal the model")


def check_entries():
    words = {0x01: 0x4A180001, 0x30: 0x4A280030}
    counts = {0x01: 0, 0x30: 0}
    for line in (ROOT / "tests/vectors/gte.vec").read_text(encoding="ascii").splitlines():
        if not line.startswith("v cmd "):
            continue
        i, o = (bytes.fromhex(x) for x in line.split()[2:4])
        w = [int.from_bytes(i[4 * k:4 * k + 4], "little") for k in range(65)]
        regs, cmd = w[:64], w[64]
        op = cmd & 0x3F
        if op not in words or cmd & 0x80400 != 0x80000:
            continue
        want = [int.from_bytes(o[4 * k:4 * k + 4], "little") for k in range(1, 65)]
        if gm.step(regs, words[op]) != want:
            fail(f"entry word {words[op]:#010x}: the model differs from the vector of cmd {cmd:#010x}")
        counts[op] += 1
    if not counts[0x01] or not counts[0x30]:
        fail(f"entries: no vector with sf 1, lm 0 for an entry ({counts})")
    print(f"gte_model: entries: {counts[0x01]} RTPS and {counts[0x30]} RTPT vectors equal with the fixed words")


def main():
    start = time.perf_counter()
    check_hand()
    check_suite()
    check_entries()
    print(f"gte_model: passed in {time.perf_counter() - start:.1f} s")


if __name__ == "__main__":
    main()
