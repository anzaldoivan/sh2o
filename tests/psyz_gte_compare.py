#!/usr/bin/env python3
"""Compare the sh2o GTE machine with the psyz software GTE, every differing register named.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/psyz_gte_compare.py [--recordings] <cc> <source dir> <work dir> <gte program> [<psyz dir>]

CTest gives the arguments (CMakeLists.txt, tests psyz_gte and psyz_gte_recordings). The psyz dir is
<source dir>/external/psyz when not given. The script compiles the psyz GTE sources (PSYZ_SRCS) in place, unchanged
(-std=gnu11 -O2 -w -D__psyz, psyz headers), and links them with tests/psyz/psyz_gte_driver.c, which reads and writes
the lines of test_gte --driver. The gte program is a tests/test_gte.c build, run with --driver.

The cases: the 1,600 ps1-tests cases of tests/gte_sources_check.py, and with --recordings also its clean recorded PS1
GTE command pairs. One psyz process runs the cases, then each write-effect variant (VARIANTS) of the ps1-tests
cases: the case replayed from the hardware state after its writes, with only some of the write side effects
(psyz keeps state between lines, so the order is fixed). Each register where sh2o and psyz differ is named by
sh2o_explain, the reading of the record of the original port: a psyz cause (psyz against the expected value, by
write effect, read or store view, or command), psyz alone wrong where sh2o, the model and the expected value agree,
or a model cause. A difference with no name is UNEXPLAINED; there must be none.

Without --recordings the script also runs psyz on the 230 cases of tests/ps1-tests/gte_sample.txt and names each
register where psyz differs from the expected value (cause()); each is compared with the cause the sample records.
The 70 sample cases with a command sh2o lacks run on psyz only; each other sample case must equal a line of
tests/ps1-tests/gte_cases.txt.

The plant (each run): one bit of one sh2o output word flipped, at a register where sh2o, psyz, the model and the
expected value agree; the explanation must then give exactly 1 unexplained register.

The counts are pinned (PUBLIC, SAMPLE, RECORDINGS) and asserted when the psyz dir is the default one; with an explicit
psyz dir the counts are printed as "control" and only 0 unexplained is asserted.
--recordings: the private input check of tests/private_inputs.py comes first (exit code 1 or 77 as it gives).
Exit code 0: all hold; 77 when the psyz checkout is not there, but 1 when SH2O_REQUIRE_PSYZ is 1. The work dir is
removed at the end.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import gte_model as gm  # noqa: E402
import vectors_gte as vg  # noqa: E402
import private_inputs  # noqa: E402
import gte_sources_check as gsc  # noqa: E402

M32 = 0xFFFFFFFF
PSYZ_SRCS = ["psyz/src/psyz/libgte.c", "psyz/src/psyz/libgte_sw_sqrt.c", "decomp/src/libgte/ratan.c",
             "decomp/src/libgte/sqrtbl.c", "decomp/src/libgte/msc02.c"]
CFLAGS = ["-std=gnu11", "-O2", "-w", "-D__psyz"]
SAMPLE_FILE = ROOT / "tests/ps1-tests/gte_sample.txt"
SAMPLE_SHA256 = "9b2b3c924b17f0c846855526dd8581b3acaf7a1123b193f0d339fd409846faa9"

# Pinned against the default psyz checkout. The 1,600 ps1-tests cases: the differing registers and the count of
# each cause.
PUBLIC = {"cases": 1600, "regs": 15874, "causes": {
    "psyz: write side effect: SXYP write does not push the SXY FIFO": 4464,
    "psyz: write side effect: IRGB write does not set IR1-3": 2460,
    "psyz: read view: IRGB/ORGB not derived from IR1-3 (reads 0)": 2608,
    "psyz: store: OFX/OFY write drops the low 16 bits (fraction)": 2462,
    "psyz: read view: LZCR not computed from LZCS (reads 0)": 1600,
    "psyz: store: LZCS write not stored (reads 0)": 1476,
    "psyz: read view: H read zero-extended (hw CFC2 sign-extends H)": 656,
    "psyz: write side effect: FLAG write not masked to bits 12-30 / bit 31 not recomputed": 82,
    "psyz: command: AVSZ3/AVSZ4 MAC0 32-bit overflow: hw sets FLAG MAC0+/- and limits OTZ from the "
    "unwrapped sum; PsyZ limits OTZ from the wrapped 32-bit MAC0, no MAC0 flag": 22,
    "psyz: command: MVMVA cv=2 (FC) hw bug: IR1-3 saturation flags of the FC partial not modelled": 10,
    "psyz: command: RTPS SY off by 1 (projection Y rounding) [sf=0 lm=0 sat]": 4,
    "psyz: command: RTPT SY off by 1 (projection Y rounding) [sf=0 lm=0 sat]": 4,
    "psyz: command: NCCS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCDS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCDT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+": 2,
    "psyz: command: NCDT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+": 2,
    "psyz: command: NCT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: RTPS/RTPT MAC0- PsyZ-only: PsyZ flags a negative IR*q product below -2^31 though "
    "OFX/OFY + IR*q is in range (probe: not mirrored for a positive product)": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC1+|MAC1-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC1-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC3-": 2
}}
# The sample: its cases, those with a command sh2o lacks (psyz only), and the registers where psyz differs from the
# expected value whose cause equals (or changed from) the cause the sample records. No cause changed.
SAMPLE = {"cases": 230, "psyz only": 70, "equal": 2386, "changed": 0}
# --recordings: the 1,600 ps1-tests cases and the 2,864 clean recorded pairs (none is a DPCT). Before DPCT (1,500
# ps1-tests cases, 4,364 cases) the differing-register count equalled the one the record of the original port gives
# (22,034); the 100 ps1-tests DPCT cases add 868 registers, each cause as in PUBLIC.
RECORDINGS = {"cases": 4464, "regs": 22902, "causes": {
    "psyz: read view: IRGB/ORGB not derived from IR1-3 (reads 0)": 6772,
    "psyz: write side effect: SXYP write does not push the SXY FIFO": 4464,
    "model: PS1 view: Redux LZCR raw storage (set only by an LZCS write) reads 0 with LZCS 0; psx-spx "
    "and ps1-tests: LZCR = leading count of LZCS (32)": 2864,
    "psyz: write side effect: IRGB write does not set IR1-3": 2460,
    "psyz: store: OFX/OFY write drops the low 16 bits (fraction)": 2462,
    "psyz: read view: LZCR not computed from LZCS (reads 0)": 1600,
    "psyz: store: LZCS write not stored (reads 0)": 1476,
    "psyz: read view: H read zero-extended (hw CFC2 sign-extends H)": 656,
    "psyz: write side effect: FLAG write not masked to bits 12-30 / bit 31 not recomputed": 82,
    "psyz: command: AVSZ3/AVSZ4 MAC0 32-bit overflow: hw sets FLAG MAC0+/- and limits OTZ from the "
    "unwrapped sum; PsyZ limits OTZ from the wrapped 32-bit MAC0, no MAC0 flag": 22,
    "psyz: command: MVMVA cv=2 (FC) hw bug: IR1-3 saturation flags of the FC partial not modelled": 10,
    "psyz: command: RTPS SY off by 1 (projection Y rounding) [sf=0 lm=0 sat]": 4,
    "psyz: command: RTPT SY off by 1 (projection Y rounding) [sf=0 lm=0 sat]": 4,
    "psyz: command: NCCS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCDS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCDT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+": 2,
    "psyz: command: NCDT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCS FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: NCT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+": 2,
    "psyz: command: NCT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: RTPS/RTPT MAC0- PsyZ-only: PsyZ flags a negative IR*q product below -2^31 though "
    "OFX/OFY + IR*q is in range (probe: not mirrored for a positive product)": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC1+|MAC1-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC1-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2+|MAC2-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC2-": 2,
    "psyz: command: RTPT FLAG MAC1-3 overflow flag (hw: each 44-bit intermediate sum) hw-only MAC3-": 2
}}
# The differing-register count that the record of the original port gives over the 4,364 cases before DPCT.
ORIGINAL_PORT_REGS = 22034

NAMES = {0x00: "none", 0x01: "RTPS", 0x06: "NCLIP", 0x0C: "OP", 0x10: "DPCS", 0x11: "INTPL", 0x12: "MVMVA",
         0x13: "NCDS", 0x14: "CDP", 0x16: "NCDT", 0x1B: "NCCS", 0x1C: "CC", 0x1E: "NCS", 0x20: "NCT", 0x28: "SQR",
         0x29: "DCPL", 0x2A: "DPCT", 0x2D: "AVSZ3", 0x2E: "AVSZ4", 0x30: "RTPT", 0x3D: "GPF", 0x3E: "GPL",
         0x3F: "NCCT"}
REG = ["VXY0", "VZ0", "VXY1", "VZ1", "VXY2", "VZ2", "RGBC", "OTZ", "IR0", "IR1", "IR2", "IR3", "SXY0", "SXY1",
       "SXY2", "SXYP", "SZ0", "SZ1", "SZ2", "SZ3", "RGB0", "RGB1", "RGB2", "RES1", "MAC0", "MAC1", "MAC2", "MAC3",
       "IRGB", "ORGB", "LZCS", "LZCR", "RT11RT12", "RT13RT21", "RT22RT23", "RT31RT32", "RT33", "TRX", "TRY", "TRZ",
       "L11L12", "L13L21", "L22L23", "L31L32", "L33", "RBK", "GBK", "BBK", "LR1LR2", "LR3LG1", "LG2LG3", "LB1LB2",
       "LB3", "RFC", "GFC", "BFC", "OFX", "OFY", "H", "DQA", "DQB", "ZSF3", "ZSF4", "FLAG"]
FLAG_BITS = {31: "error", 30: "MAC1+", 29: "MAC2+", 28: "MAC3+", 27: "MAC1-", 26: "MAC2-", 25: "MAC3-", 24: "IR1",
             23: "IR2", 22: "IR3", 21: "R", 20: "G", 19: "B", 18: "SZ3/OTZ", 17: "DIV", 16: "MAC0+", 15: "MAC0-",
             14: "SX2", 13: "SY2", 12: "IR0"}
VARIANTS = {"all": ("sxy", "ir", "flag"), "sxy": ("sxy",), "ir": ("ir",), "flag": ("flag",)}
EFFECT = {"sxy": "write side effect: SXYP write does not push the SXY FIFO",
          "ir": "write side effect: IRGB write does not set IR1-3",
          "flag": "write side effect: FLAG write not masked to bits 12-30 / bit 31 not recomputed"}
LZCR_RAW = ("PS1 view: Redux LZCR raw storage (set only by an LZCS write) reads 0 with LZCS 0; psx-spx and ps1-tests: "
            "LZCR = leading count of LZCS (32)")


def opname(cmd):
    return "none" if cmd is None else NAMES.get(cmd & 0x3F, f"0x{cmd & 0x3F:02x}")


def flag_names(v):
    return [n for b, n in sorted(FLAG_BITS.items(), reverse=True) if v >> b & 1]


def hw_state(inp, effects=("sxy", "ir", "flag")):
    """The hardware register state after the 64 writes in[0..63]: SXYP write pushes the SXY FIFO (SXY0..2 = in[13],
    in[14], in[15]); IRGB write (after IR1-3) sets IR1-3 = 5-bit fields << 7; FLAG write keeps bits 12-30, bit 31 =
    OR of 30-23, 18-13."""
    s = list(inp)
    if "sxy" in effects:
        s[12], s[13], s[14] = inp[13], inp[14], inp[15]
    if "ir" in effects:
        s[9], s[10], s[11] = ((inp[28] >> sh & 0x1F) << 7 for sh in (0, 5, 10))
    if "flag" in effects:
        f = inp[63] & 0x7FFFF000
        s[63] = f | (1 << 31 if f & 0x7F87E000 else 0)
    return s


def variant_writes(c, name):
    """The writes of c replayed from the hardware's post-write state with only VARIANTS[name]'s effects."""
    s = hw_state(c["in"], VARIANTS[name])
    return [(r, s[r]) for r in vg.ORDER]


def view_cause(r, exp, got, sxy2=None):
    """Read/store view of one register that writing the hardware state cannot fix (the psyz register file)."""
    if r == 58 and got == exp & 0xFFFF:
        return "read view: H read zero-extended (hw CFC2 sign-extends H)"
    if r in (56, 57) and got == exp & 0xFFFF0000:
        return "store: OFX/OFY write drops the low 16 bits (fraction)"
    if r == 30 and got == 0:
        return "store: LZCS write not stored (reads 0)"
    if r == 31 and got == 0:
        return "read view: LZCR not computed from LZCS (reads 0)"
    if r in (28, 29) and got == 0:
        return "read view: IRGB/ORGB not derived from IR1-3 (reads 0)"
    if r == 15 and sxy2 is not None and got != sxy2:
        return "read view: SXYP is its own register (SXP/SYP), not a mirror of SXY2"
    return None


def coarse_class(c):
    """Grouping class of a command case: sf, lm, (MVMVA mx, cv) and whether hw FLAG shows DIV or any saturation."""
    k, f = c["cmd"], c["out"][63]
    t = [f"sf={k >> 19 & 1}", f"lm={k >> 10 & 1}"]
    if k & 0x3F == 0x12:
        t += [f"mx={k >> 17 & 3}", f"cv={k >> 13 & 3}"]
    t.append("DIV" if f >> 17 & 1 else "sat" if f & 0x7FFFF000 else "noflag")
    return " ".join(t)


def cause(c, r, got, vgot, sxy2=None):
    """Name of one difference (case c, register r, raw driver value got; vgot: variant -> value of r replayed from
    the hw post-write state). Fixed by the full hw state -> the single write effect that fixes it (or 'combined');
    else a read/store view; else the command's own semantics, by opcode, register and coarse input class."""
    exp = c["out"][r]
    if vgot["all"] == exp:
        one = [e for e in ("sxy", "ir", "flag") if vgot[e] == exp]
        return EFFECT[one[0]] if len(one) == 1 else "write side effects combined (SXYP push, IRGB->IR, FLAG mask)"
    v = view_cause(r, exp, vgot["all"], sxy2)
    if v:
        return v
    op, g = c["cmd"] & 0x3F, vgot["all"]
    if op in (0x2D, 0x2E) and r in (7, 63) and (r == 7 or (exp ^ g) & 0x18000):
        return ("command: AVSZ3/AVSZ4 MAC0 32-bit overflow: hw sets FLAG MAC0+/- and limits OTZ from the unwrapped "
                "sum; PsyZ limits OTZ from the wrapped 32-bit MAC0, no MAC0 flag")
    if op == 0x12 and r == 63 and c["cmd"] >> 13 & 3 == 2:
        return "command: MVMVA cv=2 (FC) hw bug: IR1-3 saturation flags of the FC partial not modelled"
    if op in (0x01, 0x30) and r == 63 and g & ~exp & 0x7FFFFFFF == 1 << 15 and not exp & ~g & 0x7FFFFFFF:
        return ("command: RTPS/RTPT MAC0- PsyZ-only: PsyZ flags a negative IR*q product below -2^31 though OFX/OFY + "
                "IR*q is in range (probe: not mirrored for a positive product)")
    if r == 63:
        miss, extra = flag_names(exp & ~g & 0x7FFFFFFF), flag_names(g & ~exp & 0x7FFFFFFF)
        why = " MAC1-3 overflow flag (hw: each 44-bit intermediate sum)" if (exp ^ g) & 0x7E000000 == (exp ^ g) & \
            0x7FFFFFFF else ""
        return (f"command: {opname(c['cmd'])} FLAG{why}" + (f" hw-only {'|'.join(miss)}" if miss else "")
                + (f" PsyZ-only {'|'.join(extra)}" if extra else "") + (" (bit 31 only)" if not miss + extra else ""))
    if op in (0x01, 0x30) and r in (12, 13, 14, 15) and abs((exp >> 16) - (g >> 16)) == 1 and exp & 0xFFFF == g & 0xFFFF:
        return f"command: {opname(c['cmd'])} SY off by 1 (projection Y rounding) [{coarse_class(c)}]"
    return f"command: {opname(c['cmd'])} {REG[r]} [{coarse_class(c)}]"


