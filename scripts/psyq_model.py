#!/usr/bin/env python3
"""A small R3000 interpreter that runs the PsyQ libgte bodies from their own object bytes.

Copyright (c) 2026 sh2o Contributors. MIT License.

The reference of src/sh2o_psyq.c: it runs each body's own object bytes, read at test time from a private build (the
libgte objects, the link map and the data objects; docs/EXACTNESS.md gives the variables) and never stored here, with
the COP2 commands of scripts/gte_model.py (step, read_view). Nothing in it comes from the C.

Machine(objects_dir, link_map, data_dir) finds every input itself; the script names no file, address or link symbol:
- Link map: the lines "0x<8 hex> <name>"; the first definition of a name wins.
- Body object: the *.s.o of objects_dir whose symbol table defines the body's name. A loaded object loads at the map
  address of its first .text symbol; each of its .text symbols must sit at its map address. A .text symbol ends at
  the next .text symbol or at the section end.
- R_MIPS_26 target that the object does not define: the objects_dir object that defines it is loaded too (and so on);
  else a stub at its map address (it records an event and returns v0 = 0); else an error.
- R_MIPS_HI16/LO16 target: the data object (any *.o below data_dir) whose .data defines it, loaded at the map address
  of its first .data symbol. The matrix stack of PushMatrix/PopMatrix: of the data they reference, the target that
  spans 160 words, after a one-word target (the top word). ratan2's table: the data word it references, 1,025
  halfwords; the 2 bytes before it come from the data object whose placement covers them (loaded too).
- R3000: branch delay slots; load delay slots (lw/lh/lhu/lb/lbu, mfc2/cfc2, mfc0) checked, never modelled: an
  instruction that reads the register loaded just before it raises Hazard. div: a zero divisor gives lo = -1 when the
  dividend >= 0 else +1, hi = dividend; INT32_MIN / -1 gives lo = INT32_MIN, hi = 0. add/addi/sub raise Trap on signed
  overflow. break raises Break(code), code = bits 6-25 (`break 7` is 1c00h, `break 6` 1800h). The caller's return
  address is a sentinel; the run ends when pc reaches it.
- COP2: 64 storage words; MTC2/CTC2/LWC2 store by the psx-spx write rules (15 pushes SXY0-2, 28 sets IR1-3 from its
  5-bit fields << 7, 29 and 31 read-only, 63 keeps bits 12-30); MFC2/CFC2/SWC2 read gte_model.read_view; a command is
  step(read_view, cmd low 25 bits), a command the model does not have raises. COP0 SR (mfc0/mtc0 $12) is a plain word.
identity_digest(): the sha256 of the sorted sha256 lines of every file read. text_sha256(name): the sha256 of a body's
.text bytes (unrelocated). table_bytes(): ratan2's table and the 2 bytes before it, as the machine reads them.
"""
import hashlib
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gte_model as gm  # noqa: E402
import psyq_lnk as pl  # noqa: E402

M32 = 0xffffffff
RET = 0x80000f00          # sentinel return address (not mapped)
SP = 0x801fff00
MAP_LINE = re.compile(r"^\s+0x([0-9a-f]{8})\s+([A-Za-z_]\w*)\s*$", re.M)
STACK_WORDS = 160         # the matrix stack after its top word
TABLE_ENTRIES = 1025      # ratan2's table: the halfwords of the C table
R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 = 4, 5, 6


class Trap(Exception):
    """add/addi/sub signed overflow (the R3000 traps)."""


class Break(Exception):
    """a `break` instruction; .code = the 20-bit BRK code (bits 6-25), .pc its address."""
    def __init__(self, code, pc):
        super().__init__(f"break {code:#x} at {pc:#010x}")
        self.code, self.pc = code, pc


class Hazard(Exception):
    """an instruction in a load delay slot reads the loaded register (not modelled)."""


def s32(v):
    v &= M32
    return v - 0x100000000 if v & 0x80000000 else v


