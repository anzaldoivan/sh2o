# PSY-Q libgte calls

This guide tells how sh2o gives 19 PSY-Q `libgte` calls, with the contract of each call.

**Status: ported.** `include/sh2o/psyq.h` has the 19 bodies, four types and a break hook. Each body is C over the sh2o GTE image (see [GTE.md](GTE.md)). Each body does the steps of the PS1 library code in the same order and gives the same result bytes, on all 64 GTE registers and in memory. sh2o includes no Sony library code. The tests compare the C bodies with the library code of a private PS1 build, run in a Python R3000 model (`scripts/psyq_model.py`). That code is never in the repository.

Each body has `sh2o_gte_t *g` as its first argument. A call that does not use the GTE ignores `g`. Each body has a contract tag, `psyq-<name>-1` in lower case. The header gives the full contract of each body and the sha256 of the PS1 code bytes that it follows.

## The types

The types hold native integers of the SH-2 (the host). Convert PS1 little-endian data (for example a file or a dump) to native integers first. Do not copy PS1 bytes into the structs. The bodies read and write each type one field at a time. A PS1 word is the low field OR the high field SHL 16.

- `sh2o_psyq_matrix_t` (32 bytes): `int16_t m[3][3]` (row major), `int16_t pad`, `int32_t t[3]`.
- `sh2o_psyq_vector_t` (16 bytes): `int32_t vx, vy, vz, pad`.
- `sh2o_psyq_svector_t` (8 bytes): `int16_t vx, vy, vz, pad`.
- `sh2o_psyq_mstack_t`: the matrix stack of PushMatrix and PopMatrix. The caller owns it. `int32_t top` is the count of bytes used, 0 to 0x280 in steps of 32 (20 entries). `uint32_t w[160]` holds the entries (640 bytes). Entry k is `w[8k]` to `w[8k + 7]`: the words of GTE registers 32 to 39 (RT and TR). A zeroed stack is empty.

The PS1 code keeps its matrix stack in two globals. sh2o gives the stack to the call, so a program can have more than one stack.

## The break hook

`void (*sh2o_psyq_break)(uint32_t code)` is a public global. Its default is NULL (no hook).

The PS1 compiler puts two checks around each divide. A zero divisor runs `break 7` (BRK code 0x1C00). INT32_MIN / -1 runs `break 6` (BRK code 0x1800). On the PS1, the BIOS then stops the machine. In sh2o, the body calls the hook with the code, if the hook is set. Then the body stops: it writes nothing more and returns 0 (a `void` body returns).

Only two bodies can reach a break: SetFogNearFar and ratan2.

These PS1 steps are not done: the COP0 SR write and the BIOS patch call of InitGeom, the message print of PushMatrix and PopMatrix, and the return-address words that the PS1 code stores on its stack. The PS1 code traps on a signed overflow in `add`, `addi` and `sub`. sh2o wraps. Only CompMatrix has such an add, and its domain excludes the overflow.

## The bodies

The order is the order of the header. For each body, "Reads" gives the GTE registers that the body or its GTE commands read. "Sets" gives the registers that it writes. After each call, the tests compare all 64 registers and all the memory that the call can write. GTE register numbers: VXY0 0, VZ0 1, IR1-3 9-11, MAC1-3 25-27, RT 32-36, TR 37-39, light matrix 40-44, BK 45-47, LCM 48-52, FC 53-55, OFX 56, OFY 57, H 58, DQA 59, DQB 60, ZSF3 61, ZSF4 62, FLAG 63.

### SetRotMatrix

Tag `psyq-setrotmatrix-1`. `void sh2o_psyq_SetRotMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m)`.

Writes words 0 to 4 of `m` (the 3 x 3 elements and `pad`) to RT. Domain: any matrix. Reads: none. Sets: RT.

### SetLightMatrix

Tag `psyq-setlightmatrix-1`. `void sh2o_psyq_SetLightMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m)`.

Writes words 0 to 4 of `m` to the light matrix. Domain: any matrix. Reads: none. Sets: the light matrix (40-44).

### SetColorMatrix

Tag `psyq-setcolormatrix-1`. `void sh2o_psyq_SetColorMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m)`.

Writes words 0 to 4 of `m` to LCM. Domain: any matrix. Reads: none. Sets: LCM.

### SetTransMatrix

Tag `psyq-settransmatrix-1`. `void sh2o_psyq_SetTransMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m)`.

Writes `m->t` to TR. Domain: any matrix. Reads: none. Sets: TR.

### SetBackColor

Tag `psyq-setbackcolor-1`. `void sh2o_psyq_SetBackColor(sh2o_gte_t *g,int32_t rbk,int32_t gbk,int32_t bbk)`.

Writes each value SHL 4 to BK. Domain: any int. Reads: none. Sets: BK.

### SetFarColor

Tag `psyq-setfarcolor-1`. `void sh2o_psyq_SetFarColor(sh2o_gte_t *g,int32_t rfc,int32_t gfc,int32_t bfc)`.

Writes each value SHL 4 to FC. Domain: any int. Reads: none. Sets: FC.

### SetGeomOffset

Tag `psyq-setgeomoffset-1`. `void sh2o_psyq_SetGeomOffset(sh2o_gte_t *g,int32_t ofx,int32_t ofy)`.

Writes each value SHL 16 to OFX and OFY. Domain: any int. Reads: none. Sets: OFX, OFY.

### SetGeomScreen

Tag `psyq-setgeomscreen-1`. `void sh2o_psyq_SetGeomScreen(sh2o_gte_t *g,int32_t h)`.