def _generic(n):
    return bool(re.match(r"command: \S+ \S+ \[", n) or re.match(r"command: \S+ FLAG (hw-only|PsyZ-only|\(bit)", n))


def model_diffs(c):
    """-> (model out | None, {r: cause}) of the model against the case's expected out."""
    o = gm.step(c["model_in"], c["cmd"])
    if o is None:
        return None, {}
    d = {}
    for r in (r for r in range(64) if o[r] != c["out"][r] & M32):
        if c["source"] == "ps1-pairs" and r == 31 and c["out"][31] == c["in"][31] and o[31] == gm.lzcr(c["in"][30]):
            d[r] = LZCR_RAW
        else:
            d[r] = (f"UNNAMED model vs {c['source']}: {opname(c['cmd'])} {REG[r]} "
                    f"[{coarse_class(c) if c['cmd'] is not None else 'none'}]")
    return o, d


def psyz_cause(c, r, got, vgot, sxy2, mo, mc):
    """A psyz difference: write effects / read-store views (cause()); else a source cause when the model disagrees
    with the expected value too; else the command name (UNNAMED when only the generic fallback)."""
    exp = c["out"][r]
    if vgot["all"] == exp or view_cause(r, exp, vgot["all"], sxy2):
        return cause(c, r, got, vgot, sxy2)
    if mo is not None and mo[r] != exp & M32:
        return "source: " + mc[r] + ("" if vgot["all"] == mo[r] else " [PsyZ differs from the model too]")
    n = cause(c, r, got, vgot, sxy2)
    return "UNNAMED " + n if _generic(n) else n