def s16(v):
    v &= 0xffff
    return v - 0x10000 if v & 0x8000 else v


def sha256(b):
    return hashlib.sha256(b).hexdigest()


class Elf:
    """ELF32 LE relocatable: sections, symbols, REL entries."""
    def __init__(self, path, data):
        self.path = path
        self.b = b = data
        shoff, = struct.unpack_from("<I", b, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2e)
        self.sh = [struct.unpack_from("<10I", b, shoff + i * shentsize) for i in range(shnum)]
        names = self.sh[shstrndx]
        self.names = [self._str(names[4], s[0]) for s in self.sh]
        sym = next(i for i, s in enumerate(self.sh) if s[1] == 2)
        strtab = self.sh[self.sh[sym][6]][4]
        self.syms = []
        for k in range(self.sh[sym][5] // 16):
            nm, val, size, info, other, shndx = struct.unpack_from("<IIIBBH", b, self.sh[sym][4] + 16 * k)
            self.syms.append({"name": self._str(strtab, nm), "value": val, "type": info & 0xf, "bind": info >> 4,
                              "shndx": shndx})

    def _str(self, off, i):
        e = self.b.index(b"\0", off + i)
        return self.b[off + i:e].decode()

    def index(self, name):
        return self.names.index(name) if name in self.names else None

    def section(self, name):
        i = self.names.index(name)
        return i, self.b[self.sh[i][4]:self.sh[i][4] + self.sh[i][5]]

    def rels(self, name):
        if ".rel" + name not in self.names:
            return []
        s = self.sh[self.names.index(".rel" + name)]
        return [struct.unpack_from("<II", self.b, s[4] + 8 * k) for k in range(s[5] // 8)]

    def defined(self, section):
        """Named symbols of section (no section/file symbols)."""
        i = self.index(section)
        return [s for s in self.syms if i is not None and s["shndx"] == i and s["type"] not in (3, 4) and s["name"]]

    def text_symbols(self):
        """.text symbols by value -> [(name, start, end)]."""
        _, text = self.section(".text")
        syms = sorted((s["value"], s["name"]) for s in self.defined(".text"))
        return [(n, v, syms[k + 1][0] if k + 1 < len(syms) else len(text)) for k, (v, n) in enumerate(syms)]


class Machine:
    """The body objects, their callees and their data loaded at their map addresses; run(name, args) runs one call."""
    def __init__(self, objects_dir, link_map, data_dir, bodies):
        self.files = {}
        self.map = {}
        for a, n in MAP_LINE.findall(self._read(Path(link_map)).decode("utf-8", errors="replace")):
            self.map.setdefault(n, int(a, 16))
        self.mem, self.entry, self.stubs, self.ranges = {}, {}, {}, {}
        objs = [Elf(p, self._read(p)) for p in sorted(Path(objects_dir).glob("*.s.o"))]
        datas = [Elf(p, self._read(p)) for p in sorted(Path(data_dir).rglob("*.o"))]
        defines = {}
        for e in objs:
            for s in e.syms:
                if s["shndx"] != 0 and s["type"] not in (3, 4) and s["name"]:
                    defines.setdefault(s["name"], e)
        self.body_object = {}
        for name in bodies:
            if name not in defines:
                raise ValueError(f"{objects_dir}: no object defines {name}")
            self.body_object[name] = defines[name]
        self.data_object = {}
        for e in datas:
            for s in e.defined(".data"):
                self.data_object.setdefault(s["name"], e)
        self.refs = {}            # .text symbol -> the HI16/LO16 target names in its bytes
        loaded, todo, data = set(), sorted({e.path for e in self.body_object.values()}), {}
        by_path = {e.path: e for e in objs}
        while todo:
            p = todo.pop(0)
            if p in loaded:
                continue
            loaded.add(p)
            calls, datarefs = self._load_text(by_path[p])
            for t in sorted(calls):
                if t in defines:
                    todo.append(defines[t].path)
                elif t in self.map:
                    self.stubs[self.map[t]] = t
                else:
                    raise ValueError(f"{p.name}: call target {t} is in no object and not in the link map")
            for t in sorted(datarefs):
                if t not in self.data_object:
                    raise ValueError(f"{p.name}: data target {t} is in no data object")
                data[self.data_object[t].path] = self.data_object[t]
        self.placed = {}
        for e in datas:
            first = min(e.defined(".data"), key=lambda s: s["value"], default=None)
            if first is not None and first["name"] in self.map:
                self.placed[e.path] = (e, self.map[first["name"]] - first["value"])
        for e in data.values():
            self._load_data(e)
        self.stack_top, self.stack = self._stack()
        self.table = self._only(self.refs["ratan2"], "ratan2")
        below = [e for e, base in self.placed.values()
                 if base <= self.table - 2 and self.table <= base + len(e.section(".data")[1])]
        if len(below) != 1:
            raise ValueError(f"{len(below)} data objects cover the 2 bytes before ratan2's table")
        if below[0].path not in data:
            self._load_data(below[0])

    def _read(self, path):
        b = path.read_bytes()
        self.files[str(path)] = sha256(b)
        return b

    def identity_digest(self):
        return sha256("".join(h + "\n" for h in sorted(self.files.values())).encode("ascii"))

    def _only(self, names, what):
        if len(names) != 1:
            raise ValueError(f"{what}: {len(names)} data targets, expected 1")
        return self.map[next(iter(names))]

    def _stack(self):
        """(top word address, first stack word address) from the data PushMatrix and PopMatrix reference: the stack is
        the target whose .data symbol spans the 160 words (to the next symbol or the section end), the top the
        one-word target just before it."""
        size = {}
        for n in self.refs["PushMatrix"] | self.refs["PopMatrix"]:
            e = self.data_object[n]
            vals = sorted({s["value"] for s in e.defined(".data")} | {len(e.section(".data")[1])})
            v = next(s["value"] for s in e.defined(".data") if s["name"] == n)
            size[self.map[n]] = vals[vals.index(v) + 1] - v
        stack = [a for a, z in size.items() if z == 4 * STACK_WORDS and size.get(a - 4) == 4]
        if len(stack) != 1:
            raise ValueError(f"matrix stack: {len(stack)} targets span {STACK_WORDS} words after a one-word target")
        return stack[0] - 4, stack[0]

    def text_sha256(self, name):
        e = self.body_object[name]
        _, text = e.section(".text")
        n, a, z = next(t for t in e.text_symbols() if t[0] == name)
        return sha256(text[a:z])

    def table_bytes(self):
        """(ratan2's table, the 2 bytes before it) as the machine reads them."""
        a = self.table
        return (bytes(self.mem.get(a + i, 0) for i in range(2 * TABLE_ENTRIES)),
                bytes(self.mem.get(a - 2 + i, 0) for i in range(2)))

    def _put(self, base, data):
        for i, v in enumerate(data):
            self.mem[base + i] = v

    def _load_data(self, e):
        if e.path not in self.placed:
            raise ValueError(f"{e.path.name}: its first .data symbol is not in the link map")
        self._put(self.placed[e.path][1], e.section(".data")[1])

    def _load_text(self, e):
        """Load e at its map address with its relocations applied -> (call targets, data targets) it does not
        define."""
        ti, text = e.section(".text")
        syms = e.text_symbols()
        base = self.map[syms[0][0]] - syms[0][1]
        for n, a, z in syms:
            if self.map.get(n) != base + a:
                raise ValueError(f"{e.path.name}: {n} not at its map address")
            self.entry[n] = base + a
            self.ranges[n] = (a, z)
            self.refs.setdefault(n, set())
        text = bytearray(text)
        hi, calls, datarefs = {}, set(), set()
        for off, info in e.rels(".text"):
            t, s = info & 0xff, e.syms[info >> 8]
            if s["shndx"] == ti:
                S = base + s["value"] if s["name"] else base
            elif s["shndx"] == 0:
                if s["name"] not in self.map:
                    raise ValueError(f"{e.path.name}: {s['name']} is not in the link map")
                S = self.map[s["name"]]
                if t == R_MIPS_26:
                    calls.add(s["name"])
                else:
                    datarefs.add(s["name"])
                    for n, a, z in syms:
                        if a <= off < z:
                            self.refs[n].add(s["name"])
            else:
                raise ValueError(f"{e.path.name}: relocation against {s}")
            w, = struct.unpack_from("<I", text, off)
            P = base + off
            if t == R_MIPS_26:
                tgt = (((w & 0x3ffffff) << 2) | ((P + 4) & 0xf0000000)) + S
                w = (w & 0xfc000000) | ((tgt >> 2) & 0x3ffffff)
            elif t == R_MIPS_HI16:                       # its LO16 addend: 0 in every libgte object
                hi[info >> 8] = (w & 0xffff) << 16
                v = S + hi[info >> 8]
                w = (w & 0xffff0000) | (((v + 0x8000) >> 16) & 0xffff)
            elif t == R_MIPS_LO16:
                v = S + hi.get(info >> 8, 0) + s16(w)
                w = (w & 0xffff0000) | (v & 0xffff)
            else:
                raise ValueError(f"{e.path.name}: relocation type {t}")
            struct.pack_into("<I", text, off, w)
        self._put(base, text)
        return calls - {n for n, _, _ in syms}, datarefs

    # --- memory
    def rd(self, a, n):
        a &= M32
        if a % n:
            raise ValueError(f"unaligned {n}-byte read at {a:08x}")
        return sum(self.mem.get(a + i, 0) << (8 * i) for i in range(n))

    def wr(self, a, n, v):
        a &= M32
        if a % n:
            raise ValueError(f"unaligned {n}-byte write at {a:08x}")
        for i in range(n):
            self.mem[a + i] = (v >> (8 * i)) & 0xff

    # --- COP2
    def gte_write(self, reg, v):
        v &= M32
        g = self.gte
        if reg == 15:
            g[12], g[13], g[14], g[15] = g[13], g[14], v, v
        elif reg == 28:
            g[28] = v
            g[9], g[10], g[11] = (v & 0x1f) << 7, (v >> 5 & 0x1f) << 7, (v >> 10 & 0x1f) << 7
        elif reg in (29, 31):
            pass
        elif reg == 63:
            g[63] = v & 0x7ffff000
        else:
            g[reg] = v

    def gte_read(self, reg):
        return gm.read_view(self.gte)[reg]

    def gte_command(self, cmd):
        out = gm.step(gm.read_view(self.gte), cmd & 0x1ffffff)
        if out is None:
            raise ValueError(f"unmodelled GTE command {cmd:07x}")
        self.gte = list(out)

    def run(self, name, args=(), gte=None, limit=20000):
        """Call name with a0-a3 = args; -> v0. self.gte: 64 storage words (in/out); self.events, self.sr."""
        r = [0] * 32
        for i, v in enumerate(args):
            r[4 + i] = v & M32
        r[29], r[31] = SP, RET
        if gte is not None:
            self.gte = list(gte)
        self.events, self.sr = [], getattr(self, "sr", 0)
        hi = lo = 0
        pc, npc = self.entry[name], self.entry[name] + 4
        pend = None                                      # register loaded by the previous instruction
        for _ in range(limit):
            if pc == RET:
                return r[2]
            if pc in self.stubs:
                self.events.append((self.stubs[pc],))
                r[2] = 0
                pc, npc = r[31], r[31] + 4
                pend = None
                continue
            w = self.rd(pc, 4)
            op, rs, rt, rd = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31
            sa, fn, imm = w >> 6 & 31, w & 63, w & 0xffff
            simm = s16(imm)
            reads, load = self._reads(w), None
            if pend and pend in reads:
                raise Hazard(f"{name}+{pc - self.entry[name]:x}: reads ${pend} in its load delay slot")
            npc2, wreg, val = npc + 4, None, None
            if op == 0:
                if fn == 0:
                    wreg, val = rd, r[rt] << sa
                elif fn == 2:
                    wreg, val = rd, r[rt] >> sa
                elif fn == 3:
                    wreg, val = rd, s32(r[rt]) >> sa
                elif fn == 4:
                    wreg, val = rd, r[rt] << (r[rs] & 31)
                elif fn == 6:
                    wreg, val = rd, r[rt] >> (r[rs] & 31)
                elif fn == 7:
                    wreg, val = rd, s32(r[rt]) >> (r[rs] & 31)
                elif fn == 8:
                    npc2 = r[rs]
                elif fn == 9:
                    wreg, val, npc2 = rd, pc + 8, r[rs]
                elif fn == 13:
                    raise Break(w >> 6 & 0xfffff, pc)
                elif fn == 16:
                    wreg, val = rd, hi
                elif fn == 17:
                    hi = r[rs]
                elif fn == 18:
                    wreg, val = rd, lo
                elif fn == 19:
                    lo = r[rs]
                elif fn in (24, 25):
                    p = s32(r[rs]) * s32(r[rt]) if fn == 24 else r[rs] * r[rt]
                    lo, hi = p & M32, (p >> 32) & M32
                elif fn == 26:
                    n, d = s32(r[rs]), s32(r[rt])
                    if d == 0:
                        lo, hi = (-1 if n >= 0 else 1) & M32, n & M32
                    elif n == -0x80000000 and d == -1:
                        lo, hi = 0x80000000, 0
                    else:
                        q = abs(n) // abs(d) * (1 if (n < 0) == (d < 0) else -1)
                        lo, hi = q & M32, (n - q * d) & M32
                elif fn == 27:
                    n, d = r[rs], r[rt]
                    lo, hi = (M32, n) if d == 0 else (n // d, n % d)
                elif fn in (32, 34):
                    v = s32(r[rs]) + s32(r[rt]) * (1 if fn == 32 else -1)
                    if not -0x80000000 <= v <= 0x7fffffff:
                        raise Trap(f"{name}+{pc - self.entry[name]:x}: {'add' if fn == 32 else 'sub'} overflow")
                    wreg, val = rd, v
                elif fn == 33:
                    wreg, val = rd, r[rs] + r[rt]
                elif fn == 35:
                    wreg, val = rd, r[rs] - r[rt]
                elif fn == 36:
                    wreg, val = rd, r[rs] & r[rt]
                elif fn == 37:
                    wreg, val = rd, r[rs] | r[rt]
                elif fn == 38:
                    wreg, val = rd, r[rs] ^ r[rt]
                elif fn == 39:
                    wreg, val = rd, ~(r[rs] | r[rt])
                elif fn == 42:
                    wreg, val = rd, int(s32(r[rs]) < s32(r[rt]))
                elif fn == 43:
                    wreg, val = rd, int(r[rs] < r[rt])
                else:
                    raise ValueError(f"{name}+{pc - self.entry[name]:x}: SPECIAL {fn}")
            elif op == 1:
                take = s32(r[rs]) < 0 if rt & 1 == 0 else s32(r[rs]) >= 0
                if rt & 0x10:
                    wreg, val = 31, pc + 8
                if take:
                    npc2 = npc + (simm << 2)
            elif op in (2, 3):
                npc2 = (npc & 0xf0000000) | ((w & 0x3ffffff) << 2)
                if op == 3:
                    wreg, val = 31, pc + 8
            elif op in (4, 5, 6, 7):
                a, b = s32(r[rs]), s32(r[rt])
                take = (a == b, a != b, a <= 0, a > 0)[op - 4]
                if take:
                    npc2 = npc + (simm << 2)
            elif op == 8:
                v = s32(r[rs]) + simm
                if not -0x80000000 <= v <= 0x7fffffff:
                    raise Trap(f"{name}+{pc - self.entry[name]:x}: addi overflow")
                wreg, val = rt, v
            elif op == 9:
                wreg, val = rt, r[rs] + simm
            elif op == 10:
                wreg, val = rt, int(s32(r[rs]) < simm)
            elif op == 11:
                wreg, val = rt, int(r[rs] < (simm & M32))
            elif op == 12:
                wreg, val = rt, r[rs] & imm
            elif op == 13:
                wreg, val = rt, r[rs] | imm
            elif op == 14:
                wreg, val = rt, r[rs] ^ imm
            elif op == 15:
                wreg, val = rt, imm << 16
            elif op in (16, 18):
                sub = rs
                if op == 16 and sub in (0, 4) and rd == 12:          # mfc0 / mtc0 SR
                    if sub == 0:
                        wreg, val, load = rt, self.sr, rt
                    else:
                        self.sr = r[rt]
                elif op == 18 and rs & 0x10:
                    self.gte_command(w)
                elif op == 18 and sub in (0, 2):                     # mfc2 / cfc2
                    wreg, val, load = rt, self.gte_read(rd + (32 if sub == 2 else 0)), rt
                elif op == 18 and sub in (4, 6):                     # mtc2 / ctc2
                    self.gte_write(rd + (32 if sub == 6 else 0), r[rt])
                else:
                    raise ValueError(f"{name}+{pc - self.entry[name]:x}: COP{op - 16} {w:08x}")
            elif op in (32, 33, 35, 36, 37):
                n = {32: 1, 33: 2, 35: 4, 36: 1, 37: 2}[op]
                v = self.rd(r[rs] + simm, n)
                if op in (32, 33):
                    v = v - (1 << (8 * n)) if v >> (8 * n - 1) else v
                wreg, val, load = rt, v, rt
            elif op in (40, 41, 43):
                n = {40: 1, 41: 2, 43: 4}[op]
                self.wr(r[rs] + simm, n, r[rt] & ((1 << (8 * n)) - 1))
            elif op == 50:                                           # lwc2
                self.gte_write(rt, self.rd(r[rs] + simm, 4))
            elif op == 58:                                           # swc2
                self.wr(r[rs] + simm, 4, self.gte_read(rt))
            else:
                raise ValueError(f"{name}+{pc - self.entry[name]:x}: opcode {op}")
            if wreg:
                r[wreg] = val & M32
            pend = load if load else None
            pc, npc = npc, npc2 & M32
        raise ValueError(f"{name}: no return in {limit} steps")

    @staticmethod
    def _reads(w):
        """GPRs w reads (r0 excluded), for the load delay check."""
        op, rs, rt, fn = w >> 26, w >> 21 & 31, w >> 16 & 31, w & 63
        if op == 0:
            if fn in (0, 2, 3):
                s = {rt}
            elif fn in (8, 9, 17, 19):
                s = {rs}
            elif fn in (13, 16, 18):
                s = set()
            else:
                s = {rs, rt}
        elif op == 1 or op in (6, 7) or 8 <= op <= 14 or op in (32, 33, 35, 36, 37, 50, 58):
            s = {rs}
        elif op in (4, 5, 40, 41, 43):
            s = {rs, rt}
        elif op in (16, 18):
            s = {rt} if rs in (4, 6) and not rs & 0x10 else set()
        else:
            s = set()
        return s - {0}


class LinkedMachine(Machine):
    """The bodies linked from the PsyQ 4.7 archive by scripts/psyq_lnk.py (link(lib, ROUTINES)), no map: mem is the
    image at CODE_BASE; entry every exported .text symbol of the modules that define a ROUTINES name (as the game's
    libgte objects); stubs the targets of their REL26 imports outside those modules (the link's stubs and the routines
    of modules the link placed only for its import closure), so those run as in the game machine; refs[routine] the addresses its HI16/LO16 patches evaluate to, outside its own .text. A target's size runs
    to the next mark of its section (its export, local and uninitialized offsets, every placed patch target into it,
    its end with bss). Stack, top and table by Machine's shape rules on those sizes; the halfword before the table is
    0 (the game link's value, from no SDK object)."""
    def __init__(self, zip_path, bodies):
        self.files = {str(zip_path): pl.sha256_file(zip_path)}
        _member, raw = pl.read_member(zip_path)
        lib = pl.parse_lib(raw)
        image, sym_addr, _stubs, sec_addr = pl.link(lib, pl.ROUTINES)
        defs = pl.definers(lib)
        core = {pl.module_of(defs, n).name for n in pl.ROUTINES}
        by_name = {m.name: m for m in lib}
        placed = [by_name[n] for n in sorted({k[0] for k in sec_addr})]
        self.mem = {pl.CODE_BASE + i: v for i, v in enumerate(image)}
        self.entry, self.stubs, self.ranges, self.refs = {}, {}, {}, {}
        spans, marks, hilo = {}, {}, []
        for m in placed:
            for i, s in m.sections.items():
                base = sec_addr[m.name, i]
                spans[m.name, i] = (base, base + len(s.data) + s.bss)
                marks[m.name, i] = ({base + o for _j, sc, o in m.exports.values() if sc == i}
                                    | {base + o for sc, o, _n in m.locals if sc == i}
                                    | {base + len(s.data) + o for _j, sc, _z, o in m.uninit.values() if sc == i}
                                    | {base + len(s.data) + s.bss})
            for n, (_j, sec, off) in m.exports.items():
                if m.name in core and pl.is_code(m.sections[sec]):
                    self.entry[n] = sec_addr[m.name, sec] + off
                    self.ranges[n] = (off, off + pl.extent(m, n, pad=False))
                    self.refs[n] = set()
        for m in placed:
            for sec, t, off, expr, at in m.patches:
                v = pl.evaluate(expr, m, sec_addr, sym_addr, at)
                if t == pl.REL26 and expr[0] == "symbol" and m.name in core:
                    what, n = m.symbols.get(expr[1], (None, None))
                    if what == "import" and n not in self.entry:
                        self.stubs[v] = n
                for key, (a, z) in spans.items():
                    if a <= v < z:
                        marks[key].add(v)
                if t in (pl.HI16, pl.LO16):
                    hilo.append((m, sec, off, v))
        for m, sec, off, v in hilo:
            a, z = spans[m.name, sec]
            if a <= v < z:
                continue
            for n, (_j, s, o) in m.exports.items():
                if s == sec and n in self.ranges and self.ranges[n][0] <= off < self.ranges[n][1]:
                    self.refs[n].add(v)
        missing = sorted(set(bodies) - set(self.entry))
        if missing:
            raise ValueError(f"{zip_path}: no placed module exports {', '.join(missing)}")
        self.size = {}
        for ms in marks.values():
            ms = sorted(ms)
            for a, b in zip(ms, ms[1:]):
                self.size[a] = b - a
        self.stack_top, self.stack = self._stack()
        self.table = self._only(self.refs["ratan2"], "ratan2")
        self.mem[self.table - 2] = self.mem[self.table - 1] = 0

    def _only(self, targets, what):
        if len(targets) != 1:
            raise ValueError(f"{what}: {len(targets)} data targets, expected 1")
        return next(iter(targets))

    def _stack(self):
        """The stack: the target of PushMatrix|PopMatrix spanning 160 words with a 4-byte target just before it."""
        targets = self.refs["PushMatrix"] | self.refs["PopMatrix"]
        stack = [a for a in targets
                 if self.size.get(a) == 4 * STACK_WORDS and a - 4 in targets and self.size.get(a - 4) == 4]
        if len(stack) != 1:
            raise ValueError(f"matrix stack: {len(stack)} targets span {STACK_WORDS} words after a one-word target")
        return stack[0] - 4, stack[0]