Writes `h` to H. Domain: any int. Reads: none. Sets: H.

### InitGeom

Tag `psyq-initgeom-1`. `void sh2o_psyq_InitGeom(sh2o_gte_t *g)`.

Writes ZSF3 = 155h, ZSF4 = 100h, H = 3E8h, DQA = -1062h, DQB = 1400000h, OFX = 0 and OFY = 0, in this order. The COP0 SR write and the BIOS patch call of the PS1 code are not done. Domain: none (no argument). Reads: none. Sets: OFX, OFY, H, DQA, DQB, ZSF3, ZSF4.

### SetFogNearFar

Tag `psyq-setfognearfar-1`. `void sh2o_psyq_SetFogNearFar(sh2o_gte_t *g,int32_t a,int32_t b,int32_t h)`.

All arithmetic wraps to 32 bits. d = b - a. When d < 100, the body does nothing. Else q1 = (-a * b) / d, q2 = (b SHL 12) / d and q3 = (q1 SHL 8) / h. DQA gets q3, clamped to -8000h..7FFFh. DQB gets q2 SHL 12. Domain: any int. When h = 0, or h = -1 and q1 SHL 8 = INT32_MIN, the divide breaks (0x1C00 or 0x1800): the body calls the hook and writes nothing. Reads: none. Sets: DQA, DQB.

### TransMatrix

Tag `psyq-transmatrix-1`. `sh2o_psyq_matrix_t *sh2o_psyq_TransMatrix(sh2o_gte_t *g,sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v)`.

Writes `v->vx`, `v->vy` and `v->vz` to `m->t`. Returns `m`. No GTE access. Domain: any matrix and vector. Reads: none. Sets: none.

### TransposeMatrix

Tag `psyq-transposematrix-1`. `sh2o_psyq_matrix_t *sh2o_psyq_TransposeMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,sh2o_psyq_matrix_t *m1)`.

Writes the transpose of `m0->m` to `m1->m`. `m1->pad` and `m1->t` do not change. Returns `m1`. No GTE access. Domain: any matrix. `m1` can be `m0`. Reads: none. Sets: none.

### CompMatrix

Tag `psyq-compmatrix-1`. `sh2o_psyq_matrix_t *sh2o_psyq_CompMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m0,const sh2o_psyq_matrix_t *m1,sh2o_psyq_matrix_t *m2)`.

Sets RT from `m0`. For each column of `m1->m`, it writes the column to VXY0 and VZ0 and runs MVMVA (sf 1, RT x V0, no translation). IR1-3 give the column of `m2->m`. The high half of IR3 of the last column goes to `m2->pad`. Then it runs MVMVA on the low halves of `m1->t[0]` and `m1->t[1]` (VXY0) and on `m1->t[2]` (VZ0). `m2->t` gets `m0->t` + MAC1-3. Returns `m2`. Domain: `m0->t` + MAC must not overflow 32 bits (the PS1 `add` traps). Reads: RT, VXY0, VZ0, IR1-3, MAC1-3. Sets: RT, VXY0, VZ0, IR1-3, MAC1-3, FLAG.

### ApplyMatrix

Tag `psyq-applymatrix-1`. `sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrix(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_svector_t *v0,sh2o_psyq_vector_t *v1)`.

Sets RT from `m`, VXY0 and VZ0 from `v0` (`v0->pad` in the high half of VZ0). Runs MVMVA (sf 1, RT x V0, no translation). `v1->vx`, `vy` and `vz` get MAC1-3. `v1->pad` does not change. Returns `v1`. The body reads all its inputs before it writes, so `v1` can be `m`. Domain: any matrix and short vector. Reads: RT, VXY0, VZ0. Sets: RT, VXY0, VZ0, IR1-3, MAC1-3, FLAG.

### ApplyMatrixLV

Tag `psyq-applymatrixlv-1`. `sh2o_psyq_vector_t *sh2o_psyq_ApplyMatrixLV(sh2o_gte_t *g,const sh2o_psyq_matrix_t *m,const sh2o_psyq_vector_t *v0,sh2o_psyq_vector_t *v1)`.

Sets RT from `m`. Splits each component of `v0` into a high part (|x| SAR 15) and a low part (|x| AND 7FFFh), each with the sign of x. Runs MVMVA (sf 0, RT x IR) on the high parts and MVMVA (sf 1, RT x IR) on the low parts. Each result component is MAC (high) x 8 + MAC (low), wrapped to 32 bits. Returns `v1`. Domain: any matrix and vector. `v1` can be `v0`. Reads: RT, IR1-3, MAC1-3. Sets: RT, IR1-3, MAC1-3, FLAG.

### RotTrans

Tag `psyq-rottrans-1`. `void sh2o_psyq_RotTrans(sh2o_gte_t *g,const sh2o_psyq_svector_t *v0,sh2o_psyq_vector_t *v1,int32_t *flag)`.

Sets VXY0 and VZ0 from `v0`. Runs MVMVA (sf 1, RT x V0 + TR). `v1->vx`, `vy` and `vz` get MAC1-3. `*flag` gets FLAG. Domain: any short vector. Reads: RT, TR, VXY0, VZ0, MAC1-3, FLAG. Sets: VXY0, VZ0, IR1-3, MAC1-3, FLAG.

### PushMatrix

Tag `psyq-pushmatrix-1`. `int sh2o_psyq_PushMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s)`.

