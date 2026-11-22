#!/usr/bin/env python3
"""Read the PsyQ 4.7 libgte library (LIB of LNK objects) and link a set of its routines in memory.

Copyright (c) 2026 sh2o Contributors. MIT License.

The formats were read at pcsx-redux src/supportpsx/psyq-obj.h:42-93 (and psyq-obj.cc) at 6b3c25a4 and psyq2elf
src/psyqdump.c at cba9b60d; no text was copied from them. Nothing read from the archive is stored or printed beyond
counts, names, sizes and the stamp byte.

fetch [--dir build/psyq47]: downloads the archive (urllib) to a temp name in the dir, checks its sha256 and size, then
renames it into place. An existing file with the right sha256 is reused; any failure (network, size, sha256, a wrong
existing file) removes the file, prints the reason and exits 77.
list <zip>: the archive's sha256 is checked before any read; the one member whose path ends in LIB/LIBGTE.LIB (any
case) is read in memory, never extracted.

LIB: "LIB", version 1; then modules, each a 20-byte header (name[8] space-padded, u32 date, u32 offset from the module
start to its LNK object, u32 offset to the next module; the bytes between header and object are an export list,
skipped), little-endian.
LNK: "LNK", version 2, then one-byte opcodes up to END. Names are a length byte and that many bytes. A RELOCATION
offset counts from the start of the last BYTES chunk of the current section. Expression operators read their two
operands as (b, a) and mean a + b, a - b, a / b. Every opcode, patch type and expression opcode outside the known
sets raises FormatError naming the value, the module and the file offset.
Extent of an exported symbol of a .text section: from its offset to the next symbol (exported or local) of that
section, else to the section end. Pad rule (pad_end): an extent that runs to its section's end stops after the delay
slot of its last `jr ra` (0x03e00008) when every word past that slot is zero. Both sides of a digest comparison take
it: the game's objects keep that zero pad, the archive's differ in it.
Stamp: the 8-byte pair "Ps" xx 00 00 00 vv 00 in the module that defines InitGeom; vv is the version byte.

link(lib, names): places the modules defining names plus the closure of modules defining their imports, in module
name order, each module's sections in index order and alignment, from 0x80010000; every other import gets an 8-byte
stub from 0x800f0000. Patches replace their field, never add to it: a field holds its expression's own constant or,
with a section base in the expression, the assembler's provisional address; any other field raises.
REL32 = v; REL26 keeps the top 6 bits, low 26 = (v >> 2) & 0x3ffffff, same 256 MiB region as the field;
HI16 = (v + 0x8000) >> 16; LO16 = v & 0xffff. Any other patch type met by the link raises.

digest(code, sites): the one rule that compares a routine across builds. A site is (offset from the routine start,
kind REL32|REL26|HI16|LO16, target). Copy code; zero each site's field (HI16/LO16: the low 16 bits of the
little-endian word; REL26: the low 26 bits; REL32: the whole word); for each 4-aligned p with code[p:p+2] == "Ps",
code[p+3:p+6] zero and code[p+7] zero (the stamp pattern), zero the word at p+4. The digest is the sha256 of the
masked bytes, "\n", then one line "<offset hex> <kind> <target>\n" per site in sorted order. Targets: "call <name>"
for a symbol that the routine's own object does not define (here: an import in a REL26 patch); "local <hex offset
from the routine start, may be negative>" for any target in the routine's own .text section; "data <hex offset in
its section>" for any other target. Any other target raises with its name.
lnk_sites(lib, mod, name): the sites of a routine of mod, from the patches whose field lies in its extent. A target
is section base + constant(expr); an uninitialized symbol is at len(data) + its bss offset; an import in a
HI16/LO16/REL32 patch resolves through definers to its defining module's section offset.
"""
import argparse
import hashlib
import os
import struct
import sys
import urllib.request
import zipfile
from pathlib import Path

URL = "https://github.com/psyqz/psyz/releases/download/requirements/Psy-Q_47.zip"
SHA256 = "e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745"
SIZE = 6103526
ZIP_NAME = "Psy-Q_47.zip"
MEMBER = "lib/libgte.lib"
SKIP = 77
ROUTINES = ("ApplyMatrix", "ApplyMatrixLV", "CompMatrix", "InitGeom", "PopMatrix", "PushMatrix", "RotTrans",
            "SetBackColor", "SetColorMatrix", "SetFarColor", "SetFogNearFar", "SetGeomOffset", "SetGeomScreen",
            "SetLightMatrix", "SetRotMatrix", "SetTransMatrix", "TransMatrix", "TransposeMatrix", "ratan2",
            "SetDQA", "SetDQB")
