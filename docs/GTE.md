# GTE register image and commands

This guide tells how sh2o gives the results of the PS1 GTE registers and commands.

**Status: ported.** `include/sh2o/gte.h` has the register image and 15 commands. On each test case, each command gives the bytes of the Python model `scripts/gte_model.py` on all 64 registers, FLAG included. The model uses the formulas of psx-spx, "Geometry Transformation Engine (GTE)" (https://psx-spx.consoledev.net/geometrytransformationenginegte/). The C code does not use the model.

The commands are C. The divide of RTPS and RTPT is the k kernel `sh2o_k_unr_div`. On the SH-2, the MAC stage of MVMVA, RTPS, RTPT and the light commands goes to the k kernel `sh2o_k_mvb3` (see [KERNELS.md](KERNELS.md)).

## The register image

`sh2o_gte_t` holds `r[64]`, in register index order:

- `r[0]` to `r[31]`: the data registers (MTC2 and MFC2).
- `r[32]` to `r[63]`: the control registers (CTC2 and CFC2).

The image keeps the written words. A 16-bit field is the half of a word that a shift reads, so the image does not depend on the byte order. A zeroed image is a valid state. A command keeps its results in the image.

`sh2o_gte_read(g, reg)` gives the MFC2 or CFC2 value (contract tag `gte-regs-1`):

| Registers | Read |
|---|---|
| VZ0-2, IR0-3, RT33, L33, LB3, H, DQA, ZSF3, ZSF4 | The low 16 bits, sign-extended. |
| OTZ, SZ0-3 | The low 16 bits, zero-extended. |
| 15 (SXYP) | The value of SXY2. |
| 28 (IRGB), 29 (ORGB) | IR1-3 SAR 7, each saturated to 0..1Fh, packed 5:5:5. |
| 31 (LZCR) | The count of leading 0 bits of LZCS (LZCS >= 0) or of leading 1 bits (LZCS < 0). |
| 63 (FLAG) | Bits 12 to 30 as stored. Bit 31 is the OR of bits 30 to 23 and 18 to 13. |
| All other registers | The full 32-bit word. |
| reg > 63 | 0. |

`sh2o_gte_write(g, reg, v)` does an MTC2 or CTC2 (contract tag `gte-regs-1`):

| Registers | Write |
|---|---|
| 15 (SXYP) | Pushes the screen FIFO: SXY0 = SXY1, SXY1 = SXY2, SXY2 = v. |
| 28 (IRGB) | Stores v and sets IR1-3 to its three 5-bit fields, each shifted left by 7. |
| 29 (ORGB), 31 (LZCR) | No effect. They are read-only. |
| 63 (FLAG) | Keeps bits 12 to 30 of v. |
| All other registers | Stores v. |
| reg > 63 | No effect. |

## The command word

`sh2o_gte_command(g, cmd)` does one command (contract tag `gte-command-1`). `cmd` is the low 25 bits of the COP2 word. The opcode is bits 0 to 5. The commands read these fields:

| Field | Bits | Meaning |
|---|---|---|
| sf | 19 | 1: the MAC1-3 results are shifted right by 12. |
| mx | 17-18 | MVMVA matrix: 0 RT, 1 light matrix, 2 LCM, 3 the hardware-bug matrix. |
| v | 15-16 | MVMVA vector: 0 V0, 1 V1, 2 V2, 3 IR1-3. |
| cv | 13-14 | MVMVA translation: 0 TR, 1 BK, 2 FC (the hardware-bug path), 3 none. |
| lm | 10 | 1: the lower limit of IR1-3 is 0, not -8000h. |

A command in the table sets FLAG to 0 first, then adds each FLAG bit that its steps set. It returns 0. For an opcode that is not in the table (for example `0Ch`), the function returns -1 and does not change the image, FLAG included. The direct functions (`sh2o_gte_mvmva` and the others) do not check the opcode field. They read sf, lm, mx, v and cv from `cmd`.

## The commands

| Command | Opcode | Contract tag | Fields | FLAG bits it can set |
|---|---|---|---|---|
| RTPS | `01h` | `gte-rtps-1` | sf, lm | 30-25, 24-22, 18, 17, 16-15, 14-13, 12 |
| NCLIP | `06h` | `gte-nclip-1` | none | 16-15 |
| DPCS | `10h` | `gte-dpcs-1` | sf, lm | 30-25, 24-22, 21-19 |
| DPCT | `2Ah` | `gte-dpct-1` | sf, lm | 30-25, 24-22, 21-19 |
| MVMVA | `12h` | `gte-mvmva-1` | sf, lm, mx, v, cv | 30-25, 24-22 |
| NCDS | `13h` | `gte-ncds-1` | sf, lm | 30-25, 24-22, 21-19 |
| NCDT | `16h` | `gte-ncdt-1` | sf, lm | 30-25, 24-22, 21-19 |
| NCCS | `1Bh` | `gte-nccs-1` | sf, lm | 30-25, 24-22, 21-19 |
| NCS | `1Eh` | `gte-ncs-1` | sf, lm | 30-25, 24-22, 21-19 |
| NCT | `20h` | `gte-nct-1` | sf, lm | 30-25, 24-22, 21-19 |
| SQR | `28h` | `gte-sqr-1` | sf, lm | 24-22 |
| AVSZ3 | `2Dh` | `gte-avsz3-1` | none | 18, 16-15 |
| RTPT | `30h` | `gte-rtpt-1` | sf, lm | 30-25, 24-22, 18, 17, 16-15, 14-13, 12 |
| GPF | `3Dh` | `gte-gpf-1` | sf, lm | 24-22, 21-19 |
| GPL | `3Eh` | `gte-gpl-1` | sf, lm | 30-25, 24-22, 21-19 |

Bit 31 of the FLAG read is the OR of bits 30 to 23 and 18 to 13 for each command.

What each command calculates:

- **RTPS:** the perspective transform of V0: (TR * 1000h + RT * V0) SAR (sf * 12) to MAC1-3 and IR1-3. SZ3 is pushed. The divide gives SX2 and SY2, which are pushed. Then the depth cue gives MAC0 and IR0.
- **RTPT:** RTPS on V0, V1 and V2. Only V2 gets the depth cue.
- **NCLIP:** MAC0 = SX0 * SY1 + SX1 * SY2 + SX2 * SY0 - SX0 * SY2 - SX1 * SY0 - SX2 * SY1.
- **AVSZ3:** MAC0 = ZSF3 * (SZ1 + SZ2 + SZ3). OTZ = MAC0 SAR 12, from the sum before it wraps.
- **SQR:** MAC1-3 = (IRn * IRn) SAR (sf * 12).
- **GPF:** MAC1-3 = (IRn * IR0) SAR (sf * 12), then the colour FIFO.
- **GPL:** MAC1-3 = (MACn SHL (sf * 12) + IRn * IR0) SAR (sf * 12), then the colour FIFO.
- **DPCS:** the colour of RGBC SHL 16, moved to the far colour by IR0: MAC + (FC - MAC) * IR0, then the colour FIFO.
- **DPCT:** DPCS three times on RGB0 in place of RGBC (see [DPCT](#dpct)).
- **MVMVA:** MAC1-3 = (T * 1000h + M * V) SAR (sf * 12), with M, V and T from mx, v and cv.
- **NCS:** the light of V0: (light matrix * V0) SAR (sf * 12) to IR1-3, then (BK * 1000h + LCM * IR) SAR (sf * 12) to IR1-3, then the colour FIFO.
- **NCT:** NCS on V0, V1 and V2.
- **NCDS:** the light of V0, then (RGB * IR) SHL 4, moved to the far colour by IR0 as in DPCS.
- **NCDT:** NCDS on V0, V1 and V2.
- **NCCS:** the light of V0, then MAC1-3 = ((RGB * IR) SHL 4) SAR (sf * 12), then the colour FIFO.

## Limits and saturation

| Value | Limit | FLAG bit |
|---|---|---|
| Each partial sum of MAC1-3 | Above 2^43 - 1 or below -2^43: the bit is set, and the sum is cut to 44 bits. | 30-28 (above), 27-25 (below) |
| MAC1-3 | The 44-bit sum SAR (sf * 12), the low 32 bits. | none |
| IR1-3 | -8000h (0 when lm = 1) to 7FFFh. | 24-22 |
| Colour FIFO channels | MAC1-3 SAR 4, saturated to 0..FFh. The CODE byte of RGBC is kept. | 21-19 |
| SZ3 and OTZ | 0 to FFFFh. | 18 |
| Divide result | H >= 2 * SZ3 gives 1FFFFh. | 17 (and 31) |
| MAC0 | Above 7FFFFFFFh or below -80000000h. MAC0 is not saturated. | 16 (above), 15 (below) |
| SX2, SY2 | -400h to 3FFh. | 14 (SX2), 13 (SY2) |
| IR0 | 0 to 1000h. | 12 |

The divide of RTPS and RTPT is the GTE table divide `sh2o_k_unr_div(H, SZ3)`, not an exact quotient. H is unsigned in the calculation, but its read is sign-extended.

## Hardware behaviours

The port gives the results of these hardware behaviours. The first two are hardware bugs.

- **MVMVA mx = 3:** the matrix is [-R * 10h, R * 10h, IR0; RT13, RT13, RT13; RT22, RT22, RT22]. R is the red byte of RGBC.
- **MVMVA cv = 2 (FC):** the first part, FC * 1000h + M[n][0] * V[0], sets MAC and IR FLAG bits (IR with lm = 0), but it is not in the result. The result is the sum of the last two products only.
- **RTPS and RTPT, IR3:** FLAG bit 22 comes from MAC3 SAR 12 with the limits of lm = 0, for each sf. The value of IR3 uses sf and lm.
- **RTPS and RTPT, SZ3:** SZ3 is the 44-bit sum SAR 12, for each sf.
- **DPCS, DPCT, NCDS and NCDT:** the first step, (FC SHL 12) - MAC, saturates IR1-3 with lm = 0. Only the second step uses lm.
- **AVSZ3:** OTZ comes from the sum before it wraps to 32 bits.

## The fixed-word entries

`include/sh2o/gte.h` has two entries with a fixed command word, contract tags `gte-nrtps-1` and `gte-nrtpt-1`:

```c
void sh2o_gte_nrtps(sh2o_gte_t *g); /* RTPS with 0x4A180001 */
void sh2o_gte_nrtpt(sh2o_gte_t *g); /* RTPT with 0x4A280030 */
```

They are the nRTPS and nRTPT macros of the PSY-Q macro family. The word `0x4A180001` is RTPS with sf 1 and lm 0; `0x4A280030` is RTPT with sf 1 and lm 0. Each entry gives the same image as `sh2o_gte_command(g, word)` with its word, FLAG included. On the PS1, each macro puts two NOPs before the command, to wait for the load delay of the GTE after the register writes. sh2o's image has no load delay, so the entries are the commands alone. With `SH2O_COUNT=ON` they count under their own names, and their commands under `op_01` and `op_30` ([COUNT.md](COUNT.md)).

The host test `gte` runs each entry on the input image of each RTPS and RTPT `cmd` vector (7 and 6) and compares the 64 registers with `sh2o_gte_command` and the entry's word. The test `gte_sources` runs the 13 cases through `tests/test_gte.c --driver` and the Python model. The mutants `gte_entry_mutant_nrtps_sf0` and `gte_entry_mutant_nrtpt_lm1` change the word and must be found. `make sh2-model` does the same comparison in the native and portable SH-2 programs, with SR.S = 0 and SR.S = 1, and gives the MODELLED cycles of one entry call ([benchmarks/gte-model/README.md](../benchmarks/gte-model/README.md)). The records are [validation/gte-nrtps-1.md](validation/gte-nrtps-1.md) and [validation/gte-nrtpt-1.md](validation/gte-nrtpt-1.md).

## DPCT

`include/sh2o/gte.h` has `sh2o_gte_dpct`, contract tag `gte-dpct-1`:

```c
void sh2o_gte_dpct(sh2o_gte_t *g,uint32_t cmd); /* 2Ah: DPCS on RGB0, three times (colour FIFO) */
```

`sh2o_gte_command(g, cmd)` with opcode `2Ah` calls it; a caller can also call it with the word. psx-spx gives DPCS and DPCT one formula: MAC1-3 = [R, G, B] SHL 16, then MAC + (FC - MAC) * IR0, then SAR (sf * 12), then the colour FIFO push [MAC1/16, MAC2/16, MAC3/16, CODE] and IR1-3 = MAC1-3. DPCT "executes thrice, and reads the R,G,B values from RGB0 (ie. reads from the Bottom of the Color FIFO, instead of from the RGBC register)"; CODE still comes from RGBC. Each step's push moves the next colour down to RGB0, so the three steps read the three FIFO colours in turn, and after the command RGB0-2 hold the three results. The (FC - MAC) term saturates with lm = 0, as in DPCS. The COP2 word of DPCT with sf 1 and lm 0 is `0x4AF8002A`.

Checks and counts:

- Host: the test `gte` runs the 13 DPCT `cmd` vectors of `tests/gte_oracle.h` through `sh2o_gte_command` and through `sh2o_gte_dpct` with each vector's word: the 64 registers equal the vector's result. `gte_sources` runs the 100 DPCT cases of ps1-tests through the C and the model: 0 differences. It also compares the model with their expected values, a pinned check: 100 cases, 0 differences. The mutants `dpct_reads_rgbc` and `dpct_fifo_not_advanced` must be found.
- `make sh2-model` runs the same vectors through the command and the entry in the native and portable SH-2 programs, with SR.S = 0 and SR.S = 1, and gives the MODELLED cycles of each ([benchmarks/gte-model/README.md](../benchmarks/gte-model/README.md)).
- With `SH2O_COUNT=ON` a call counts under `SH2O_COUNT_GTE_DPCT` and under `op_2a` ([COUNT.md](COUNT.md)).
- The record is [validation/gte-dpct-1.md](validation/gte-dpct-1.md).

Where the answers come from: the DPCT answers come from `scripts/gte_model.py`, written from psx-spx (problemkaputt.de/psx-spx.htm, "COP2 0F8002Ah - 17 Cycles - DPCT", read 2026-11-22), as the C is; so they are not an independent derivation ([below](#where-the-answers-come-from)). Corroboration: the 100 DPCT cases of ps1-tests give 0 differences between their expected values and the model (pinned in `gte_sources`), and between the model and the C; the software GTE of psyz (`libgte.c:2321`) gives 0 differences on the DPCT command over those 100 cases. The private recordings hold 0 DPCT pairs, so they add no DPCT corroboration (0 model/C differences on DPCT pairs, as there are none); `gte_recordings` gives 0 differences on all 79,802 cases.

## The MVMVA list

`include/sh2o/list.h` has one call, contract tag `gte-mvmva-list-1`:

```c
void sh2o_gte_mvmva_list(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
```

The list does n MVMVA singles in order. For each item k, it writes `v[k][0]` to VXY0 (MTC2 0) and `v[k][1]` to VZ0 (MTC2 1), then runs `sh2o_gte_mvmva(g, cmd)`. `v[k]` is V0 in its word pair form, the PS1 words of an SVECTOR. `cmd` is as for `sh2o_gte_mvmva`: any mx, v, cv, sf and lm. With v other than 0, the items still load V0, as the singles do. When n is 0, the call does nothing.

Each out row has six words, as MFC2 reads them: IR1, IR2 and IR3 (sign-extended from 16 bits), then MAC1, MAC2 and MAC3. After item k, the image and the out row are equal to what the singles give after their item k, on all 64 registers.

The list runs on the calling CPU only (on the Saturn, the master SH-2). It does not use the slave SH-2 or the SCU DSP. The list is C over the GTE image (`src/sh2o_list.c`). On the SH-2, each item takes the MVMVA path of the gte family, so its MAC stage is the k kernel `sh2o_k_mvb3_w` where MVMVA uses it (see [KERNELS.md](KERNELS.md)).

Build the tests with `make test CC=gcc` (the folder `build/gcc`).

- **Oracle vectors.** `scripts/vectors_list.py` writes 88 vectors (op `list`) to `tests/vectors/list.vec`. The reference is the Python model `scripts/gte_model.py`. The vectors have each mx x v x cv selector, n = 0, 1 and 16, the cv = 2 bug, mx = 3, and saturated inputs. [EXACTNESS.md](EXACTNESS.md) gives the layout. Command: `ctest --test-dir build/gcc -R '^list$'`.
- **Suite.** `tests/suite/sh2o_list_test_suite.c` compares each list with its singles, item by item, on all 64 registers. The seed is the gte suite's. Its counts are pinned: 256 runs, 2,144 items, 0 prefixes that differ, 238 of 238 planted reorders caught. The same test runs it. Command: `ctest --test-dir build/gcc -R '^list$'`.
- **Mutants.** `tests/mutant.py` plants 5 list mutants in a copy of `src/sh2o_list.c`: `item_dropped`, `ir_not_sign_extended`, `mac_wrong_register`, `rows_swapped` and `vz0_not_loaded`. Each mutant must give an oracle-vector difference. Command: `ctest --test-dir build/gcc -R '^list_mutant_'`.
- **SH-2.** `make cross` checks the list object with the spec `tests/sh2/list_static.txt`: its only call is `sh2o_gte_mvmva`, and it has no `MAC.W`. `make sh2-model` runs the 88 vectors in the native and the portable program, with zero differences. It also runs the singles of each vector and compares them with the list. It computes the suite counts in each program and gets the pins. With `SR.S` = 1, the native result differs for 0 of the 88 vectors. The library checks `SR.S`. The cycle figures are MODELLED, not Saturn timings: see [benchmarks/list-model/README.md](../benchmarks/list-model/README.md).

The record is [validation/gte-mvmva-list-1.md](validation/gte-mvmva-list-1.md).

## MVMVA with a prepared shadow

`include/sh2o/gte_prepared.h` has two calls, contract tag `gte-mvmva-prepared-1`:

```c
void sh2o_gte_prepared_load(sh2o_gte_prepared_t *p,const sh2o_gte_t *g,unsigned parts);
int sh2o_gte_mvmva_prepared(sh2o_gte_t *g,const sh2o_gte_prepared_t *p,uint32_t cmd);
```

This is a second path for MVMVA, beside `sh2o_gte_mvmva`. It is not the default: no default path calls it, and `sh2o/sh2o.h` does not include its header. The caller includes `sh2o/gte_prepared.h` and owns the shadow `sh2o_gte_prepared_t`.

- **The load.** `sh2o_gte_prepared_load` reads the image g and writes the parts of the shadow p that `parts` names: `SH2O_GTE_PREPARED_RT`, `_LIGHT`, `_COLOR` (the rows of a matrix, each element R SHL 4), `_TR`, `_BK`, `_NONE` (the words of t SHL 16 for MACH and MACL, and the range check of each translation). `SH2O_GTE_PREPARED_ALL` names all six. The load does not change the other parts. The first load of a shadow is `SH2O_GTE_PREPARED_ALL`.
- **The call.** `sh2o_gte_mvmva_prepared` runs MVMVA on g with the shadow p. On its gate it runs the prepared kernel and the sf = 1 tail (MAC1-3, IR1-3 with lm, FLAG bits 24-22) and gives 1. Else it calls `sh2o_gte_mvmva(g, cmd)` and gives 0. In both cases the 64 words of g are equal to those of `sh2o_gte_mvmva`.
- **The gate.** sf = 1, mx < 3, cv other than 2, and each word of the translation that cv selects inside |t| < 7FF40000h (cv = 3 has no translation). The load does the range check; the call reads its result.
- **SR.S.** On the SH-2 the kernel is `MAC.L` rows closed by `XTRCT`. With SR.S = 1, `MAC.L` saturates. The call reads SR.S one time (`STC SR`; it does not write SR) and calls `sh2o_gte_mvmva` when SR.S is 1. SR.S at the load has no effect. The portable path is C and does not read SR.
- **Staleness.** The shadow is a copy. After each write to the registers of a part (a matrix or a translation), the caller loads that part again. A stale shadow gives a wrong result: no function checks it.

Tests:

- **Host.** The test `prepared` (`tests/test_prepared.c`): the MVMVA `cmd` vectors of `tests/gte_oracle.h`; a digest over the vectors of the k suite (pin `0x63A08AA3`, 50,970 vectors on the kernel path); 4,096 random images and words against `sh2o_gte_mvmva` (2,297 on the kernel path); partial loads; the fallback cases; three planted bugs. Command: `ctest --test-dir build/gcc -R '^prepared$'`.
- **SH-2.** `make sh2-model`, family `prepared`: the 71 MVMVA vectors at SR.S 0 and 1 (at the load and at the call), the digest and the random runs in the program, then the MODELLED cycles of the two paths, the two kernels and the loads. See [benchmarks/prepared-model/README.md](../benchmarks/prepared-model/README.md).

The record is [validation/gte-mvmva-prepared-1.md](validation/gte-mvmva-prepared-1.md).

## MVMVA list with a prepared shadow

`include/sh2o/gte_prepared.h` also has a list, contract tag `gte-mvmva-list-prepared-1`:

```c
void sh2o_gte_mvmva_list_prepared(sh2o_gte_t *g,uint32_t cmd,const uint32_t (*v)[2],unsigned n,uint32_t (*out)[6]);
```

It has the arguments and the contract of `sh2o_gte_mvmva_list`: the image after the call and the out rows are equal to those of today's list. It is not the default: no default path calls it.

- **The gate.** The list decides one time for each call. When n = 0 it does nothing. It runs today's list when sf = 0, v is not V0, mx = 3, cv = 2, or (SH-2 only) `SR.S` = 1 (one `STC SR`). Else it loads a shadow on its stack, one time, with only the matrix and the translation that `cmd` selects. When that translation is outside the range of the gate, it runs today's list.
- **The items.** On the gate, each item runs the prepared kernel and the sf = 1 tail into its out row. The image is written one time at the end, as the singles leave it: V0, IR1-3, MAC1-3 and FLAG of the last item.
- **The cost.** The load is in each call: a short list costs more than today's. In the model, n = 2 is the break-even length (see the record).
- **Counts.** With `SH2O_COUNT`, a call adds 1 to `SH2O_COUNT_LIST_PREPARED_CALLS`, n to `SH2O_COUNT_LIST_ITEMS` and n to the MVMVA op counter, on each path ([COUNT.md](COUNT.md)).
- **Host.** The test `list_prepared` (`tests/test_list_prepared.c`): the 88 vectors of `tests/list_oracle.h`, 1,024 random lists against `sh2o_gte_mvmva_list`, two planted bugs and one input mutant. Command: `ctest --test-dir build/gcc -R '^list_prepared$'`.
- **SH-2.** `make sh2-model`, family `listprep`: the same checks at `SR.S` 0 and 1, then the MODELLED cycles an item of the two lists for n = 1 to 8. See [benchmarks/listprep-model/README.md](../benchmarks/listprep-model/README.md).

The record is [validation/gte-mvmva-list-prepared-1.md](validation/gte-mvmva-list-prepared-1.md).

## When to batch

A batch path does the work of a matrix one time and uses it for many vectors. It is faster only when the run on one matrix is long. Use the figures below to decide. They come from the validation records. They are MODELLED cycles on the SH-2 instruction model, native program, `SR.S` = 0. They are not Saturn console timings.

- **Only MVMVA has a batch entry.** The batch entries are the list `sh2o_gte_mvmva_list` (`sh2o/list.h`), the prepared call `sh2o_gte_mvmva_prepared` and the prepared list `sh2o_gte_mvmva_list_prepared` (`sh2o/gte_prepared.h`). RTPS, RTPT, NC, DPCS and the other commands have only single entries.
- **The results are byte-identical.** Each batch entry gives the same 64 image words and the same out rows as the singles. The choice changes only the time.
- **The prepared paths are not the default.** No default path calls them, and `sh2o/sh2o.h` does not include their header. A change of the default is a separate decision and a separate commit.

Cycles for each item of a list call, the load included, command `4A486012h` (record [gte-mvmva-list-prepared-1](validation/gte-mvmva-list-prepared-1.md)):

| Items n | Today's list | Prepared list |
|---|---|---|
| 1 | 454.12 | 605.00 |
| 2 | 440.72 | 365.50 |
| 4 | 432.86 | 245.75 |
| 8 | 429.56 | 185.88 |

- **The break-even.** For the prepared list, the break-even length is n = 2. For the prepared single call with a load of the rotation matrix and the translation, the break-even is about 1.91 calls for each load (record [gte-mvmva-prepared-1](validation/gte-mvmva-prepared-1.md)). With a load of all six parts, it is 4.65 calls for each load.
- **The cost at n = 1.** The prepared list loads its shadow in each call. At n = 1 it costs 605.00 cycles against 454.12 for today's list. A run of one item is slower on the batch path.
- **The fallback.** The prepared list runs today's list when its range check fails (a translation outside the gate), and also for sf 0, mx 3, cv 2 or `SR.S` = 1. A fallback call pays for the check and gets no gain. Over the 84 oracle lists with n > 0, 74 fall back, and the mean is 519.30 cycles an item against 515.28 for today's list.

How a port decides:

1. Count the run length at each call site: the number of MVMVA calls in a row on one matrix. The option `SH2O_COUNT` gives only the mean: the MVMVA calls for each load of the rotation matrix over the whole run ([COUNT.md](COUNT.md)). It does not give the run length at one call site.
2. Use a batch entry only at a call site where the run length is above the break-even, and where the inputs stay in the range of the gate.
3. Keep today's entries at all the other call sites. Do not make a batch path the default.
4. Measure again on the target after the change. The figures above are MODELLED, not console figures.

## Sources and counts

Build the tests with `make test CC=gcc` (the folder `build/gcc`). Each item gives the command that runs it again.

- **Oracle vectors.** `scripts/vectors_gte.py` writes 195 vectors (165 `cmd`, 30 `write`) to `tests/vectors/gte.vec`; 13 of the `cmd` vectors are DPCT. The reference is the model. Command: `ctest --test-dir build/gcc -R '^gte$'`. [EXACTNESS.md](EXACTNESS.md) gives the layout.
- **Suite.** `tests/suite/sh2o_gte_test_suite.c` makes 3,840 cases: 14 opcodes x 256 cases, then the 256 MVMVA selectors. DPCT is not in the suite: its op list is pinned, so the digest did not move. The seed is `0x53484733`. The FNV-1a 32 digest of the register reads is pinned at `0xA50D51B1`. The suite also has 14 known answers, one for each of its 14 opcodes. `tests/gte_model_check.py` computes the same digest and known answers from the model. Command: `ctest --test-dir build/gcc -R '^gte(_model)?$'`.
- **Edge and LCG cases.** `scripts/vectors_gte.py` makes 13,898 edge cases and 61,440 LCG cases (4,096 for each of the 15 opcodes; DPCT's are drawn after the other 14, so the other opcodes' cases did not move). The LCG seed is `0x53484732`. The edge MVMVA group uses the seed `0x53484732 ^ 0x12`. `tests/gte_sources_check.py` compares the C machine with the model on all 64 registers, FLAG included. Command: `ctest --test-dir build/gcc -R '^gte_sources$'`.
- **ps1-tests.** `tests/ps1-tests/gte_cases.txt` has 1,600 cases from ps1-tests (MIT License, Copyright (c) 2020 Jakub Czekański, upstream commit `3c210f4a`; see [NOTICE.md](../NOTICE.md)). `gte_sources` runs them through the C machine and the model. It also compares the model with ps1-tests' expected values for its 100 NCCS cases and its 100 DPCT cases: each count is pinned and 0 differences are required. ps1-tests does not state where its expected values come from, so they are not hardware results. Command: `ctest --test-dir build/gcc -R '^gte_sources$'`.
- **Private recordings.** `SH2O_PS1_GTE_RECORDINGS` names a folder of GTE command pairs outside the repository. The pairs are captures from the PCSX-Redux emulator, not results of PS1 hardware. `gte_recordings` adds the 2,864 clean pairs to the public cases, 79,802 cases in total (75,261 before DPCT, which added 4,541 public cases; the pairs hold no DPCT command). When the variable is not set, the test stops with exit code 77 and CTest shows it as skipped. Command: `SH2O_PS1_GTE_RECORDINGS=<folder> ctest --test-dir build/gcc -R '^gte_recordings$'`.
- **Mutants.** `tests/mutant.py` plants 10 gte mutants in a copy of `src/` and `include/`. Each mutant must give an oracle-vector difference (`gte_mutant_<name>`), and `gte_sources` pins its count of differing cases. Command: `ctest --test-dir build/gcc -R '^gte_(mutant_|sources)'`.

| Mutant | Public cases that differ (76,938) | All cases that differ (79,802) |
|---|---|---|
| `shift_11` | 13,337 | 13,337 |
| `shift_13` | 12,852 | 12,852 |
| `clamp_off_by_one` | 5,808 | 5,808 |
| `unr_index_off_by_one` | 31 | 85 |
| `swapped_flag_bit` | 1,088 | 1,088 |
| `dropped_sxy_push` | 8,833 | 9,057 |
| `dropped_sz_push` | 8,372 | 8,591 |
| `dropped_colour_push` | 38,781 | 39,745 |
| `dpct_reads_rgbc` | 4,540 | 4,540 |
| `dpct_fifo_not_advanced` | 4,198 | 4,198 |

The public counts of `shift_11`, `shift_13` and `dropped_colour_push` moved when DPCT was added (before: 10,618, 10,463 and 34,240 of 72,397 cases); the added differences are all DPCT cases. Over all cases the same three moved (before: 10,618, 10,463 and 35,204 of 75,261 cases), again only on DPCT cases.

- **UNR proofs.** `gte_unr` compares the 65,535 reciprocals (SZ 1..FFFFh) and the `unr[0x101]` table with the model. `gte_unr_sweep` (label `proof`) compares `sh2o_k_unr_div` on all 3,221,192,704 pairs. Three more mutants (`gte_unr_mutant_<name>`) must be caught by these checks. Command: `make proof-tests` for the sweep, `make test` for the others.
- **List.** `tests/vectors/list.vec` has 88 vectors from the model. The list suite has 256 runs and 2,144 items. 5 list mutants. See [The MVMVA list](#the-mvmva-list). Command: `ctest --test-dir build/gcc -R '^list(_mutant_.*)?$'`.
- **SH-2.** `make cross` builds the gte objects for the SH-2 and checks them with the specs `tests/sh2/gte_*_static.txt`. In the native build, the only `MAC.W` instructions are the 9 of `sh2o_k_mvb3`. `make sh2-model` runs the code in the SH-2 instruction model, in the native and the portable program. In each program the 190 vectors are exact, the 3,840 suite cases give the pinned digest, and the 14 known answers hold. Its cycle figures are MODELLED, not Saturn timings: see [benchmarks/gte-model/README.md](../benchmarks/gte-model/README.md).

[VALIDATION.md](VALIDATION.md) has the records (see "Kernel records").

## Where the answers come from

Each GTE oracle gets its answers from one source. No oracle is a result of PS1 hardware.

- **The model.** `scripts/gte_model.py` uses the formulas of psx-spx. The C code also follows psx-spx. Thus the model is not an independent derivation: an error in psx-spx can be in the model and in the C. The other oracles below can show such an error.
- **ps1-tests.** The expected values come from `gte/test-all/tests.c` and the `gte-fuzz` log of ps1-tests at commit `3c210f4a`. ps1-tests does not state where these values come from. Thus they are not hardware results.
- **The recordings.** The 2,864 recorded pairs are captures from the PCSX-Redux emulator (commit `2e053044`). They are emulator results, not PS1 hardware results.
- **psyz.** The software GTE of psyz (`external/psyz/psyz/src/psyz/libgte.c`) is the psyz C. Its comments cite psx-spx (:29, :3297), the source of the model. `psyz_gte` and `psyz_gte_recordings` name a cause for each register that differs. psyz does not decide a difference.

## Not done

- No console run. The library did not run on a Saturn.
- No comparison with the GTE of a second emulator. Generation 2 adds it.
