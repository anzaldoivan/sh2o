#!/usr/bin/env python3
"""Write the vector file of the k family: a Python model of include/sh2o/k.h.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/vectors_k.py tests/vectors/k.vec

The model uses Python integers, which do not overflow. It reads only the
contracts in include/sh2o/k.h and the GTE division of psx-spx, not the C
source. tests/k_bulk_check.py also imports it. Each 64-bit result is stored as
two fields: the low word (u32), then the high word (i32).
"""
import sys

M32 = 0xFFFFFFFF


def s16(value):
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s32(value):
    value &= M32
    return value - 0x100000000 if value & 0x80000000 else value


def dot3b(m, v, tr):
    """(tr << 12) + m . v, exact."""
    return (tr << 12) + m[0] * v[0] + m[1] * v[1] + m[2] * v[2]


def mvb3(w, v, t):
    """Each word holds two int16 values, the low half first; the nine values
    are the 3x3 matrix in row order (the high half of w[4] is not used)."""
    halves = []
    for word in w:
        halves += [s16(word), s16(word >> 16)]
    return [dot3b(halves[3 * i:3 * i + 3], v, t[i]) for i in range(3)]


def gpf3(w):
    """IR(i+1) * IR0; IR0..IR3 are the low halves of w[0..3]."""
    return [s16(w[1 + i]) * s16(w[0]) for i in range(3)]


def gpl3(w, mac, sf):
    """(MAC << 12 when sf, else MAC) + IR(i+1) * IR0; MAC is a signed word."""
    return [(s32(mac[i]) << (12 if sf else 0)) + s16(w[1 + i]) * s16(w[0]) for i in range(3)]


def clz32(value):
    return 32 - (value & M32).bit_length()


