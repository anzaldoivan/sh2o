#!/usr/bin/env python3
"""Compare the PsyQ 4.7 libgte routines with the game's libgte objects by one digest rule, without either's bytes.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage:
  python3 tests/psyq47_digests.py game [--write]
  python3 tests/psyq47_digests.py archive
  python3 tests/psyq47_digests.py plants

The rule is digest in scripts/psyq_lnk.py: a routine's bytes with its link-time fields and its version word zeroed,
then its relocation sites as text (offset, kind, call/local/data target). tests/psyq47/game_digests.txt holds one
digest per routine of scripts/psyq_lnk.py ROUTINES.

game: the private inputs of tests/psyq_game_check.py (SH2O_PS1_LIBGTE_OBJECTS, SH2O_PS1_LINK_MAP, SH2O_PS1_DATA; a
path in the repository gives exit code 1, no variable or an absent path 77) read by scripts/psyq_model.py, whose
identity digest must be the pinned one. A routine runs from its symbol to the next .text symbol, else to the section
end and then by the pad rule (pad_end in scripts/psyq_lnk.py). Each routine's sites come from its object's .rel.text:
an undefined R_MIPS_26 symbol is a call; a .text symbol is local; any other symbol is data, at its value in the data
object that defines it plus the in-place addend (LO16: the field as s16; HI16: field << 16 plus its paired LO16's s16).
--write writes the digests file; else each digest must equal the file's.
archive: SH2O_PSYQ47_ARCHIVE names the archive of scripts/psyq_lnk.py (unset or absent: exit code 77; with
SH2O_REQUIRE_PSYQ47=1 exit code 1). Each routine's
digest (lnk_sites on its extent) must equal the file's. Each body with an Identity line in include/sh2o/psyq.h is
"identical" (the sha256 of its range from the symbol to the next mark or the section end, no pad rule, equals the
line) or "equal after masking <n> sites". ratan2's one data target
gives 2,050 table bytes, and the 2 bytes before it come from the link of ROUTINES; both sha256 must equal the
header's table lines.
plants: offline, a routine of the two-module LIB of tests/psyq_lnk_check.py: a changed unmasked byte, a site moved
4 bytes and a changed local target each change the digest; a changed masked field does not.
Only names, counts, the first 8 hex of digests and statuses are printed. Exit code 1 on the first failure.
"""
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import psyq_lnk as pl  # noqa: E402
import psyq_model as pm  # noqa: E402
import psyq_game_check as gc  # noqa: E402
import psyq_lnk_check as lc  # noqa: E402

DIGESTS = ROOT / "tests/psyq47/game_digests.txt"
HEADER = ("# One sha256 per routine by the rule `digest` in scripts/psyq_lnk.py, from the private libgte objects.\n"
          "# `tests/psyq47_digests.py game` re-derives them; `game --write` writes this file.\n")
GAME_KIND = {2: "REL32", 4: "REL26", 5: "HI16", 6: "LO16"}
TABLE = 2 * pm.TABLE_ENTRIES


def fail(text):
    print(f"psyq47: FAIL: {text}")
    sys.exit(1)


def committed():
    out = {}
    for line in DIGESTS.read_text().splitlines():
        if line and not line.startswith("#"):
            n, d = line.split()
            out[n] = d
    if sorted(out) != sorted(pl.ROUTINES):
        fail(f"{DIGESTS.name}: {len(out)} routines, not the {len(pl.ROUTINES)} of ROUTINES")
    return out


def data_symbols(m):
    """name -> [offset in its section] over the data objects, each file equal to the one the machine hashed."""
    out = {}
    for p in sorted(Path(os.environ["SH2O_PS1_DATA"]).rglob("*.o")):
        b = p.read_bytes()
        if m.files.get(str(p)) != pm.sha256(b):
            fail("a data object differs from the one the identity digest covers")
        for s in pm.Elf(p, b).syms:
            if s["name"] and 0 < s["shndx"] < 0xff00 and s["type"] not in (3, 4):
                out.setdefault(s["name"], []).append(s["value"])
    return out