When `s->top` >= 0x280, the stack is full: the body returns -1 and writes nothing. When `s->top` is negative or not a multiple of 32, it returns -2 and writes nothing. Else it reads registers 32 to 39 into entry `top / 32`, adds 32 to `top` and returns 0. Domain: `top` 0 to 0x280 in steps of 32 (the PS1 code writes outside its stack for other values). Reads: RT, TR. Sets: none.

### PopMatrix

Tag `psyq-popmatrix-1`. `int sh2o_psyq_PopMatrix(sh2o_gte_t *g,sh2o_psyq_mstack_t *s)`.

When `s->top` <= 0, the stack is empty: the body returns -1 and writes nothing. When `s->top` is above 0x280 or not a multiple of 32, it returns -2 and writes nothing. Else it subtracts 32 from `top`, writes entry `top / 32` to registers 32 to 39 and returns 0. Domain: `top` 0 to 0x280 in steps of 32. Reads: none. Sets: RT, TR.

### ratan2

Tag `psyq-ratan2-1`. `int32_t sh2o_psyq_ratan2(sh2o_gte_t *g,int32_t y,int32_t x)`.

Returns the angle of (x, y) in units of 1/4096 of a turn. No GTE access. Domain: any pair of ints. Reads: none. Sets: none.

The steps:

1. Negate a negative x or y in 32 bits. INT32_MIN stays negative.
2. When x = 0 and y = 0, return 0. This is not a break.
3. When y < x: q = (y SHL 10) / x, or y / (x SAR 10) when y has a bit set in bits 21-30. t = table[q].
4. Else: q = (x SHL 10) / y, or x / (y SAR 10) when x has a bit set in bits 21-30. t = 400h - table[q].
5. When x was negative, t = 800h - t. When y was negative, t = -t. Return t.

The pairs (0, INT32_MIN) and (INT32_MIN, 0) divide 0 by 0. The body calls the hook with 0x1C00 and returns 0.

The table has 1,025 entries, i = 0 to 1024:

```
table[i] = floor(atan((i + 3/8) / 1024) * 2048 / pi + 1/2)
```

table[1024] = 512 (45 degrees). Each value is at least 0.0008 from a rounding boundary, so each double-precision `atan` gives the same table. `scripts/psyq_tables.py` makes the C literal in `src/sh2o_psyq.c` from the formula. The table bytes are equal to the PS1 table.

## Sources and counts

Build the tests with `make test CC=gcc` (the folder `build/gcc`). Each item gives the command that runs it again.

- **Suite.** `tests/suite/sh2o_psyq_test_suite.c` makes 1,216 cases: 19 bodies x 64 cases. The seed is `0x50535951`. The FNV-1a 32 digest of the return word, the memory words and the 64 register reads is pinned at `0xE430E70B`. The suite also has 6 `ratan2` known answers: (0, 0) gives 0; (0, INT32_MIN) and (INT32_MIN, 0) give 0 and the hook code 0x1C00; (1, 1) gives 0x200; (1, -1) gives 0x600; (-1, -1) gives -0x600. The suite sets the hook only for the known answers. The family has no `.vec` file (see [EXACTNESS.md](EXACTNESS.md)). Command: `ctest --test-dir build/gcc -R '^psyq$'`.
- **Table.** `tests/psyq_table_check.py` makes the 1,025 entries from the formula and compares them with the literal in `src/sh2o_psyq.c`. Command: `ctest --test-dir build/gcc -R '^psyq_table$'`.
- **Mutants.** `tests/mutant.py` plants 19 psyq mutants, one in each body, in a copy of `src/` and `include/`. Each mutant program must make the suite fail (`psyq_mutant_<name>`). 17 mutants change the digest (exit code 0x500). `fog_no_break_hook` fails known answer 2 (0x402). `ratan_index_off_by_one` fails known answer 4 (0x404). The private check `psyq_bodies` also counts the cases that each mutant changes. Command: `ctest --test-dir build/gcc -R '^psyq_mutant_'`.

| Mutant | Body | Private cases that differ |
|---|---|---|
| `back_colour_shift` | SetBackColor | 263 of 265 |
| `rot_matrix_swapped` | SetRotMatrix | 256 of 301 |
| `transpose_dropped_store` | TransposeMatrix | 320 of 365 |
| `rot_trans_no_sf` | RotTrans | 301 of 301 |
| `pop_matrix_bit` | PopMatrix | 40 of 42 |
| `fog_no_break_hook` | SetFogNearFar | 109 of 1,212 |
| `ratan_index_off_by_one` | ratan2 | 894 of 1,465 |
| `init_geom_screen` | InitGeom | 64 of 64 |
| `apply_matrix_swapped` | ApplyMatrix | 320 of 365 |
| `apply_matrix_lv_sign` | ApplyMatrixLV | 336 of 365 |
| `comp_matrix_sign` | CompMatrix | 481 of 491 |
| `push_matrix_top` | PushMatrix | 40 of 42 |
| `colour_matrix_base` | SetColorMatrix | 301 of 301 |
| `far_colour_shift` | SetFarColor | 263 of 265 |
| `geom_offset_shift` | SetGeomOffset | 263 of 265 |
| `geom_screen_plus_one` | SetGeomScreen | 265 of 265 |
| `light_matrix_base` | SetLightMatrix | 301 of 301 |
| `trans_matrix_swapped` | SetTransMatrix | 256 of 301 |
| `trans_matrix_store` | TransMatrix | 256 of 301 |