# psx-spx, GTE division: unr[i] = max(0, (40000h / (i + 100h) + 1) / 2 - 101h), i 0..100h
UNR = [max(0, (0x40000 // (i + 0x100) + 1) // 2 - 0x101) for i in range(0x101)]


def unr_recip(d):
    """The UNR reciprocal of d, 8000h..FFFFh."""
    u = UNR[(d - 0x7FC0) >> 7] + 0x101
    r = (0x2000080 - d * u) >> 8
    return (0x80 + r * u) >> 8


def unr_div(h, sz):
    """The RTPS/RTPT divide for h < 2 sz: normalise sz to 8000h..FFFFh, then
    (h * recip + 8000h) >> 16, at most 1FFFFh."""
    z = 16 - sz.bit_length()
    return min(0x1FFFF, ((h << z) * unr_recip(sz << z) + 0x8000) >> 16)


# The bulk vectors of the C suite (tests/suite/sh2o_k_test.h): the edge cross
# product (tr outermost, then m, then v, element 0 fastest), then the LCG
# x = x * 1664525 + 1013904223 from the seed: tr, then m0..m2, then v0..v2.
EDGE_TR = (-2 ** 31, -2 ** 31 + 1, -1, 0, 1, 2 ** 31 - 2, 2 ** 31 - 1)
EDGE_S16 = (-32768, -1, 0, 1, 32767)
EDGE_COUNT = len(EDGE_TR) * 125 * 125
EDGE_TR0_COUNT = 125 * 125
RANDOM_COUNT = 4096
SEED = 0x53415432


def bulk_vectors():
    for tr in EDGE_TR:
        for mi in range(125):
            m = (EDGE_S16[mi % 5], EDGE_S16[mi // 5 % 5], EDGE_S16[mi // 25])
            for vi in range(125):
                yield m, (EDGE_S16[vi % 5], EDGE_S16[vi // 5 % 5], EDGE_S16[vi // 25]), tr
    x = SEED
    for _ in range(RANDOM_COUNT):
        draws = []
        for _ in range(7):
            x = (x * 1664525 + 1013904223) & M32
            draws.append(x)
        yield tuple(s16(d) for d in draws[1:4]), tuple(s16(d) for d in draws[4:7]), s32(draws[0])


def bulk_mvb3_inputs(m, v, tr):
    """Rows m, (m1 m2 m0), (m2 m0 m1); the high half of w[4] is A5A5h; t = (tr, ~tr, tr)."""
    u = [value & 0xFFFF for value in m]
    w = [u[0] | u[1] << 16, u[2] | u[1] << 16, u[2] | u[0] << 16, u[2] | u[0] << 16, u[1] | 0xA5A50000]
    return w, v, (tr, ~tr, tr)


def bulk_gp3_inputs(index, m, v, tr):
    """IR0 = m0 (high half A5A5h), IR1..IR3 = v0..v2 (high halves m1, m2, 5A5Ah);
    MAC = (tr, ~tr, tr); sf = bit 0 of the vector index."""
    w = [m[0] & 0xFFFF | 0xA5A50000, v[0] & 0xFFFF | (m[1] & 0xFFFF) << 16,
         v[1] & 0xFFFF | (m[2] & 0xFFFF) << 16, v[2] & 0xFFFF | 0x5A5A0000]
    return w, [tr & M32, ~tr & M32, tr & M32], index & 1


def digest(values):
    """FNV-1a 32 over the 8 bytes of each value, most significant first."""
    h = 0x811C9DC5
    for value in values:
        for byte in (value & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big"):
            h = ((h ^ byte) * 0x01000193) & M32
    return h


OPS = (
    ("dot3b", "in m0:i16 m1:i16 m2:i16 v0:i16 v1:i16 v2:i16 tr:i32 out r_lo:u32 r_hi:i32"),
    ("mvb3", "in w0:u32 w1:u32 w2:u32 w3:u32 w4:u32 v0:i16 v1:i16 v2:i16 t0:i32 t1:i32 t2:i32"
             " out r0_lo:u32 r0_hi:i32 r1_lo:u32 r1_hi:i32 r2_lo:u32 r2_hi:i32"),
    ("gpf3", "in w0:u32 w1:u32 w2:u32 w3:u32 out r0_lo:u32 r0_hi:i32 r1_lo:u32 r1_hi:i32 r2_lo:u32 r2_hi:i32"),
    ("gpl3", "in w0:u32 w1:u32 w2:u32 w3:u32 mac0:u32 mac1:u32 mac2:u32 sf:u8"
             " out r0_lo:u32 r0_hi:i32 r1_lo:u32 r1_hi:i32 r2_lo:u32 r2_hi:i32"),
    ("clz32", "in v:u32 out n:u8"),
    ("unr_recip", "in d:u32 out r:u32"),
    ("unr_div", "in h:u32 sz:u32 out q:u32"),
)


def le(value, size):
    return (value & ((1 << 8 * size) - 1)).to_bytes(size, "little").hex()


def i64(value):
    return le(value, 8)


def lcg(seed):
    x = seed
    while True:
        x = (x * 1664525 + 1013904223) & M32
        yield x


def dot3b_cases(draw):
    cases = [
        ((0, 0, 0), (0, 0, 0), 1),
        ((1, 2, 3), (4, 5, 6), -1),
        ((-1, 0, 0), (1, 0, 0), 0),
        ((-1, 0, 0), (1, 0, 0), 1),  # carry out of the low word
        ((-32768,) * 3, (-32768,) * 3, 2 ** 31 - 1),
        ((32767,) * 3, (-32768,) * 3, -2 ** 31),
        ((0, 0, 5), (0, 7, 3), 0),  # the third product alone: element 2 against element 1
        ((0, 0, -3), (0, 2, 9), -5),
        ((0, 0, 0), (0, 0, 0), -1),  # the sign of tr into the high word
        ((0, 0, 0), (0, 0, 0), -2 ** 31),
        ((0, 0, 0), (0, 0, 0), 2 ** 31 - 1),
        ((0, 0, 0), (0, 0, 0), 0x00100000),  # bit 20 of tr: the first bit of the high word
        ((0, 0, 0), (0, 0, 0), 0x000FFFFF),
        ((1, 1, 1), (-1, -1, -1), 0),
        ((-32768, 32767, -1), (32767, -32768, 1), 7),
        ((-1, -1, -1), (1, 1, 1), 1),  # three carries
        ((32767, 32767, 32767), (32767, 32767, 32767), -2 ** 31 + 1),
    ]
    for tr in EDGE_TR:
        for s in EDGE_S16:
            cases.append(((s, -s if s != -32768 else 32767, s), (s, 1, -s if s != -32768 else 32767), tr))
    for _ in range(40):
        tr = s32(next(draw))
        cases.append((tuple(s16(next(draw)) for _ in range(3)), tuple(s16(next(draw)) for _ in range(3)), tr))
    return cases


def words(m9):
    u = [value & 0xFFFF for value in m9] + [0]
    return [u[2 * i] | u[2 * i + 1] << 16 for i in range(5)]


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    draw = lcg(0x6B766563)
    lines = [
        "# Vectors for the k family, written by scripts/vectors_k.py.",
        "# Reference: the Python model of the contracts in include/sh2o/k.h. Each 64-bit result is",
        "# its low word, then its high word.",
        "sh2o-vec 1",
        "family k",
    ]
    lines += ["op %s %s" % op for op in OPS]

    for m, v, tr in dot3b_cases(draw):
        data = "".join(le(x, 2) for x in m + v) + le(tr, 4)
        lines.append("v dot3b %s %s" % (data, i64(dot3b(m, v, tr))))

    mvb3_cases = [
        (words([1, 2, 3, 4, 5, 6, 7, 8, 9]), (1, -1, 2), (0, -1, 1)),
        (words([-32768] * 9), (-32768,) * 3, (2 ** 31 - 1, -2 ** 31, -1)),
        (words([32767] * 9), (-32768,) * 3, (-2 ** 31, 2 ** 31 - 1, 0)),
        (words([0, 0, 5, 0, 0, -6, 0, 0, 7]), (0, 3, 11), (0, 0, 0)),  # element 2 of each row
        (words([-1, 0, 0, -1, 0, 0, -1, 0, 0]), (1, 0, 0), (1, 1, 1)),  # carries in each row
        (words([4096, 0, 0, 0, 4096, 0, 0, 0, 4096]), (100, -200, 300), (-7, 8, -9)),
    ]
    for _ in range(24):
        w = [next(draw) for _ in range(5)]
        mvb3_cases.append((w, tuple(s16(next(draw)) for _ in range(3)), tuple(s32(next(draw)) for _ in range(3))))
    for w, v, t in mvb3_cases:
        data = "".join(le(x, 4) for x in w) + "".join(le(x, 2) for x in v) + "".join(le(x, 4) for x in t)
        lines.append("v mvb3 %s %s" % (data, "".join(i64(r) for r in mvb3(w, v, t))))

    gp_words = [
        [0x00001000, 0x00001000, 0xFFFFF000, 0x00000001],
        [0x00008000, 0x00008000, 0x00007FFF, 0x0000FFFF],  # -32768 * -32768 = 2^30
        [0x00007FFF, 0x00008000, 0x00007FFF, 0x00000000],
        [0xA5A5FFFF, 0x12340001, 0x5678FFFF, 0x5A5A8000],  # high halves must be ignored
        [0x00000000, 0x00007FFF, 0x00008000, 0x0000FFFF],
    ]
    for _ in range(15):
        gp_words.append([next(draw) for _ in range(4)])
    for w in gp_words:
        lines.append("v gpf3 %s %s" % ("".join(le(x, 4) for x in w), "".join(i64(r) for r in gpf3(w))))
    macs = [[0, 0, 0], [0x7FFFFFFF, 0x80000000, 0xFFFFFFFF], [0x00080000, 0xFFF80000, 0x000FFFFF],
            [1, 0xFFFFFFFF, 0x00100000]]
    for k, w in enumerate(gp_words):
        for sf in (0, 1):
            mac = macs[k % len(macs)] if k < 10 else [next(draw) for _ in range(3)]
            data = "".join(le(x, 4) for x in w + mac) + le(sf, 1)
            lines.append("v gpl3 %s %s" % (data, "".join(i64(r) for r in gpl3(w, mac, sf))))

    clz_inputs = [0, 0xFFFFFFFF]
    for bit in range(32):
        clz_inputs += [1 << bit, (1 << bit) | 1, ((1 << bit) - 1) | (1 << bit)]
    clz_inputs += [next(draw) >> (k % 32) for k in range(32)]
    for value in clz_inputs:
        lines.append("v clz32 %s %s" % (le(value, 4), le(clz32(value), 1)))

    recip_inputs = [0x8000, 0x8001, 0xFFFE, 0xFFFF]
    for k in range(1, 257, 5):  # each change of the table index, (d - 7FC0h) >> 7
        recip_inputs += [0x7FC0 + 128 * k - 1, 0x7FC0 + 128 * k]
    recip_inputs += [0x8000 | next(draw) & 0x7FFF for _ in range(20)]
    for d in recip_inputs:
        if 0x8000 <= d <= 0xFFFF:
            lines.append("v unr_recip %s %s" % (le(d, 4), le(unr_recip(d), 4)))

    div_cases = []
    for sz in (1, 2, 3, 0x7F, 0x80, 0x7FFF, 0x8000, 0x8001, 0xFFFF):
        for h in (0, 1, sz - 1, sz, sz + 1, 2 * sz - 1):
            if 0 <= h < 2 * sz:
                div_cases.append((h, sz))
    for _ in range(30):
        sz = (next(draw) >> (next(draw) % 16)) & 0xFFFF or 1
        div_cases.append((next(draw) % (2 * sz), sz))
    for h, sz in dict.fromkeys(div_cases):
        lines.append("v unr_div %s %s" % (le(h, 4) + le(sz, 4), le(unr_div(h, sz), 4)))

    with open(argv[1], "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv)