def game_sites(e, a, z, data):
    ti, text = e.section(".text")
    rels = e.rels(".text")
    out = []
    for k, (off, info) in enumerate(rels):
        if not a <= off < z:
            continue
        t, s = info & 0xff, e.syms[info >> 8]
        if t not in GAME_KIND:
            raise ValueError(f"relocation type {t} at a site of {s['name'] or 'a section symbol'}")
        w, = struct.unpack_from("<I", text, off)
        if t == pm.R_MIPS_LO16:
            add = pm.s16(w)
        elif t == pm.R_MIPS_HI16:
            lo = next((o for o, i in rels[k + 1:] if i == (info & ~0xff) | pm.R_MIPS_LO16), None)
            if lo is None:
                raise ValueError(f"HI16 of {s['name']} has no paired LO16")
            add = ((w & 0xffff) << 16) + pm.s16(struct.unpack_from("<I", text, lo)[0])
        elif t == pm.R_MIPS_26:
            add = (w & 0x3ffffff) << 2
        else:
            add = w
        if s["shndx"] == 0 and t == pm.R_MIPS_26:
            if add:
                raise ValueError(f"call {s['name']} with an in-place addend")
            target = f"call {s['name']}"
        elif s["shndx"] == ti:
            target = f"local {s['value'] + add - a:x}"
        elif s["shndx"] == 0:
            vals = data.get(s["name"], [])
            if len(vals) != 1:
                raise ValueError(f"data target {s['name']}: defined by {len(vals)} data objects")
            target = f"data {vals[0] + add:x}"
        else:
            target = f"data {s['value'] + add:x}"
        out.append((off - a, GAME_KIND[t], target))
    return out


def game(write):
    gc.private_check()
    m = pm.Machine(*(os.environ[v] for v in gc.VARIABLES), sorted(pl.ROUTINES))
    d = m.identity_digest()
    print(f"psyq47: {len(m.files)} files read, identity digest {d[:8]}")
    if d != gc.IDENTITY_DIGEST:
        fail(f"identity digest {d[:8]}, pinned {gc.IDENTITY_DIGEST[:8]}")
    data = data_symbols(m)
    got = {}
    for name in sorted(pl.ROUTINES):
        e = m.body_object[name]
        _, text = e.section(".text")
        _n, a, z = next(t for t in e.text_symbols() if t[0] == name)
        if z == len(text):
            z = a + pl.pad_end(text[a:z])
        sites = game_sites(e, a, z, data)
        got[name] = pl.digest(text[a:z], sites)
        print(f"psyq47: {name} {len(sites)} sites digest {got[name][:8]}")
    if write:
        DIGESTS.parent.mkdir(exist_ok=True)
        DIGESTS.write_text(HEADER + "".join(f"{n} {got[n]}\n" for n in sorted(got)))
        print(f"psyq47: wrote {len(got)} digests")
        return
    want = committed()
    drift = [n for n in sorted(got) if got[n] != want[n]]
    if drift:
        fail(f"digests differ from the file for {', '.join(drift)}")
    print(f"psyq47: game digests of {len(got)} routines equal the file")


def routine_results(zp):
    """(lib, rows) for the archive zp: one row (name, digest, file digest, how, mod, sites) per routine of ROUTINES in
    name order; how is "differs", "identical", "equal after masking <n> sites" or (no Identity line) "equal"."""
    _member, raw = pl.read_member(zp)
    lib = pl.parse_lib(raw)
    defs = pl.definers(lib)
    want, hdr = committed(), gc.header_ids()
    rows = []
    for name in sorted(pl.ROUTINES):
        mod = pl.module_of(defs, name)
        _i, sec, off = mod.exports[name]
        code = bytes(mod.sections[sec].data[off:off + pl.extent(mod, name)])
        sites = pl.lnk_sites(lib, mod, name)
        d = pl.digest(code, sites)
        if d != want[name]:
            how = "differs"
        elif name in hdr:
            raw = bytes(mod.sections[sec].data[off:off + pl.extent(mod, name, pad=False)])
            how = "identical" if pm.sha256(raw) == hdr[name][0] else f"equal after masking {len(sites)} sites"
        else:
            how = "equal"
        rows.append((name, d, want[name], how, mod, sites))
    return lib, rows