def psyz_named(cases, got, vgot, models):
    """psyz results of cases (vgot: variant -> results of the ps1-tests cases, by index) -> list of {r: cause}."""
    out = []
    for i, c in enumerate(cases):
        mo, mc = models[i]
        g = got[i]
        vg_ = {v: vgot[v][i] for v in VARIANTS} if c["source"] == "ps1-tests" else {v: g for v in VARIANTS}
        out.append({r: psyz_cause(c, r, g[r], {v: vg_[v][r] for v in VARIANTS}, vg_["all"][14], mo, mc)
                    for r in range(64) if g[r] != c["out"][r] & M32})
    return out


def sh2o_explain(c, sh, pz, named, mo, mc):
    """The reading of each register where sh2o (sh) differs from psyz (pz) on case c. named: {r: psyz cause}; mo, mc:
    model_diffs(c). -> {r: cause}; a cause starting UNEXPLAINED is a difference with no name."""
    d = {}
    for r in (r for r in range(64) if sh[r] != pz[r]):
        exp, n = c["out"][r] & M32, named.get(r)
        if n and not n.startswith("UNNAMED") and not _generic(n):
            d[r] = "psyz: " + n
        elif n and mo is not None and sh[r] == mo[r] == exp:
            d[r] = "psyz (sh2o = model = expected): " + n.removeprefix("UNNAMED ")
        elif mo is not None and sh[r] == mo[r] and r in mc and not mc[r].startswith("UNNAMED"):
            d[r] = "model: " + mc[r]
        else:
            d[r] = (f"UNEXPLAINED sh2o {sh[r]:08x} PsyZ {pz[r]:08x} expected {exp:08x}"
                    + (f" model {mo[r]:08x}" if mo is not None else "") + (f"; PsyZ cause {n}" if n else ""))
    return d


