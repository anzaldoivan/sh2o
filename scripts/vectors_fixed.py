#!/usr/bin/env python3
"""Write the vector file of the fixed family from a Python model.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/vectors_fixed.py tests/vectors/fixed.vec

The model follows the contracts in include/sh2o/fixed.h, not the C source.
Python integers have no width, so the model makes the 64-bit product, floors
it with >> and wraps it to 32 bits by hand. The inputs are the edge values and
a fixed xorshift32 sequence, so the file is the same on each run.
"""
import sys

EDGES = [-2147483648, -2147483647, -65536, -32769, -32768, -4097, -4096, -4095, -1,
         0, 1, 4095, 4096, 4097, 32767, 32768, 65536, 2147483646, 2147483647]
RANDOM_MUL = 256
RANDOM_LIM = 128


def wrap32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value >= 0x80000000 else value


def mul_q12(a, b):
    return wrap32((a * b) >> 12)


def lim_ir(value, lm):
    low = 0 if lm else -32768
    return max(low, min(32767, value))


def le32(value):
    return (value & 0xFFFFFFFF).to_bytes(4, "little").hex()


def xorshift32(state):
    while True:
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        yield wrap32(state)


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    lines = [
        "# Vectors for the fixed family, written by scripts/vectors_fixed.py.",
        "# Reference: the Python model of the contracts in include/sh2o/fixed.h.",
        "sh2o-vec 1",
        "family fixed",
        "op mul_q12 in a:i32 b:i32 out r:i32",
        "op lim_ir in value:i32 lm:u8 out r:i32",
    ]
    numbers = xorshift32(0x5348324F)
    pairs = [(a, b) for a in EDGES for b in EDGES]
    pairs += [(next(numbers), next(numbers)) for _ in range(RANDOM_MUL)]
    for a, b in pairs:
        lines.append("v mul_q12 %s%s %s" % (le32(a), le32(b), le32(mul_q12(a, b))))
    limits = [(value, lm) for value in EDGES for lm in (0, 1)]
    limits += [(next(numbers) >> (next(numbers) & 15), next(numbers) & 1) for _ in range(RANDOM_LIM)]
    for value, lm in limits:
        lines.append("v lim_ir %s%02x %s" % (le32(value), lm, le32(lim_ir(value, lm))))
    with open(argv[1], "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv)