def archive():
    zp = os.environ.get("SH2O_PSYQ47_ARCHIVE")
    if not zp or not Path(zp).is_file():
        if os.environ.get("SH2O_REQUIRE_PSYQ47") == "1":
            print("psyq47: required (SH2O_REQUIRE_PSYQ47=1), SH2O_PSYQ47_ARCHIVE is not set or not there")
            sys.exit(1)
        print("psyq47: SH2O_PSYQ47_ARCHIVE is not set or not there; skipped")
        sys.exit(pl.SKIP)
    lib, rows = routine_results(zp)
    hdr = gc.header_ids()
    equal = 0
    targets = {}
    for name, d, want, how, mod, sites in rows:
        targets[name] = (mod, sites)
        if how == "differs":
            fail(f"{name}: digest {d[:8]}, file {want[:8]}")
        equal += 1
        if name in hdr:
            print(f"psyq47: {name} digest {d[:8]} equal, {how}")
        else:
            print(f"psyq47: {name} digest {d[:8]} equal")
    print(f"psyq47: {equal} of {len(pl.ROUTINES)} digests equal the file")
    mod, sites = targets["ratan2"]
    data = sorted({t for _o, _k, t in sites if t.startswith("data ")})
    if len(data) != 1:
        fail(f"ratan2: {len(data)} data targets, expected 1")
    toff = int(data[0].split()[1], 16)
    _i, sec, off = mod.exports["ratan2"]
    at = off + next(o for o, _k, t in sites if t == data[0])
    leaf = pl.leaves(next(p[3] for p in mod.patches if p[0] == sec and p[2] == at))[0][0]
    if leaf[0] != "base":
        fail("ratan2: its table target is not a section base")
    dsec = leaf[1]
    table = bytes(mod.sections[dsec].data[toff:toff + TABLE])
    image, _syms, _stubs, sec_addr = pl.link(lib, pl.ROUTINES)
    p = sec_addr[mod.name, dsec] + toff - pl.CODE_BASE
    before = image[p - 2:p]
    got = [pm.sha256(table), pm.sha256(before)]
    print(f"psyq47: ratan2 table {got[0][:8]}, 2 bytes before {got[1][:8]}")
    if len(table) != TABLE or got != hdr["ratan2"][1]:
        fail("ratan2's table ranges differ from the header")
    print("psyq47: ratan2's table ranges equal the header")


def plants():
    lib = pl.parse_lib(lc.build(False), "MODA+MODB")
    mod = next(m for m in lib if m.name == "MODA")
    _i, sec, off = mod.exports["FuncA"]
    code = bytes(mod.sections[sec].data[off:off + pl.extent(mod, "FuncA")])
    sites = pl.lnk_sites(lib, mod, "FuncA")
    hi = next(o for o, k, _t in sites if k == "HI16")
    free = next(o for o in range(0, len(code), 4) if all(o != s[0] for s in sites))
    local = sites + [(free, "REL26", "local 8")]
    base = pl.digest(code, local)

    def flip(b, at, mask):
        c = bytearray(b)
        c[at] ^= mask
        return bytes(c)
    cases = [("changed unmasked byte", pl.digest(flip(code, hi + 3, 0x01), local), True),
             ("site moved 4 bytes", pl.digest(code, [(o + 4 if o == free else o, k, t) for o, k, t in local]), True),
             ("changed local target", pl.digest(code, sites + [(free, "REL26", "local c")]), True),
             ("changed masked field", pl.digest(flip(flip(code, hi, 0x5a), free, 0x5a), local), False)]
    print(f"psyq47: plant routine FuncA, {len(local)} sites, digest {base[:8]}")
    for what, d, changes in cases:
        ok = (d != base) == changes
        print(f"psyq47: plant {what}: digest {'changed' if d != base else 'same'}, {'ok' if ok else 'wrong'}")
        if not ok:
            fail(f"plant {what}")
    print(f"psyq47: {len(cases)} plants passed")


def main(argv):
    if len(argv) < 2 or argv[1] not in ("game", "archive", "plants") or argv[2:] not in ([], ["--write"]) \
            or (argv[2:] and argv[1] != "game"):
        sys.exit(__doc__)
    if argv[1] == "game":
        game(argv[2:] == ["--write"])
    elif argv[1] == "archive":
        archive()
    else:
        plants()


if __name__ == "__main__":
    main(sys.argv)