def sh2o_summary(cases, sh, pz, named, models):
    """-> (differing registers, Counter of causes (UNEXPLAINED as one key), first unexplained texts)."""
    causes, ex = Counter(), []
    for c, a, b, n, (mo, mc) in zip(cases, sh, pz, named, models):
        for r, why in sorted(sh2o_explain(c, a, b, n, mo, mc).items()):
            key = "UNEXPLAINED" if why.startswith("UNEXPLAINED") else why
            causes[key] += 1
            if key == "UNEXPLAINED" and len(ex) < 5:
                ex.append(f"{c['id']} {REG[r]}: {why}")
    return sum(causes.values()), causes, ex


def line(cmd, writes):
    return (f"{'-' if cmd is None else format(cmd, 'x')} {len(writes)} "
            + " ".join(f"{r} {v & M32:x}" for r, v in writes) + "\n")


def run(argv, stdin, n):
    r = subprocess.run(argv, input=stdin, capture_output=True, text=True)
    if r.returncode != 0:
        fail(f"{argv[0]}: exit code {r.returncode}: {r.stderr[-500:]}")
    out = [[int(w, 16) for w in ln.split() if w != "x"] for ln in r.stdout.splitlines()]
    if len(out) != n or any(len(w) != 64 for w in out):
        fail(f"{argv[0]}: {len(out)} lines for {n} cases")
    return out