CODE_BASE = 0x80010000
STUB_BASE = 0x800f0000
LIMIT = 0x80100000
JR_RA = 0x03e00008
REL32, REL26, HI16, LO16 = 16, 74, 82, 84
PATCH_TYPES = {8: "REL32_BE", 12: "GPREL16_BE", 16: "REL32", 30: "GPREL16_LE", 74: "REL26", 82: "HI16",
               84: "LO16", 92: "REL26_BE", 96: "HI16_BE", 98: "LO16_BE", 100: "GPREL16"}
EXPR_LEAF = {0: "value", 2: "symbol", 4: "base", 12: "start", 22: "end"}
EXPR_OP = {44: "add", 46: "sub", 50: "div"}
M32 = 0xffffffff


class FormatError(Exception):
    pass


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(dirpath, url=URL, sha=SHA256, size=SIZE):
    """Download the archive into dirpath; 0 when the file is in place, SKIP on any failure."""
    d = Path(dirpath)
    d.mkdir(parents=True, exist_ok=True)
    dest = d / ZIP_NAME
    if dest.exists():
        if sha256_file(dest) == sha:
            print(f"fetch: {dest} present, sha256 ok")
            return 0
        dest.unlink()
        print(f"fetch: {dest} had a wrong sha256; removed")
        return SKIP
    tmp = d / (ZIP_NAME + ".part")
    try:
        with urllib.request.urlopen(url, timeout=60) as r, open(tmp, "wb") as f:
            while chunk := r.read(1 << 20):
                f.write(chunk)
        got = tmp.stat().st_size
        if got != size:
            raise OSError(f"size {got}, expected {size}")
        digest = sha256_file(tmp)
        if digest != sha:
            raise OSError(f"sha256 {digest}, expected {sha}")
        os.replace(tmp, dest)
    except Exception as e:  # network, size or sha: no file stays behind
        tmp.unlink(missing_ok=True)
        print(f"fetch: failed: {e}")
        return SKIP
    print(f"fetch: {dest} ok ({size} bytes, sha256 ok)")
    return 0


def read_member(zip_path, sha=SHA256):
    """The LIBGTE.LIB bytes of the archive, after its sha256 check."""
    digest = sha256_file(zip_path)
    if digest != sha:
        raise FormatError(f"{zip_path}: sha256 {digest}, expected {sha}")
    with zipfile.ZipFile(zip_path) as z:
        hits = [n for n in z.namelist() if n.lower().replace("\\", "/").endswith(MEMBER)
                and (n.lower() == MEMBER or n.lower().replace("\\", "/").endswith("/" + MEMBER))]
        if len(hits) != 1:
            raise FormatError(f"{zip_path}: {len(hits)} members ending in {MEMBER}")
        return hits[0], z.read(hits[0])


class Reader:
    def __init__(self, data, pos, module, end):
        self.data, self.pos, self.module, self.end = data, pos, module, end

    def take(self, n):
        if self.pos + n > self.end:
            raise FormatError(f"{self.module}: read past the object end at file offset {self.pos:#x}")
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack("<H", self.take(2))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def name(self):
        return self.take(self.u8()).decode("latin-1")

    def fail(self, what, value, at):
        raise FormatError(f"{self.module}: unknown {what} {value} at file offset {at:#x}")


class Section:
    def __init__(self, idx, group, align, name):
        self.idx, self.group, self.align, self.name = idx, group, align, name
        self.data = bytearray()
        self.ptr = 0           # start of the last BYTES chunk: base of RELOCATION offsets
        self.bss = 0           # bytes of UNINITIALIZED symbols after data


class Module:
    def __init__(self, name, offset):
        self.name, self.offset = name, offset
        self.sections = {}
        self.exports = {}      # name -> (idx, section, offset)
        self.imports = {}      # idx -> name
        self.locals = []       # (section, offset, name)
        self.uninit = {}       # name -> (idx, section, size, offset in the section's bss)
        self.symbols = {}      # idx -> ("export"|"import"|"uninit", name)
        self.patches = []      # (section, type, offset, expr, file offset)