- **Private comparison with the PS1 code.** Three variables name the inputs of a private PS1 build, outside the repository: `SH2O_PS1_LIBGTE_OBJECTS` (the folder of `libgte` objects), `SH2O_PS1_LINK_MAP` (the link map) and `SH2O_PS1_DATA` (the folder of data objects). `tests/psyq_game_check.py` loads the code of each body into `scripts/psyq_model.py`, a Python R3000 model with the GTE commands of `scripts/gte_model.py`. Nothing in the model comes from the C. When a variable is not set or its path is not there, each test stops with exit code 77 and CTest shows it as skipped. A path in the repository makes the test fail. Command: `SH2O_PS1_LIBGTE_OBJECTS=<folder> SH2O_PS1_LINK_MAP=<file> SH2O_PS1_DATA=<folder> ctest --test-dir build/gcc -R '^psyq_(identity|game_table|bodies|model_suite|ratan2_pairs)$'`. The five tests:
  - `psyq_identity`: the sha256 of the files that it reads is pinned, and the code bytes of each body and the two table ranges have the sha256 that the header gives.
  - `psyq_game_table`: the PS1 table equals the formula.
  - `psyq_bodies`: 7,279 seeded cases. 7,277 are compared. The 2 other cases are CompMatrix cases where the PS1 code traps on an add overflow (outside the domain). The C gives 0 differences on the return, the memory and the 64 registers. SetFogNearFar has 109 cases with a break and ratan2 has 2. The test also runs the 19 mutants (table above).
  - `psyq_model_suite`: the model computes the 1,216 suite cases and gives the pinned digest `0xE430E70B`.
  - `psyq_ratan2_pairs`: 12,169 `ratan2` pairs (169 edge pairs, 6,000 seeded pairs in -2048..2048 and 6,000 xorshift pairs) give 0 differences.

| Body | Cases | Compared | With break |
|---|---|---|---|
| ApplyMatrix | 365 | 365 | 0 |
| ApplyMatrixLV | 365 | 365 | 0 |
| CompMatrix | 493 | 491 | 0 |
| InitGeom | 64 | 64 | 0 |
| PopMatrix | 42 | 42 | 0 |
| PushMatrix | 42 | 42 | 0 |
| RotTrans | 301 | 301 | 0 |
| SetBackColor | 265 | 265 | 0 |
| SetColorMatrix | 301 | 301 | 0 |
| SetFarColor | 265 | 265 | 0 |
| SetFogNearFar | 1,212 | 1,212 | 109 |
| SetGeomOffset | 265 | 265 | 0 |
| SetGeomScreen | 265 | 265 | 0 |
| SetLightMatrix | 301 | 301 | 0 |
| SetRotMatrix | 301 | 301 | 0 |
| SetTransMatrix | 301 | 301 | 0 |
| TransMatrix | 301 | 301 | 0 |
| TransposeMatrix | 365 | 365 | 0 |
| ratan2 | 1,465 | 1,465 | 2 |

- **SH-2.** `make cross` builds `src/sh2o_psyq.c` and the suite for the SH-2 at `-Os` and `-O2`, and checks them with the specs `tests/sh2/psyq_static.txt` and `tests/sh2/psyq_suite_static.txt`. The psyq object needs only `sh2o_gte_read`, `sh2o_gte_write`, `sh2o_gte_command` and `__sdivsi3` (the libgcc signed divide). It has 20 globals, each `sh2o_psyq_`, and no `MAC.W` (see [KERNELS.md](KERNELS.md)). `make sh2-model` runs the code in the SH-2 instruction model, in the native and the portable program. In each program the 1,216 suite cases give the pinned digest, and the 6 known answers hold. The run also plants two faults (`--plant-known 1` and `--plant-digest`), and each must make the harness fail. Its cycle figures are MODELLED, not Saturn timings: see [benchmarks/psyq-model/README.md](../benchmarks/psyq-model/README.md).

## Where the answers come from

Each oracle of the psyq family gets its answers from one source. No oracle is a result of PS1 hardware.