def build(cc, work, psyz):
    objs = []
    for i, s in enumerate(PSYZ_SRCS):
        objs.append(str(work / f"{i}.o"))
        compile_([cc, *CFLAGS, "-I", str(psyz / "psyz/include"), "-c", str(psyz / s), "-o", objs[-1]])
    objs.append(str(work / "driver.o"))
    compile_([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-c",
              str(ROOT / "tests/psyz/psyz_gte_driver.c"), "-o", objs[-1]])
    exe = str(work / "psyz_gte")
    compile_([cc, "-o", exe, *objs, "-lm"])
    return exe


def compile_(argv):
    r = subprocess.run(argv, capture_output=True, text=True)
    if r.returncode != 0:
        fail(f"{' '.join(argv)}:\n{r.stdout}{r.stderr}")


def fail(text):
    print(f"psyz_gte: FAIL: {text}")
    sys.exit(1)


def run_psyz(exe, cases):
    """cases, then each variant over the ps1-tests cases, in one psyz process -> (results, {variant: results})."""
    vt = [c for c in cases if c["source"] == "ps1-tests"]
    stdin = "".join(line(c["cmd"], c["writes"]) for c in cases)
    stdin += "".join(line(c["cmd"], variant_writes(c, v)) for v in VARIANTS for c in vt)
    res = run([exe], stdin, len(cases) + len(VARIANTS) * len(vt))
    n, k = len(cases), len(vt)
    vres = {v: res[n + k * i:n + k * (i + 1)] for i, v in enumerate(VARIANTS)}
    return res[:n], vres