def parse_expr(r):
    at = r.pos
    op = r.u8()
    if op in EXPR_LEAF:
        kind = EXPR_LEAF[op]
        return (kind, r.u32() if op == 0 else r.u16())
    if op in EXPR_OP:
        b = parse_expr(r)
        a = parse_expr(r)
        return (EXPR_OP[op], a, b)
    r.fail("expression opcode", op, at)


def parse_lnk(data, start, end, mod):
    r = Reader(data, start, mod.name, end)
    if r.take(3) != b"LNK" or r.u8() != 2:
        raise FormatError(f"{mod.name}: no LNK v2 at file offset {start:#x}")
    cur = None

    def section(i, at):
        if i not in mod.sections:
            raise FormatError(f"{mod.name}: section {i} not declared at file offset {at:#x}")
        return mod.sections[i]
    while True:
        at = r.pos
        op = r.u8()
        if op == 0:
            break
        elif op == 2:
            s = section(cur, at)
            s.ptr = len(s.data)
            s.data += r.take(r.u16())
        elif op == 6:
            cur = r.u16()
        elif op == 8:
            section(cur, at).data += bytes(r.u32())
        elif op == 10:
            t = r.u8()
            if t not in PATCH_TYPES:
                r.fail("patch type", t, at)
            off = r.u16()
            s = section(cur, at)
            mod.patches.append((cur, t, s.ptr + off, parse_expr(r), at))
        elif op == 12:
            idx, sec, off = r.u16(), r.u16(), r.u32()
            n = r.name()
            mod.exports[n] = (idx, sec, off)
            mod.symbols[idx] = ("export", n)
        elif op == 14:
            idx = r.u16()
            n = r.name()
            mod.imports[idx] = n
            mod.symbols[idx] = ("import", n)
        elif op == 16:
            idx, group, align = r.u16(), r.u16(), r.u8()
            mod.sections[idx] = Section(idx, group, align, r.name())
        elif op == 18:
            sec, off = r.u16(), r.u32()
            mod.locals.append((sec, off, r.name()))
        elif op == 28:
            r.u16()
            r.name()
        elif op == 46:
            t = r.u8()
            if t not in (7, 9):
                r.fail("program type", t, at)
        elif op == 48:
            idx, sec, size = r.u16(), r.u16(), r.u32()
            n = r.name()
            mod.uninit[n] = [idx, sec, size, 0]
            mod.symbols[idx] = ("uninit", n)
        elif op in (50, 52, 54, 56, 58, 60):
            r.take({50: 2, 52: 3, 54: 4, 56: 6, 58: 8, 60: 2}[op])
        elif op in (74, 86):
            r.take(32 + (8 if op == 86 else 0))
            r.name()
        elif op in (76, 78, 80):
            r.take(10)
        elif op == 82:
            r.take(14)
            r.name()
        elif op == 84:
            r.take(14)
            r.take(2 * r.u16())
            r.name()
            r.name()
        else:
            r.fail("opcode", op, at)
    # UNINITIALIZED symbols sit after their section's data, in index order, aligned to min(section align, size).
    for n, u in sorted(mod.uninit.items(), key=lambda kv: kv[1][0]):
        s = section(u[1], start)
        a = max(1, min(s.align, u[2]))
        s.bss = (s.bss + a - 1) & -a
        u[3] = s.bss
        s.bss += u[2]
    return mod


def parse_lib(data, fname="LIBGTE.LIB"):
    """The modules of a LIB image, in file order."""
    if data[:3] != b"LIB" or data[3] != 1:
        raise FormatError(f"{fname}: no LIB v1 header")
    mods, pos = [], 4
    while pos < len(data):
        if pos + 20 > len(data):
            raise FormatError(f"{fname}: truncated module header at file offset {pos:#x}")
        name = data[pos:pos + 8].decode("latin-1").rstrip()
        _date, head, size = struct.unpack_from("<III", data, pos + 8)
        if size < head or pos + size > len(data):
            raise FormatError(f"{name}: module size {size} at file offset {pos:#x}")
        mods.append(parse_lnk(data, pos + head, pos + size, Module(name, pos)))
        pos += size
    return mods


def is_code(sec):
    return sec.name == ".text"


