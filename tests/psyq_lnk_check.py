#!/usr/bin/env python3
"""Offline check of scripts/psyq_lnk.py on a two-module LIB built here (no archive bytes).

Copyright (c) 2026 sh2o Contributors. MIT License.

MODA: .text of two BYTES chunks (patches after the second count from its start): a call to MODB's FuncB (REL26,
an import resolved across modules), a HI16/LO16 pair on ADD(.data base, 0x10), a call to an import no module defines
(a stub), jr ra and zero pad; .data: one word, 4 ZEROES bytes, and a REL32 of its own FuncA. MODB: FuncB, then a
local label 8 bytes on. Checks the split, the records, the extents (pad rule and next symbol), the link addresses
and each patched field. --plant changes the HI16/LO16 VALUE in the built LIB; the check must then fail.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import psyq_lnk as pl  # noqa: E402

TEXT, DATA = 1, 2
JAL, LUI, ADDIU, NOP, JR_RA = 0x0c000000, 0x3c010000, 0x24210000, 0, 0x03e00008


def name(s):
    return bytes([len(s)]) + s.encode()


def u16(v):
    return struct.pack("<H", v)


def u32(v):
    return struct.pack("<I", v)


def words(*ws):
    return b"".join(u32(w) for w in ws)


def bytes_rec(b):
    return b"\x02" + u16(len(b)) + b


def section(idx, nm):
    return b"\x10" + u16(idx) + u16(0) + b"\x08" + name(nm)


def reloc(t, off, expr):
    return b"\x0a" + bytes([t]) + u16(off) + expr


def value(v):
    return b"\x00" + u32(v)


def symbol(i):
    return b"\x02" + u16(i)


def base(s):
    return b"\x04" + u16(s)


def add(a, b):
    return b"\x2c" + b + a          # operands stored as (b, a)


def module(nm, body):
    obj = b"LNK\x02" + b"\x2e\x07" + body + b"\x00"
    exports = b"\x05EXPRT\x00"      # the export list between header and object: skipped
    head = 20 + len(exports)
    return nm.ljust(8).encode() + u32(0) + u32(head) + u32(head + len(obj)) + exports + obj


def build(plant):
    k = 0x11 if plant else 0x10
    a = (section(TEXT, ".text") + section(DATA, ".data") + b"\x1c" + u16(1) + name("a.s")
         + b"\x0c" + u16(3) + u16(TEXT) + u32(0) + name("FuncA")
         + b"\x0e" + u16(4) + name("FuncB") + b"\x0e" + u16(5) + name("Missing")
         + b"\x06" + u16(TEXT) + bytes_rec(words(JAL, NOP)) + reloc(74, 0, symbol(4))
         + bytes_rec(words(LUI, ADDIU, JAL, NOP, JR_RA, NOP, 0, 0))
         + b"\x38" + u16(0) + u32(1)
         + reloc(82, 0, add(base(DATA), value(k))) + reloc(84, 4, add(base(DATA), value(k)))
         + reloc(74, 8, symbol(5))
         + b"\x06" + u16(DATA) + bytes_rec(u32(0x11223344)) + b"\x08" + u32(4) + reloc(16, 4, symbol(3)))
    b = (section(TEXT, ".text") + b"\x0c" + u16(2) + u16(TEXT) + u32(0) + name("FuncB")
         + b"\x12" + u16(TEXT) + u32(8) + name("lb")
         + b"\x06" + u16(TEXT) + bytes_rec(words(JR_RA, NOP, JR_RA, NOP)))
    return b"LIB\x01" + module("MODA", a) + module("MODB", b)


def main(argv):
    plant = "--plant" in argv
    fails = []

    def check(what, got, want):
        if got != want:
            fails.append(f"{what}: got {got!r}, want {want!r}")
    lib = pl.parse_lib(build(plant), "test.lib")
    check("modules", [m.name for m in lib], ["MODA", "MODB"])
    ma, mb = lib
    check("sections", {i: (s.name, len(s.data)) for i, s in ma.sections.items()}, {1: (".text", 40), 2: (".data", 8)})
    check("exports", ma.exports, {"FuncA": (3, TEXT, 0)})
    check("imports", ma.imports, {4: "FuncB", 5: "Missing"})
    check("locals", mb.locals, [(TEXT, 8, "lb")])
    check("patches", [(s, t, o) for s, t, o, _e, _a in ma.patches],
          [(1, 74, 0), (1, 82, 8), (1, 84, 12), (1, 74, 16), (2, 16, 4)])
    check("add expr", ma.patches[1][3], ("add", ("base", DATA), ("value", 0x10)))
    check("extent pad rule", pl.extent(ma, "FuncA"), 32)
    check("extent next symbol", pl.extent(mb, "FuncB"), 8)
    image, syms, stubs, secs = pl.link(lib, ["FuncA"])
    ta, da, tb = 0x80010000, 0x80010028, 0x80010030
    check("placements", secs, {("MODA", 1): ta, ("MODA", 2): da, ("MODB", 1): tb})
    check("FuncB", syms["FuncB"], tb)
    check("stubs", stubs, {"Missing": 0x800f0000})
    w = [struct.unpack_from("<I", image, p)[0] for p in range(0, len(image), 4)]
    target = da + 0x10
    check("REL26 import", w[0], JAL | ((tb >> 2) & 0x3ffffff))
    check("HI16", w[2], LUI | ((target + 0x8000) >> 16))
    check("LO16", w[3], ADDIU | (target & 0xffff))
    check("REL26 stub", w[4], JAL | ((0x800f0000 >> 2) & 0x3ffffff))
    check("data word", w[10], 0x11223344)
    check("REL32", w[11], ta)
    check("image end", len(image), 0x40)
    for f in fails:
        print("psyq_lnk:", f)
    print(f"psyq_lnk: {'FAIL' if fails else 'ok'} ({len(fails)} failed){' [plant]' if plant else ''}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