def with_in(c):
    inp = [0] * 64
    for r, v in c["writes"]:
        inp[r] = v & M32
    return {**c, "in": inp if c["source"] == "ps1-tests" else c["model_in"]}


def sample_cases():
    data = SAMPLE_FILE.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != SAMPLE_SHA256:
        fail(f"{SAMPLE_FILE.relative_to(ROOT)}: sha256 {digest}, pinned {SAMPLE_SHA256}")
    names, out = {}, []
    for ln in data.decode("ascii").splitlines():
        if ln.startswith("# cause "):
            k, text = ln[8:].split(" ", 1)
            names[int(k)] = text
        if ln.startswith("#"):
            continue
        f = ln.split()
        inp = [int(w, 16) for w in f[3:67]]
        kd = {} if f[131] == "-" else {e.split("=")[0]: names[int(e.split("=")[1])] for e in f[131].split(",")}
        out.append({"source": "ps1-tests", "src": f[0], "id": f"{f[0]} {f[1]}",
                    "cmd": None if f[2] == "-" else int(f[2], 16), "in": inp, "out": [int(w, 16) for w in f[67:131]],
                    "writes": [(e[0], 0) if isinstance(e, tuple) else (e, inp[e])
                               for e in gsc.PS1_TESTS_ORDER[f[0]]], "known": kd, "key": " ".join(f[0:1] + f[2:131])})
    return out


def check_sample(exe, control):
    cases = sample_cases()
    lines = {" ".join(ln.split()[:130]) for ln in gsc.CASE_FILE.read_text(encoding="ascii").splitlines()
             if not ln.startswith("#")}
    lacks = [c for c in cases if c["cmd"] is not None and c["cmd"] & 0x3F not in gm.MODELLED_ALL]
    missing = [c["id"] for c in cases if c not in lacks and c["key"] not in lines]
    if missing:
        fail(f"sample cases not in {gsc.CASE_FILE.name}: {missing[:5]}")
    got, vres = run_psyz(exe, cases)
    equal, changes = 0, []
    for i, c in enumerate(cases):
        now = {REG[r]: cause(c, r, got[i][r], {v: vres[v][i][r] for v in VARIANTS}, vres["all"][i][14])
               for r in range(64) if got[i][r] != c["out"][r]}
        for reg in sorted(set(now) | set(c["known"]), key=REG.index):
            if now.get(reg) == c["known"].get(reg):
                equal += 1
            else:
                changes.append(f"{c['id']} {reg}: recorded {c['known'].get(reg)!r}, now {now.get(reg)!r}")
    figures = {"cases": len(cases), "psyz only": len(lacks), "equal": equal, "changed": len(changes)}
    print(f"psyz_gte: sample{' (control)' if control else ''}: {figures}")
    for ch in changes:
        print(f"  change {ch}")
    if not control and figures != SAMPLE:
        fail(f"sample figures {figures}, pinned {SAMPLE}")


