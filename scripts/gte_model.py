#!/usr/bin/env python3
"""Python model of the GTE commands and of the register read view.

Copyright (c) 2026 sh2o Contributors. MIT License.

New code from the formulas of psx-spx (psx-spx.consoledev.net/geometrytransformationenginegte/, nocash; read
2026-11-17). It cites psx-spx and does not quote it at length. RTPS/RTPT divide: the nocash UNR divide; the script
computes its 257-entry table from the psx-spx formula (it copies no table). The model runs on Python 3.7 or later.

step(regs, cmd) -> 64 registers: regs = a hardware state as MFC2/CFC2 read it (r 0-31 data, 32-63 control), cmd the
command word (low 25 bits; fields sf 19, mx 17-18, v 15-16, cv 13-14, lm 10, op 0-5). Output in the same read view.
None for an opcode that the model does not have (MODELLED_ALL: the 14 opcodes RTPS RTPT NCLIP DPCS DPCT MVMVA NCDS NCDT
NCS NCT SQR AVSZ3 GPF GPL, MODELLED, plus NCCS); cmd None -> the read view unchanged. read_view(storage) -> the
MFC2/CFC2 view of a storage state after register writes.

psx-spx statements used: FLAG reset at each command start; bit 31 = OR of bits 30-23, 18-13, and set with the divide
overflow (bit 17, "FLAG.Bit17=1, FLAG.Bit31=1"); MAC1-3 flags = "larger than 43 bits" (bits 30-25), MAC0 = "larger
than 31 bits" (16/15); IR1-3 saturated per lm (bits 24-22), IR0 0..1000h (12), SZ3/OTZ 0..FFFFh (18), SX2/SY2
-400h..3FFh (14/13), colour FIFO MAC/16 to 0..FFh (21-19); interpolation intermediate IR "as if lm=0"; RTPS/RTPT IR3
flag always as if lm=0 and with sf=0 from MAC3 SAR 12 (IR3 itself from MAC3, lm range); SZ3 = MAC3 SAR ((1-sf)*12);
H < SZ3*2 else 1FFFFh + flag 17; mx=3 matrix [-R*10h, R*10h, IR0; RT13 x3; RT22 x3]; cv=2 (FC) "reduced to the last
two portions" with flags "as if the full operation would have been executed"; SQR lm has no effect; NCCS = NCS's
light step, then [MAC1-3] = [R*IR1, G*IR2, B*IR3] SHL 4, SAR sf, IR1-3 (lm), colour FIFO; DPCT "executes thrice,
and reads the R,G,B values from RGB0" (CODE from RGBC as usual): DPCS's step on RGB0, three times, each step reading
RGB0 after the previous step's colour FIFO push.
Where psx-spx is silent (the reading of this model, named so a difference can be attributed):
- MAC1-3 overflow flags are checked on every partial sum, each partial truncated to 44 bits (MAC44).
- IR1-3 / colour FIFO are taken from the 32-bit MAC registers; SX2/SY2/IR0 from the unwrapped projection sum
  (psx-spx: "SX2=MAC0/10000h" with the sum flagged, not wrapped).
- AVSZ3 OTZ from the unwrapped sum SAR 12 (MAC0 holds it wrapped). A first reading (OTZ from the wrapped MAC0) gave 5
  OTZ differences in the 50 AVSZ3 cases of the ps1-tests gte-fuzz set (Test 811, 818, 821, 833, 839: the hardware OTZ
  follows the sign of the unwrapped sum); psx-spx is silent, so the model takes the reading of the hardware.
- The cv=2 partial (Tx*1000h + Mx11*Vx) is flagged with IR saturation as if lm=0, MAC flags as usual.
- MAC0 flags on the unwrapped sum (NCLIP, AVSZ3, RTPS/RTPT projection and depth sums)."""
M32 = 0xFFFFFFFF
B31_MASK = 0x7F87E000                   # FLAG bits 30-23, 18-13 (psx-spx: ORed into bit 31)
SEXT_D = (1, 3, 5, 8, 9, 10, 11)        # data regs read sign-extended 16 (VZ0-2, IR0-3)
ZEXT_D = (7, 16, 17, 18, 19)            # OTZ, SZ0-3: unsigned 16
SEXT_C = (36, 44, 52, 58, 59, 61, 62)   # RT33, L33, LB3, H (hw bug), DQA, ZSF3, ZSF4
MODELLED = {0x01: "RTPS", 0x30: "RTPT", 0x06: "NCLIP", 0x10: "DPCS", 0x12: "MVMVA", 0x13: "NCDS", 0x16: "NCDT",
            0x1E: "NCS", 0x20: "NCT", 0x28: "SQR", 0x2A: "DPCT", 0x2D: "AVSZ3", 0x3D: "GPF", 0x3E: "GPL"}