def pad_end(code):
    """Length of code, a routine that runs to its section end, after the pad rule."""
    words = [struct.unpack_from("<I", code, p)[0] for p in range(0, len(code) - 3, 4)]
    last = max((k for k, w in enumerate(words) if w == JR_RA), default=None)
    if last is not None and last + 2 <= len(words) and not any(words[last + 2:]):
        return (last + 2) * 4
    return len(code)


def extent(mod, name, pad=True):
    """Bytes of an exported .text symbol, by the extent rule and (pad) the pad rule."""
    _idx, sec, off = mod.exports[name]
    s = mod.sections[sec]
    marks = sorted({o for _i, sc, o in mod.exports.values() if sc == sec}
                   | {o for sc, o, _n in mod.locals if sc == sec})
    nxt = [o for o in marks if o > off]
    if nxt:
        return nxt[0] - off
    return pad_end(bytes(s.data[off:])) if pad else len(s.data) - off


def stamp(mod):
    """The version byte of the one 'Ps' pair in the code and data of mod."""
    hits = []
    for s in mod.sections.values():
        d = s.data
        p = d.find(b"Ps")
        while p >= 0:
            if p + 8 <= len(d) and d[p + 3:p + 6] == b"\0\0\0" and d[p + 7] == 0:
                hits.append(d[p + 6])
            p = d.find(b"Ps", p + 1)
    if len(hits) != 1:
        raise FormatError(f"{mod.name}: {len(hits)} stamp pairs")
    return hits[0]


def definers(lib):
    """name -> modules defining it (export or uninitialized), in file order."""
    out = {}
    for m in lib:
        for n in list(m.exports) + list(m.uninit):
            out.setdefault(n, []).append(m)
    return out


def module_of(defs, name):
    ms = defs.get(name, [])
    if len(ms) > 1:
        raise FormatError(f"{name}: defined by {len(ms)} modules ({', '.join(m.name for m in ms)})")
    return ms[0] if ms else None


def encode(t, v):
    """The field bits of patch type t for value v (REL26: the low 26 bits only)."""
    return {REL32: v & M32, REL26: (v >> 2) & 0x3ffffff, HI16: ((v + 0x8000) >> 16) & 0xffff, LO16: v & 0xffff}[t]


def field_value(t, buf, p):
    if t in (HI16, LO16):
        return struct.unpack_from("<H", buf, p)[0]
    w = struct.unpack_from("<I", buf, p)[0]
    return w & 0x3ffffff if t == REL26 else w