def compare(exe, gte, cases, control, label, pins):
    t0 = time.monotonic()
    got, vres = run_psyz(exe, cases)
    sh = run([gte, "--driver"], "".join(line(c["cmd"], c["writes"]) for c in cases), len(cases))
    models = [model_diffs(c) for c in cases]
    named = psyz_named(cases, got, {v: vres[v] for v in VARIANTS}, models)
    regs, causes, ex = sh2o_summary(cases, sh, got, named, models)
    figures = {"cases": len(cases), "regs": regs, "causes": dict(sorted(causes.items()))}
    print(f"psyz_gte: {label}{' (control)' if control else ''}: {len(cases)} cases, {regs} differing registers, "
          f"{causes['UNEXPLAINED']} unexplained, {time.monotonic() - t0:.1f} s")
    for k, v in sorted(causes.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"  {v:6} {k}")
    if causes["UNEXPLAINED"]:
        fail(f"{label}: unexplained differences, first {ex}")
    if not control and figures != pins:
        fail(f"{label}: figures differ from the pins")
    # The plant: the first command case and register where sh2o, psyz, the model and the expected value agree.
    for i, c in enumerate(cases):
        mo = models[i][0]
        if c["cmd"] is None or mo is None:
            continue
        r = next((r for r in range(64) if sh[i][r] == got[i][r] == mo[r] == c["out"][r]), None)
        if r is not None:
            break
    planted = [list(w) for w in sh]
    planted[i][r] ^= 1
    _, pc, _ = sh2o_summary(cases, planted, got, named, models)
    print(f"psyz_gte: plant {c['id']} {REG[r]} bit 0: {pc['UNEXPLAINED']} unexplained")
    if pc["UNEXPLAINED"] != 1:
        fail(f"plant: {pc['UNEXPLAINED']} unexplained, want 1")
    return figures


def main(argv):
    rec = len(argv) > 1 and argv[1] == "--recordings"
    args = argv[2:] if rec else argv[1:]
    if len(args) not in (4, 5):
        sys.exit(__doc__)
    cc, source, work, gte = args[0], Path(args[1]), Path(args[2]), args[3]
    psyz = Path(args[4]) if len(args) == 5 else source / "external/psyz"
    control = len(args) == 5 and psyz.resolve() != (source / "external/psyz").resolve()
    t0 = time.monotonic()
    if rec:
        status = private_inputs.check(gsc.VARIABLE, str(source))
        if status:
            return status
    libgte = psyz / PSYZ_SRCS[0]
    if not libgte.exists():
        if os.environ.get("SH2O_REQUIRE_PSYZ") == "1":
            print(f"psyz_gte: required (SH2O_REQUIRE_PSYZ=1), {libgte} is not checked out")
            return 1
        print(f"psyz_gte: skipped, {libgte} is not checked out")
        return 77
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    try:
        exe = build(cc, work, psyz)
        cases = [with_in(c) for c in gsc.ps1_tests_cases()]
        if rec:
            pairs, digest = gsc.recordings(Path(os.environ[gsc.VARIABLE]))
            if digest != gsc.RECORDINGS_DIGEST or len(pairs) != gsc.CLEAN_PAIRS:
                fail(f"recordings: {len(pairs)} clean pairs, identity digest {digest}")
            cases += [with_in(c) for c in pairs]
            figures = compare(exe, gte, cases, control, "recordings", RECORDINGS)
            print(f"psyz_gte: recordings: {figures['regs']} differing registers; the record of the original port "
                  f"gives {ORIGINAL_PORT_REGS:,}")
        else:
            compare(exe, gte, cases, control, "ps1-tests", PUBLIC)
            check_sample(exe, control)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f"psyz_gte: passed{' (control)' if control else ''}, {time.monotonic() - t0:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