MODELLED_ALL = {**MODELLED, 0x1B: "NCCS"}   # step() models these 15
# UNR table (psx-spx, GTE division): unr[i] = max(0, (40000h / (i + 100h) + 1) / 2 - 101h), i 0..100h
UNR = bytes(max(0, (0x40000 // (i + 0x100) + 1) // 2 - 0x101) for i in range(0x101))


def gte_divide(h, sz):
    """RTPS/RTPT divide for h < 2 sz (the nocash UNR divide): about ((h << 17) / sz + 1) / 2, at most 1FFFFh."""
    z = 16 - sz.bit_length()
    n, d = h << z, sz << z
    u = UNR[(d - 0x7FC0) >> 7] + 0x101
    d = (0x2000080 - d * u) >> 8
    d = (0x80 + d * u) >> 8
    return min(0x1FFFF, (n * d + 0x8000) >> 16)


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def s32(v):
    v &= M32
    return v - 0x100000000 if v & 0x80000000 else v


def lzcr(v):
    """LZCR: leading 0 count of LZCS if positive, leading 1 count if negative (1..32)."""
    v &= M32
    if v & 0x80000000:
        v ^= M32
    return 32 - v.bit_length()


def orgb(ir1, ir2, ir3):
    """IRGB/ORGB read: IRn / 80h saturated to 0..1Fh, packed 5:5:5."""
    return sum(max(0, min(0x1F, s16(x) >> 7)) << (5 * i) for i, x in enumerate((ir1, ir2, ir3)))


def read_view(s):
    """MFC2/CFC2 view of a register storage state (64 words): psx-spx read rules (sign/zero extension, SXYP mirror,
    IRGB/ORGB from IR1-3, LZCR from LZCS, FLAG bit 31)."""
    o = [w & M32 for w in s]
    for r in SEXT_D + SEXT_C:
        o[r] = s16(o[r]) & M32
    for r in ZEXT_D:
        o[r] &= 0xFFFF
    o[15] = o[14]
    o[28] = o[29] = orgb(o[9], o[10], o[11])
    o[31] = lzcr(o[30])
    f = o[63] & 0x7FFFF000
    o[63] = f | (1 << 31 if f & B31_MASK else 0)
    return o


class _G:
    """One command's working state: registers decoded, FLAG accumulated."""

    def __init__(self, regs, cmd):
        self.r = [w & M32 for w in regs]
        self.sf = 12 if cmd >> 19 & 1 else 0
        self.lm = cmd >> 10 & 1
        self.flag = 0
        self.mac = [s32(self.r[24 + i]) for i in range(4)]       # MAC0-3
        self.ir = [s16(self.r[8 + i]) for i in range(4)]         # IR0-3

    # --- register fields
    def vec(self, n):
        return [s16(self.r[2 * n]), s16(self.r[2 * n] >> 16), s16(self.r[2 * n + 1])]

    def mat(self, base):
        w = self.r[base:base + 5]
        h = [s16(w[0]), s16(w[0] >> 16), s16(w[1]), s16(w[1] >> 16), s16(w[2]), s16(w[2] >> 16),
             s16(w[3]), s16(w[3] >> 16), s16(w[4])]
        return [h[0:3], h[3:6], h[6:9]]

    def cvec(self, base):
        return [s32(self.r[base + i]) for i in range(3)]

    def rgbc(self):
        c = self.r[6]
        return [c & 0xFF, c >> 8 & 0xFF, c >> 16 & 0xFF], c >> 24 & 0xFF

    # --- flagged arithmetic
    def mac44(self, i, v):
        """MAC1-3 partial (i 0-2): flags 30-i / 27-i when beyond 43 bits; truncated to 44 bits."""
        if v >= 1 << 43:
            self.flag |= 1 << (30 - i)
        elif v < -(1 << 43):
            self.flag |= 1 << (27 - i)
        v &= (1 << 44) - 1
        return v - (1 << 44) if v & 1 << 43 else v

    def mac0(self, v):
        if v > 0x7FFFFFFF:
            self.flag |= 1 << 16
        elif v < -0x80000000:
            self.flag |= 1 << 15
        return v

    def sat_ir(self, i, v, lm):
        """IR1-3 (i 0-2) from v: -8000h (0 with lm)..7FFFh, flag 24-i."""
        lo = 0 if lm else -0x8000
        if v < lo or v > 0x7FFF:
            self.flag |= 1 << (24 - i)
        return max(lo, min(0x7FFF, v))

    def set_mac_ir(self, vals, lm):
        """MAC1-3 = 44-bit sums SAR sf (32-bit registers); IR1-3 saturated from them."""
        for i, v in enumerate(vals):
            self.mac[1 + i] = s32(v >> self.sf)
            self.ir[1 + i] = self.sat_ir(i, self.mac[1 + i], lm)

    def push_rgb(self):
        """Colour FIFO: RGB0 <- RGB1 <- RGB2 <- [MAC1/16, MAC2/16, MAC3/16, CODE] saturated 0..FFh, flags 21-19."""
        out = 0
        for i in range(3):
            v = self.mac[1 + i] >> 4
            if v < 0 or v > 0xFF:
                self.flag |= 1 << (21 - i)
            out |= max(0, min(0xFF, v)) << (8 * i)
        self.r[20], self.r[21], self.r[22] = self.r[21], self.r[22], out | (self.r[6] & 0xFF000000)

    def push_sz(self, v):
        if v < 0 or v > 0xFFFF:
            self.flag |= 1 << 18
        self.r[16], self.r[17], self.r[18], self.r[19] = self.r[17], self.r[18], self.r[19], max(0, min(0xFFFF, v))

    # --- shared pipelines
    def mat_vec(self, m, v, t, lm):
        """[MAC1-3] = (T*1000h + M*V) SAR sf, flagged per partial; IR1-3 = MAC1-3 (lm)."""
        self.set_mac_ir([self.mac44(i, self.mac44(i, self.mac44(i, (t[i] << 12) + m[i][0] * v[0]) + m[i][1] * v[1])
                                    + m[i][2] * v[2]) for i in range(3)], lm)

    def interp_fc(self, base):
        """MAC+(FC-MAC)*IR0 on base = [MAC1-3] before the shift: IR = ((FC SHL 12) - MAC) SAR sf (lm=0);
        MAC = (IR * IR0 + MAC) SAR sf; IR (lm); colour FIFO."""
        fc = self.cvec(53)
        self.set_mac_ir([self.mac44(i, (fc[i] << 12) - base[i]) for i in range(3)], 0)
        self.set_mac_ir([self.mac44(i, self.ir[1 + i] * self.ir[0] + base[i]) for i in range(3)], self.lm)
        self.push_rgb()

    def light(self, n):
        """(light matrix L * Vn) SAR sf -> IR; (BK*1000h + LCM*IR) SAR sf -> IR."""
        self.mat_vec(self.mat(40), self.vec(n), [0, 0, 0], self.lm)
        self.mat_vec(self.mat(48), self.ir[1:4], self.cvec(45), self.lm)

    def rtp(self, n, last):
        """RTPS/RTPT vertex n: MAC1-3/IR1-3, SZ3, SX2/SY2; IR0/MAC0 depth cue when last."""
        m, v, t = self.mat(32), self.vec(n), self.cvec(37)
        full = [self.mac44(i, self.mac44(i, self.mac44(i, (t[i] << 12) + m[i][0] * v[0]) + m[i][1] * v[1])
                           + m[i][2] * v[2]) for i in range(3)]
        for i in range(3):
            self.mac[1 + i] = s32(full[i] >> self.sf)
            if i < 2:
                self.ir[1 + i] = self.sat_ir(i, self.mac[1 + i], self.lm)
            else:                                   # flag as if lm=0, from MAC3 SAR 12 regardless of sf
                z = full[2] >> 12
                if z < -0x8000 or z > 0x7FFF:
                    self.flag |= 1 << 22
                self.ir[3] = max(0 if self.lm else -0x8000, min(0x7FFF, self.mac[3]))
        self.push_sz(full[2] >> 12)                 # SZ3 = MAC3 SAR ((1-sf)*12)
        h, sz = self.r[58] & 0xFFFF, self.r[19]     # H unsigned in the calculation
        if h < sz * 2:
            q = gte_divide(h, sz)
        else:
            q = 0x1FFFF
            self.flag |= 1 << 17 | 1 << 31
        sxy = []
        for i, off, bit in ((1, s32(self.r[56]), 14), (2, s32(self.r[57]), 13)):
            p = self.mac0(q * self.ir[i] + off) >> 16
            if p < -0x400 or p > 0x3FF:
                self.flag |= 1 << bit
            sxy.append(max(-0x400, min(0x3FF, p)))
            self.mac[0] = s32(q * self.ir[i] + off)
        self.r[12], self.r[13], self.r[14] = self.r[13], self.r[14], (sxy[0] & 0xFFFF) | (sxy[1] & 0xFFFF) << 16
        if last:
            d = self.mac0(q * s16(self.r[59]) + s32(self.r[60]))
            self.mac[0] = s32(d)
            ir0 = d >> 12
            if ir0 < 0 or ir0 > 0x1000:
                self.flag |= 1 << 12
            self.ir[0] = max(0, min(0x1000, ir0))

    # --- output
    def out(self):
        o = list(self.r)
        for i in range(4):
            o[24 + i] = self.mac[i] & M32
            o[8 + i] = self.ir[i] & M32
        o = read_view(o)
        o[63] = self.flag | (1 << 31 if self.flag & B31_MASK else 0)   # bit 31 also set with DIV (rtp)
        return o


def step(regs, cmd):
    """One GTE command on a 64-register read-view state -> 64 registers (read view); None if not modelled."""
    if cmd is None:
        return read_view(regs)
    op = cmd & 0x3F
    if op not in MODELLED_ALL:
        return None
    g = _G(regs, cmd)
    if op == 0x01:                                  # RTPS
        g.rtp(0, True)
    elif op == 0x30:                                # RTPT
        for n in range(3):
            g.rtp(n, n == 2)
    elif op == 0x06:                                # NCLIP
        sx = [s16(g.r[12 + i]) for i in range(3)]
        sy = [s16(g.r[12 + i] >> 16) for i in range(3)]
        g.mac[0] = s32(g.mac0(sx[0] * sy[1] + sx[1] * sy[2] + sx[2] * sy[0] - sx[0] * sy[2] - sx[1] * sy[0]
                              - sx[2] * sy[1]))
    elif op == 0x2D:                                # AVSZ3: MAC0 = ZSF3*(SZ1+SZ2+SZ3); OTZ = MAC0/1000h, 0..FFFFh
        a = g.mac0(s16(g.r[61]) * (g.r[17] + g.r[18] + g.r[19]))
        g.mac[0] = s32(a)
        z = a >> 12                                 # unwrapped sum (see docstring: AVSZ3 OTZ)
        if z < 0 or z > 0xFFFF:
            g.flag |= 1 << 18
        g.r[7] = max(0, min(0xFFFF, z))
    elif op == 0x28:                                # SQR: IRn*IRn SAR sf, saturated (lm no effect: positive)
        g.set_mac_ir([g.mac44(i, g.ir[1 + i] * g.ir[1 + i]) for i in range(3)], g.lm)
    elif op == 0x3D:                                # GPF: (IR * IR0 + 0) SAR sf
        g.set_mac_ir([g.mac44(i, g.ir[1 + i] * g.ir[0]) for i in range(3)], g.lm)
        g.push_rgb()
    elif op == 0x3E:                                # GPL: (IR * IR0 + MAC SHL sf) SAR sf
        g.set_mac_ir([g.mac44(i, g.ir[1 + i] * g.ir[0] + (g.mac[1 + i] << g.sf)) for i in range(3)], g.lm)
        g.push_rgb()
    elif op == 0x10:                                # DPCS: [R,G,B] SHL 16, MAC+(FC-MAC)*IR0
        rgb, _ = g.rgbc()
        g.interp_fc([c << 16 for c in rgb])
    elif op == 0x2A:                                # DPCT: DPCS on RGB0 (bottom of the colour FIFO), three times
        for _ in range(3):
            g.interp_fc([(g.r[20] >> 8 * i & 0xFF) << 16 for i in range(3)])
    elif op in (0x13, 0x16):                        # NCDS / NCDT: light, [R*IR1..] SHL 4, MAC+(FC-MAC)*IR0
        for n in ((0,) if op == 0x13 else (0, 1, 2)):
            g.light(n)
            rgb, _ = g.rgbc()
            g.interp_fc([(rgb[i] * g.ir[1 + i]) << 4 for i in range(3)])
    elif op == 0x1B:                                # NCCS: light, [R*IR1..] SHL 4 SAR sf, colour FIFO
        g.light(0)
        rgb, _ = g.rgbc()
        g.set_mac_ir([g.mac44(i, (rgb[i] * g.ir[1 + i]) << 4) for i in range(3)], g.lm)
        g.push_rgb()
    elif op in (0x1E, 0x20):                        # NCS / NCT: light, colour FIFO from MAC
        for n in ((0,) if op == 0x1E else (0, 1, 2)):
            g.light(n)
            g.push_rgb()
    elif op == 0x12:                                # MVMVA
        mx, vx, cv = cmd >> 17 & 3, cmd >> 15 & 3, cmd >> 13 & 3
        if mx == 3:
            rgb, _ = g.rgbc()
            rt = g.mat(32)
            m = [[-(rgb[0] << 4), rgb[0] << 4, g.ir[0]], [rt[0][2]] * 3, [rt[1][1]] * 3]
        else:
            m = g.mat((32, 40, 48)[mx])
        v = g.ir[1:4] if vx == 3 else g.vec(vx)
        if cv == 2:                                 # FC bug: partial flagged, result = last two portions
            fc = g.cvec(53)
            for i in range(3):
                g.sat_ir(i, s32(g.mac44(i, (fc[i] << 12) + m[i][0] * v[0]) >> g.sf), 0)
            g.set_mac_ir([g.mac44(i, g.mac44(i, m[i][1] * v[1]) + m[i][2] * v[2]) for i in range(3)], g.lm)
        else:
            g.mat_vec(m, v, g.cvec(37) if cv == 0 else g.cvec(45) if cv == 1 else [0, 0, 0], g.lm)
    return g.out()