def constant(e):
    """The expression with every symbol and section address taken as 0."""
    if e[0] == "value":
        return e[1]
    if e[0] in EXPR_OP.values():
        a, b = constant(e[1]), constant(e[2])
        return {"add": a + b, "sub": a - b, "div": a // b if b else 0}[e[0]] & M32
    return 0


def has_base(e):
    return e[0] == "base" or (e[0] in EXPR_OP.values() and (has_base(e[1]) or has_base(e[2])))


def field_explained(t, f, e):
    """A field the link may replace: it holds the expression's own constant, or (with a section base in the
    expression) the assembler's provisional address. Anything else would be an addend the expression lacks."""
    return has_base(e) or f == encode(t, constant(e))


KIND = {REL32: "REL32", REL26: "REL26", HI16: "HI16", LO16: "LO16"}
FIELD = {"REL32": M32, "REL26": 0x3ffffff, "HI16": 0xffff, "LO16": 0xffff}


def digest(code, sites):
    """sha256 hex of code with its site fields and version word zeroed, then its sorted sites (module doc)."""
    b = bytearray(code)
    for off, kind, _t in sites:
        if not 0 <= off <= len(b) - 4:
            raise FormatError(f"site {off:#x} outside the routine")
        w = struct.unpack_from("<I", b, off)[0]
        struct.pack_into("<I", b, off, w & ~FIELD[kind] & M32)
    for p in range(0, len(code) - 7, 4):
        if code[p:p + 2] == b"Ps" and code[p + 3:p + 6] == b"\0\0\0" and code[p + 7] == 0:
            b[p + 4:p + 8] = bytes(4)
    text = "".join(f"{o:x} {k} {t}\n" for o, k, t in sorted(sites))
    return hashlib.sha256(bytes(b) + b"\n" + text.encode()).hexdigest()


def leaves(e, sign=1):
    """[(leaf, sign)] of the non-value leaves of e; sign 0 under a division."""
    if e[0] == "value":
        return []
    if e[0] in ("add", "sub"):
        return leaves(e[1], sign) + leaves(e[2], -sign if e[0] == "sub" else sign)
    if e[0] == "div":
        return [(x, 0) for x, _s in leaves(e[1]) + leaves(e[2])]
    return [(e, sign)]


def lnk_sites(lib, mod, name):
    """[(offset from the routine start, kind, target)] of the patches in the extent of name in mod (module doc)."""
    _i, sec, off = mod.exports[name]
    size = extent(mod, name)
    defs = definers(lib)
    out = []
    for ps, t, po, e, at in mod.patches:
        if ps != sec or not off <= po < off + size:
            continue
        lv = leaves(e)
        if t not in KIND or len(lv) != 1 or lv[0][1] != 1 or lv[0][0][0] not in ("base", "symbol"):
            raise FormatError(f"{name}: site {po - off:#x} ({PATCH_TYPES[t]} {e}) fits no target rule")
        c = constant(e)
        c -= (c & 0x80000000) << 1
        leaf, m = lv[0][0], mod
        if leaf[0] == "base":
            s, v = leaf[1], c
        else:
            what, n = mod.symbols.get(leaf[1], (None, None))
            if what == "import":
                if t == REL26:
                    if c:
                        raise FormatError(f"{name}: call {n} with addend {c} fits no target rule")
                    out.append((po - off, KIND[t], f"call {n}"))
                    continue
                m = module_of(defs, n)
                if m is None:
                    raise FormatError(f"{name}: target {n} fits no target rule")
                what = "export" if n in m.exports else "uninit"
            if what == "export":
                _j, s, so = m.exports[n]
                v = so + c
            elif what == "uninit":
                _j, s, _sz, bo = m.uninit[n]
                v = len(m.sections[s].data) + bo + c
            else:
                raise FormatError(f"{name}: symbol {leaf[1]} fits no target rule")
        target = f"local {v - off:x}" if m is mod and s == sec else f"data {v:x}"
        out.append((po - off, KIND[t], target))
    return out


def evaluate(e, mod, sec_addr, sym_addr, at):
    kind = e[0]
    if kind == "value":
        return e[1]
    if kind == "symbol":
        what, n = mod.symbols.get(e[1], (None, None))
        if what is None:
            raise FormatError(f"{mod.name}: symbol {e[1]} undefined at file offset {at:#x}")
        if what == "export":
            _i, sec, off = mod.exports[n]
            return sec_addr[mod.name, sec] + off
        if what == "uninit":
            _i, sec, _sz, off = mod.uninit[n]
            return sec_addr[mod.name, sec] + len(mod.sections[sec].data) + off
        return sym_addr[n]
    if kind == "base":
        return sec_addr[mod.name, e[1]]
    if kind in ("add", "sub", "div"):
        a = evaluate(e[1], mod, sec_addr, sym_addr, at)
        b = evaluate(e[2], mod, sec_addr, sym_addr, at)
        if kind == "add":
            return (a + b) & M32
        if kind == "sub":
            return (a - b) & M32
        if b == 0:
            raise FormatError(f"{mod.name}: division by zero at file offset {at:#x}")
        return (a // b) & M32
    raise FormatError(f"{mod.name}: expression {kind} not supported by the link at file offset {at:#x}")


def link(lib, names):
    """(image bytes from CODE_BASE, symbol -> address, stub name -> address, (module, section) -> address)."""
    defs = definers(lib)
    todo, chosen = [], {}
    for n in names:
        m = module_of(defs, n)
        if m is None:
            raise FormatError(f"{n}: no module defines it")
        todo.append(m)
    while todo:
        m = todo.pop()
        if m.name in chosen:
            continue
        chosen[m.name] = m
        for n in sorted(m.imports.values()):
            d = module_of(defs, n)
            if d is not None and d.name not in chosen:
                todo.append(d)
    order = [chosen[k] for k in sorted(chosen)]
    sec_addr, addr = {}, CODE_BASE
    for m in order:
        for i in sorted(m.sections):
            s = m.sections[i]
            a = max(1, s.align)
            addr = (addr + a - 1) & -a
            sec_addr[m.name, i] = addr
            addr += len(s.data) + s.bss
    end = addr
    if end > STUB_BASE:
        raise FormatError(f"image ends at {end:#x}, past the stub range {STUB_BASE:#x}")
    sym_addr = {}
    for m in order:
        for n, (_i, sec, off) in m.exports.items():
            sym_addr[n] = sec_addr[m.name, sec] + off
        for n, (_i, sec, _sz, off) in m.uninit.items():
            sym_addr[n] = sec_addr[m.name, sec] + len(m.sections[sec].data) + off
    stubs = {}
    for m in order:
        for n in sorted(m.imports.values()):
            if n not in sym_addr:
                stubs[n] = sym_addr[n] = STUB_BASE + 8 * len(stubs)
    if STUB_BASE + 8 * len(stubs) > LIMIT:
        raise FormatError("stub range runs past the argument regions")
    image = bytearray(end - CODE_BASE)
    for m in order:
        for i, s in m.sections.items():
            p = sec_addr[m.name, i] - CODE_BASE
            image[p:p + len(s.data)] = s.data
    for m in order:
        for sec, t, off, expr, at in m.patches:
            v = evaluate(expr, m, sec_addr, sym_addr, at)
            place = sec_addr[m.name, sec] + off
            p = place - CODE_BASE
            if t not in (REL32, REL26, HI16, LO16):
                raise FormatError(f"{m.name}: patch type {PATCH_TYPES[t]} not supported at file offset {at:#x}")
            if not field_explained(t, field_value(t, image, p), expr):
                raise FormatError(f"{m.name}: {PATCH_TYPES[t]} field holds a value its expression does not give "
                                  f"(an in-place addend is not supported) at file offset {at:#x}")
            if t in (HI16, LO16):
                struct.pack_into("<H", image, p, encode(t, v))
                continue
            w = struct.unpack_from("<I", image, p)[0]
            if t == REL32:
                w = v
            else:
                if (v ^ place) & 0xf0000000:
                    raise FormatError(f"{m.name}: REL26 target {v:#x} out of region at file offset {at:#x}")
                w = (w & 0xfc000000) | encode(t, v)
            struct.pack_into("<I", image, p, w)
    return bytes(image), sym_addr, stubs, sec_addr


def text_total(lib):
    """The exported symbols whose section is .text, over all modules of lib."""
    return sum(1 for m in lib for _i, sec, _o in m.exports.values() if is_code(m.sections[sec]))


def listing(zip_path):
    member, data = read_member(zip_path)
    lib = parse_lib(data, member)
    print(f"member: {member}")
    print(f"modules: {len(lib)}")
    print(f"libgte total: {text_total(lib)} (exported symbols whose section is .text, over all modules)")
    every = [(t, field_value(t, m.sections[sec].data, off), e) for m in lib for sec, t, off, e, _a in m.patches
             if t in (REL32, REL26, HI16, LO16)]
    print(f"patch fields replaceable: {sum(field_explained(*x) for x in every)} of {len(every)} "
          f"(REL32/REL26/HI16/LO16 patches, all modules)")
    defs = definers(lib)
    held = set()
    for n in ROUTINES:
        m = module_of(defs, n)
        if m is None:
            raise FormatError(f"{n}: no module defines it")
        held.add(m.name)
        _i, sec, off = m.exports[n]
        size = extent(m, n)
        inside = sum(1 for ps, _t, po, _e, _a in m.patches if ps == sec and off <= po < off + size)
        print(f"  {n}: module {m.name}, extent {size} bytes, patches {inside}")
    print(f"distinct modules: {len(held)}")
    print(f"stamp: {stamp(module_of(defs, 'InitGeom')):#04x}")
    image, syms, stubs, secs = link(lib, ROUTINES)
    placed = {k[0] for k in secs}
    npatch = sum(len(m.patches) for m in lib if m.name in placed)
    print(f"link: {len(placed)} modules, {npatch} patches written, {len(stubs)} stubs, "
          f"image {CODE_BASE:#x}..{CODE_BASE + len(image):#x}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--dir", default="build/psyq47")
    li = sub.add_parser("list")
    li.add_argument("zip")
    a = ap.parse_args(argv)
    if a.cmd == "fetch":
        return fetch(a.dir)
    return listing(a.zip)


if __name__ == "__main__":
    sys.exit(main())