- **The PS1 code (private).** The answers come from the `libgte` code of a private PS1 build. Its version byte is 0x45 (PSY-Q 4.5). `scripts/psyq_model.py` runs this code in a Python R3000 model. The GTE commands of that model are the commands of `scripts/gte_model.py`. Thus a run proves the call sequence and the memory work of each body, and its GTE arithmetic comes from the GTE model (see [GTE.md](GTE.md#where-the-answers-come-from)).
- **The known answers of psyz.** `external/psyz/psyz/tests/test_libgte.c` has 225 cases. psyz does not state where their expected values come from. The psyz decompilation targets PSY-Q 4.0. The 4.0 code is different from the 4.5 code in ApplyMatrixLV, PopMatrix, PushMatrix, SetFogNearFar and ratan2. Thus these answers can agree with sh2o, but they do not decide a difference. The GTE model decides (see [The psyz libgte test](#the-psyz-libgte-test)).
- **The PSY-Q 4.7 library (public, fetched at test time).** The answers come from the 4.7 `libgte` code. For the 19 bodies it is equal to the 4.5 code, apart from the version word and the relocation fields. `scripts/psyq_model.py` runs it with the GTE commands of `scripts/gte_model.py`, as for the private code (see [The PSY-Q 4.7 job](#the-psy-q-47-job)).
- **The ratan2 table and ratan2 of psyz.** `external/psyz/decomp/src/libgte/ratan.c` is the decompiled C of the 4.0 code. Its table entries 0 to 1024 are equal to the sh2o table (`psyz_table`). On 17,834,154 pairs, its `ratan2` and the sh2o `ratan2` give 0 differences (`psyz_ratan2_sweep`).

## The psyz libgte test

`tests/psyz_libgte_check.py` builds the libgte test of the psyz checkout (`external/psyz/psyz/tests/test_libgte.c`) and its runner (`external/psyz/ztest/ztest.c`). The two files stay in place and do not change. The program links them with `tests/psyz/sh2o_libgte_shim.c` and each `src/` C file. No psyz library file is in the program.

The shim has one GTE image. The 19 PSY-Q names call the sh2o bodies. The psyz GTE functions read and write the image registers and send the commands to `sh2o_gte_command`. Each function does the register steps and the commands of its PSY-Q `libgte.h` macro, in the same order. When sh2o does not have a command, the case stops and the runner shows it as skipped. Each other name that the test calls stops the case in the same way.

The test has 225 cases. Each case has one class:

- `pass`: the case passes.
- `differs`: the case fails. The note gives the difference.
- `notrun`: the case stops at a name or a command that sh2o does not have. The line gives that name.

The counts: 116 pass, 1 differs, 108 notrun. The script compares each case with this list and stops with a failure when a case or a count is different. When `external/psyz` is not there, the test is skipped (exit code 77). With `SH2O_REQUIRE_PSYZ=1` it fails. Command: `ctest --test-dir build/gcc -R '^psyz_libgte'`.

A `differs` case gets an arbiter: the GTE model (`scripts/gte_model.py`) on the same input state. The script runs the model and makes sure that its value is the sh2o value and not the psyz value. We do not change sh2o to agree with psyz. When the model agrees with psyz, the case is a sh2o bug to find; it is not a value to copy.

The script also builds each mutant of the `gte` and `psyq` families of `tests/mutant.py` (one known bug in a copy of `src/` and `include/`) with the same psyz objects. A mutant is caught when one or more running cases (`pass` or `differs`) change class. Caught (20): `shift_11`, `shift_13`, `unr_index_off_by_one`, `dropped_sxy_push`, `dropped_sz_push`, `dropped_colour_push`, `back_colour_shift`, `rot_matrix_swapped`, `transpose_dropped_store`, `rot_trans_no_sf`, `apply_matrix_swapped`, `apply_matrix_lv_sign`, `comp_matrix_sign`, `colour_matrix_base`, `far_colour_shift`, `geom_offset_shift`, `geom_screen_plus_one`, `light_matrix_base`, `trans_matrix_swapped`, `trans_matrix_store`. Not reached (9), because no running case gets to the changed code:

- `clamp_off_by_one`, `swapped_flag_bit`: the RTPS/RTPT SX2/SY2 saturation. The cases that saturate the screen (`rtps_screen_saturation`, `rot_trans_pers_set_geom_offset`, `rot_trans_pers3_set_geom_offset`) are `notrun`.
- `pop_matrix_bit`, `push_matrix_top`: `PushMatrix` and `PopMatrix`. Only `notrun` cases call them (sh2o lacks `ReadRotMatrix`).
- `init_geom_screen`: `InitGeom`. Only the `notrun` RotTransPers and RotTransPers3 cases call it.
- `fog_no_break_hook`: the divide break hook. The shim sets no hook, and the running `SetFogNearFar` cases do not divide by zero.
- `ratan_index_off_by_one`: `ratan2`. No case of `test_libgte.c` calls it. `tests/psyz_ratan2_sweep.py` checks it.
- `dpct_reads_rgbc`, `dpct_fifo_not_advanced`: DPCT. The one running DPCT case (`dpct_flags_far_color_difference_saturation`) has RGB0-2 and the RGBC colour all 0, so RGBC and each FIFO entry give the same input. The oracle vectors catch both.

The script also plants two wrong mappings in a copy of `tests/psyz/`: `Psyz_GteLdv0` writes VZ0 to register 2, and `SetTransMatrix` swaps TRX and TRY. Each plant must make one or more running cases fail (16 cases each).

```
rsin notrun rsin
trans_matrix pass
rot_matrix notrun RotMatrix
trans_matrix_negative pass
scale_matrix_identity notrun ScaleMatrix
scale_matrix_per_axis notrun ScaleMatrix
scale_matrix_negative notrun ScaleMatrix
mul_matrix_identity notrun MulMatrix
mul_matrix_half_identity notrun MulMatrix
mul_matrix_permutation notrun MulMatrix
set_mul_matrix_identity notrun SetMulMatrix
set_mul_matrix_general notrun SetMulMatrix
set_mul_matrix_saturates notrun SetMulMatrix
comp_matrix_rotates_and_translates pass
comp_matrix_keeps_low_16_bits_of_m1_translation pass
comp_matrix_saturates_rotation pass
transpose_matrix pass
transpose_matrix_twice_is_identity pass
transpose_matrix_symmetric_unchanged pass
square_root_0 notrun SquareRoot0
square_root_12 notrun SquareRoot12
avsz3_uses_sz123 notrun AverageZ3
avsz4_basic notrun AverageZ4
avsz4_stotz_macros notrun AverageZ4
nclip_ccw notrun NormalClip
nclip_cw notrun NormalClip
nclip_collinear notrun NormalClip
nclip_large_coords notrun NormalClip
nclip_overflow notrun NormalClip
rtps_identity_center notrun RotTransPers
rtps_offset_vertex notrun RotTransPers
rtps_division_overflow notrun RotTransPers
rtps_screen_saturation notrun RotTransPers
rtpt_three_vertices notrun RotTransPers3
rtpt_sz_fifo notrun RotTransPers3
rot_trans_pers_trans_matrix notrun RotTransPers
rot_trans_pers_rot_matrix notrun RotTransPers
rot_trans_pers3_trans_matrix_perspective notrun RotTransPers3
rot_trans_pers3_set_geom_offset notrun RotTransPers3
rot_trans_pers3_trans_and_offset notrun RotTransPers3
rot_trans_pers3_rot_matrix notrun RotTransPers3
rot_trans_pers_set_geom_offset notrun RotTransPers
rot_trans_pers_trans_and_offset notrun RotTransPers
rcos_rsin notrun rcos
rot_matrix_x notrun RotMatrixX
rot_matrix_y notrun RotMatrixY
rot_matrix_z notrun RotMatrixZ
rot_matrix_arbitrary_angles notrun RotMatrix
rot_matrix_negative_and_wrapped_angles notrun RotMatrix
rot_matrix_yxz_arbitrary_angles notrun RotMatrixYXZ
rot_matrix_yxz_negative_and_wrapped_angles notrun RotMatrixYXZ
rot_matrix_yxz_axis_only notrun RotMatrixYXZ
rot_matrix_zyx_arbitrary_angles notrun RotMatrixZYX
rot_matrix_zyx_negative_and_wrapped_angles notrun RotMatrixZYX
rot_matrix_zyx_axis_only notrun RotMatrixZYX
rot_matrix_x_arbitrary_angle notrun RotMatrixX
rot_matrix_x_negative_angle notrun RotMatrixX
rot_matrix_y_arbitrary_angle notrun RotMatrixY
rot_matrix_y_negative_angle notrun RotMatrixY
rot_matrix_z_arbitrary_angle notrun RotMatrixZ
rot_matrix_z_negative_angle notrun RotMatrixZ
rot_matrix_angle_wraps_full_turn notrun RotMatrix
rot_matrix_zero notrun RotMatrixX
apply_matrix_identity pass
apply_matrix_permutation pass
apply_matrix_ignores_translation pass
rot_trans_applies_translation pass
outer_product_0 notrun OuterProduct0
outer_product_0_large notrun OuterProduct0
outer_product_0_truncates_inputs_to_s16 notrun OuterProduct0
outer_product_12 notrun OuterProduct12
outer_product_12_rounds_toward_negative_infinity notrun OuterProduct12
outer_product_12_mixed_signs notrun OuterProduct12
outer_product_preserves_rot_matrix notrun OuterProduct0
rt_stores_untruncated_mac pass
vector_normal notrun VectorNormal
vector_normal_varied notrun VectorNormal
vector_normal_s notrun VectorNormalS
vector_normal_ss notrun VectorNormalSS
rt_applies_rotation_and_translation pass
rt_keeps_screen_xy_fifo pass
stflg_is_zero_without_overflow pass
readflg_matches_stflg pass
stsxy3_g3_fills_poly_vertices pass
stflg_sign_extends_into_long pass
readflg_sign_extends_into_long pass
rtv0_rotates_v0_without_translation pass
rtv1_rotates_v1_without_translation pass
rtv2_rotates_v2_without_translation pass
ldlv0_keeps_low_16_bits pass
rtir_rotates_ir pass
stlvl_stores_saturated_ir pass
ldopv1_loads_rotation_diagonal notrun ReadRotMatrix
ldopv1_spills_high_half_into_next_element notrun ReadRotMatrix
ldopv2_loads_ir pass
ldopv2_keeps_low_16_bits pass
op12_macros_match_outer_product_12 notrun OuterProduct12
stsz_stores_sz3 pass
stsz3_stores_sz1_to_sz3 pass
stsz4_stores_sz0_to_sz3 pass
stsz3c_stores_sz1_to_sz3_contiguously pass
stsz4c_stores_sz0_to_sz3_contiguously pass
push_pop_matrix notrun ReadRotMatrix
pop_matrix_on_empty_stack_keeps_matrix notrun ReadRotMatrix
push_matrix_holds_20_entries notrun ReadRotMatrix
ldrgb3_strgb3_round_trips_color_fifo pass
ldrgb3c_loads_contiguous_colors pass
strgb_stores_rgb2 pass
ldrgb_sets_rgbc pass
ldrgb3_sets_rgbc_to_third_color pass
ldrgb3c_sets_rgbc_to_third_color pass
strgb3_g3_fills_poly_colors pass
strgb3_gt3_fills_poly_colors pass
strgb3_g4_fills_first_three_poly_colors pass
strgb3_gt4_fills_first_three_poly_colors pass
nccs_lights_and_colors_normal pass
nccs_saturates_color pass
dpcs_moves_color_toward_far_color pass
setgeomoffset_and_screen_project pass
setgeomoffset_accepts_negative_offset pass
cmd_00_behaves_like_rtps notrun GTE op 00h
cmd_1a_behaves_like_dcpl notrun GTE op 29h
gte_ldsz3_loads_sz1_to_sz3 pass
gte_ldsz4_loads_sz0_to_sz3 pass
gte_ldsz3_keeps_low_16_bits pass
gte_ldsz3_feeds_avsz3 pass
gte_ldsz4_feeds_avsz4 notrun AVSZ4
dpcs_flags_far_color_difference_saturation pass
dpcs_far_color_difference_ignores_lm pass
dpcs_flags_far_color_difference_overflow pass
dpct_flags_far_color_difference_saturation pass
intpl_flags_far_color_difference_saturation notrun GTE op 11h
dcpl_flags_far_color_difference_saturation notrun GTE op 29h
dcpl_ignores_unused_command_bits notrun GTE op 29h
mvmva_far_color_first_step_ignores_lm pass
rtps_ir0_flags_fraction_above_limit differs FLAG bit 12: psyz 1000h, sh2o 0, GTE model (scripts/gte_model.py) 0: sh2o kept
setfognearfar_fades_from_near_to_far pass
setfognearfar_ignores_narrow_range pass
setfognearfar_clamps_dqa pass
setfognear_fades_up_to_five_times_near notrun SetFogNear
setfognear_wraps_dqa_to_16_bits notrun SetFogNear
gpl12_adds_ir_scaled_by_ir0_to_mac pass
gpl12_pushes_saturated_mac_to_color_fifo pass
gpl12_b_adds_ir_scaled_by_ir0_to_mac pass
gpl0_adds_unscaled_product_to_mac pass
gpl0_b_adds_unscaled_product_to_mac pass
ldbv_loads_two_unsigned_bytes_into_ir pass
ldcv_loads_three_unsigned_bytes_into_ir pass
ldfc_sets_far_color_for_depth_cue pass
set_rgbcd_loads_color_and_code pass
ldsxy0_is_read_back_by_stsxy0 pass
ldsxy1_is_read_back_by_stsxy1 pass
ldsxy2_is_read_back_by_stsxy2_and_stsxy pass
ldsxy3_loads_screen_xy_fifo pass
ldsxy3_feeds_nclip pass
ldsxy3c_loads_contiguous_screen_xy pass
normal_clip_returns_signed_area notrun NormalClip
stsv_stores_ir_as_shorts pass
stsxy0_stsxy1_stsxy2_store_each_screen_xy pass
stsxy01_stores_first_two_screen_xy pass
stsxy3c_stores_screen_xy_contiguously pass
stsxy3_f3_fills_poly_vertices pass
stsxy3_f4_fills_first_three_poly_vertices pass
stsxy3_ft3_fills_poly_vertices pass
stsxy3_ft4_fills_first_three_poly_vertices pass
stsxy3_g4_fills_first_three_poly_vertices pass
stsxy3_gt4_fills_first_three_poly_vertices pass
sttr_stores_translation pass
read_rot_matrix_macro_reads_rotation_and_translation notrun ReadRotMatrix
read_light_matrix_reads_light_and_back_color notrun ReadLightMatrix
read_color_matrix_reads_color_and_far_color notrun ReadColorMatrix
read_color_matrix_reads_ldfc_words notrun ReadColorMatrix
read_geom_offset_reads_integer_offset notrun ReadGeomOffset
read_geom_screen_reads_h notrun ReadGeomScreen
read_geom_screen_sign_extends_h notrun ReadGeomScreen
ldsvrtrow0_loads_first_row_and_spills_pad notrun ReadRotMatrix
ldsvllrow0_loads_first_row_and_spills_pad notrun ReadLightMatrix
ldsvlcrow0_loads_first_row_and_spills_pad notrun ReadColorMatrix
rtv0_b_rotates_v0_without_translation pass
rtv1_b_rotates_v1_without_translation pass
rtv2_b_rotates_v2_without_translation pass
rtv0tr_rotates_v0_and_adds_translation pass
rtv1tr_rotates_v1_and_adds_translation pass
rtv2tr_rotates_v2_and_adds_translation pass
rtv0tr_b_rotates_v0_and_adds_translation pass
rtv1tr_b_rotates_v1_and_adds_translation pass
rtv2tr_b_rotates_v2_and_adds_translation pass
rtv0bk_rotates_v0_and_adds_back_color pass
rtv1bk_rotates_v1_and_adds_back_color pass
rtv2bk_rotates_v2_and_adds_back_color pass
rtv0bk_b_rotates_v0_and_adds_back_color pass
rtv1bk_b_rotates_v1_and_adds_back_color pass
rtv2bk_b_rotates_v2_and_adds_back_color pass
apply_matrix_lv_multiplies_long_vector pass
apply_matrix_lv_wraps_high_half_to_16_bits pass
outer_product12_macro_matches_outer_product_12 notrun OP
outer_product12_svl_crosses_short_vectors notrun OP
outer_product12_svl_sign_extends_into_rotation_matrix notrun OP
outer_product0_svl_keeps_integer_product notrun OP
outer_product12_sv_stores_short_result notrun OP
outer_product0_sv_stores_saturated_ir notrun OP
comp_matrix_macro_multiplies_translates_and_loads_m0 notrun ReadRotMatrix
normal_color_lights_normal_with_back_color pass
normal_color_clamps_negative_light_to_zero pass
normal_color3_lights_three_normals pass
normal_color_col_modulates_light_by_color pass
normal_color_col3_modulates_three_normals notrun NCCT
normal_color_dpq_fades_toward_far_color pass
normal_color_dpq3_fades_three_normals pass
color_col_modulates_light_vector_by_color notrun CC
color_dpq_fades_light_vector_toward_far_color notrun CDP
rot_trans_pers_macro_projects_vertex notrun RotTransPers
rot_trans_pers3_macro_projects_triangle notrun RotTransPers3
rot_trans_macro_rotates_and_translates pass
rot_average3_macro_averages_z notrun RotAverage3
rot_nclip3_macro_stores_everything_and_last_z pass
rot_nclip3_macro_stores_back_facing_triangle pass
rot_average_nclip3_macro_averages_z pass
rot_color_dpq_macro_projects_and_fades_color pass
rot_color_dpq3_macro_projects_and_fades_colors pass
rot_average_nclip_color_dpq3_macro_fades_colors pass
rot_average_nclip_color_col3_macro_modulates_colors notrun NCCT
normal_color_col_function_modulates_light_by_color notrun NormalColorCol
normal_color_dpq_function_fades_toward_far_color notrun NormalColorDpq
dpq_color_moves_color_toward_far_color notrun DpqColor
```

## The PSY-Q 4.7 job

`make psyq47` compares the 19 bodies with public library code. The CI job `4.7 archive = game libgte` (`.github/workflows/sh2o-psyq47.yml`) runs it. The job is optional: it needs the network, and it is not a required check.

1. `scripts/psyq47_job.py fetch` gets `Psy-Q_47.zip` into `build/psyq47/` and checks its sha256. If the download fails or the sha256 is different, the job prints `4.7 archive = game libgte: skipped: <reason>` and stops with exit code 0.
2. After a good download, the job builds sh2o two times: the default build and the build with `SH2O_ZAM=ON`. In each build it runs the tests with the label `psyq47`, with `SH2O_PSYQ47_ARCHIVE` set to the archive and `SH2O_REQUIRE_PSYQ47=1`. Then a test that skips fails.
3. `scripts/psyq47_job.py report` prints one line for each body and the match count. A body matches when its digest is equal to the committed digest and all the `psyq47` tests pass in the two builds. The total is the number of exported code symbols of `LIBGTE.LIB` (561). In CI, the report also goes to the step summary.

The tests:

- `psyq47_digests`: `scripts/psyq_lnk.py` reads `LIB/LIBGTE.LIB` in the archive in place. For the 19 bodies and the two calls `SetDQA` and `SetDQB`, it makes one digest of the code with the relocation fields set to 0, and of the list of relocation sites. `tests/psyq47/game_digests.txt` holds the same digests made from the private PS1 build. The test compares the 21 digests. A body is identical when the sha256 of its code, with no field masked, is equal to its line in `include/sh2o/psyq.h`. The test also compares the `ratan2` table and the 2 bytes before it with the lines of the header.
- `psyq47_model_table`, `psyq47_model_bodies`, `psyq47_model_suite` and `psyq47_model_ratan2`: `scripts/psyq_lnk.py` links the 4.7 code at addresses of its own, and `scripts/psyq_model.py` runs it. The tests use the cases, the seeds and the pinned counts of the private checks (`tests/psyq_game_check.py`), with no link map and no private file.

The run of 2026-11-21 (`make psyq47`):

- The archive: 6,103,526 bytes, sha256 `e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745`. Its version byte is 0x47.
- Digests: 21 of 21 equal. 15 bodies are identical. InitGeom (5 sites), PopMatrix (13), PushMatrix (13) and ratan2 (7) are equal after masking. SetDQA and SetDQB are equal.
- Runs, in the two builds: 7,279 cases, 7,277 compared, 2 CompMatrix traps, 111 cases with a break, 0 differences. Suite: 1,216 cases, digest `0xE430E70B`. ratan2: 12,169 pairs, 0 differences, 2 with a break. Table: equal to the formula. Each of the 19 mutants gives its pinned count.
- Match count: the library has 561 `libgte` functions. sh2o has 19 of them. All 19 match.

The prior-art record of this oracle:

- **Source.** The archive is `https://github.com/psyqz/psyz/releases/download/requirements/Psy-Q_47.zip`, sha256 `e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745`. This is the download of psyz at its pin: `external/psyz/decomp/sdk/Makefile:14-16` gets it and checks it against `external/psyz/decomp/sdk/Psy-Q_47.zip.sha256:1`, and `:10-13` unzips it. sh2o reads the file in place and keeps no byte of it.
- **Where the answers come from, and the alternatives.** The answers come from the 4.7 `libgte` code of Sony. Body by body, it is equal to the 4.5 code of the private PS1 build, apart from the version word and the relocation fields. `scripts/psyq_model.py` runs it. The GTE commands of that model are the commands of `scripts/gte_model.py`, so the GTE arithmetic of the run comes from the GTE model. Rejected: the PSY-Q 4.0 library, because its code is different from the 4.5 code in ApplyMatrixLV, PopMatrix, PushMatrix, SetFogNearFar and ratan2. Rejected as the oracle: the known answers of psyz, because psyz does not state where they come from (they stay a check that agrees; see above).
- **Inputs.** The job runs the same seeded cases, the same suite and the same `ratan2` pairs as the private checks on the 4.5 code. The code is at the addresses of the job, not at the addresses of the PS1 build. The calls outside the bodies (`printf`, `_patch_gte`) are stubs, as in the private run. With the private build and the archive, the private test `psyq47_same` shows that the two runs load the same routines and give the same result for each case.
- **Bar.** 21 of 21 digests equal, 0 differences in the two builds, and the pinned count of each mutant. After a good download, a result below this fails the job.

## Not done

- Public checks are partial. The public checks against psyz at its pin are these. The `ratan2` table: entries 0 to 1024 are equal to psyz (`psyz_table`). The `ratan2` sweep: 17,834,154 pairs, 0 differences (`psyz_ratan2_sweep`). The known answers of psyz: 116 pass, 1 differs (the model gives the result of sh2o), 108 do not run (above). They check the 19 bodies only on the cases that run. The PSY-Q 4.7 job (above) runs all the seeded cases on public library code, but only when the download works.
- The PSY-Q 4.7 job is optional. It needs the network and it is not a required CI check. Without the archive, the public checks of the 19 bodies are the suite, the psyz checks and the mutants.
- No console run. The library did not run on a Saturn.
