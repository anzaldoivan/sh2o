# Validation record: 2026-11-22 (host and SH-2 model, gte-mvmva-prepared-1)

Functions `sh2o_gte_prepared_load` and `sh2o_gte_mvmva_prepared` in `sh2o/gte_prepared.h`, source `src/sh2o_gte_prepared.c`, inline kernels `sh2oi_gte_prepared_mac3_l` (SH-2) and `sh2oi_gte_prepared_mac3_c` (portable). A second path of MVMVA (opcode `12h`) beside `sh2o_gte_mvmva` (record [gte-mvmva-1](gte-mvmva-1.md)). The load makes a shadow of the matrices and translations one time, and the call uses it. No default path calls it, and `sh2o/sh2o.h` does not include its header. [GTE.md](../GTE.md#mvmva-with-a-prepared-shadow) describes it. [VALIDATION.md](../VALIDATION.md) lists the other records.

## Why this kernel

- **Source of the method.** SH2ZAM at `7f99eb70`, not linked. From SH2ZAM: the `MAC.L` row closed by `XTRCT` (guide `saturn/BATCH_TRANSFORMS.md:193-199`, `saturn/benchmarks/FUSED_SH2.md:61-65`, and kernel `saturn/src/sh2zam_batch_sh2.c:40`). Also the read of `SR.S` and the fallback when it is set (guide `saturn/BATCH_TRANSFORMS.md:201-206`, `saturn/benchmarks/FUSED_SH2.md:67-72`, and kernel `saturn/src/sh2zam_batch_sh2.c:17`). Also the plan prepared at the load, with the range check done one time (guide `saturn/BATCH_TRANSFORMS.md:29-58`, and kernel `saturn/src/sh2zam_prepared.c:48`). SH2ZAM's transform is Q16.16 over a point array and has no GTE tail (IR limits, FLAG). This kernel must give the GTE bytes. The port also uses an earlier prototype of the same design (MIT), measured on an emulator (figures below).
- **Alternatives.** A: the kernel inside `sh2o_gte_mvmva` (kept: the baseline and the fallback). B: SH2ZAM's own row kernel (rejected: it puts SH2ZAM source in sh2o, and the prototype showed no gain over C). C: adopted. Differences from the two sources: the translation goes into MACH:MACL before the rows (the prototype's method, while SH2ZAM adds it after `XTRCT`). Each call reads `SR.S` (SH2ZAM's place, while the prototype read it one time at the load), because S can change after the load. The shadow is a struct that the caller owns.
- **Inputs of the choice.** SH2ZAM has batches of points for each load. The prototype did one MVMVA (sf 1) for each call. The consumer's measured workload: 650.5 MVMVA against 152.3 rotation-matrix loads a frame, a ratio of 4.27 calls for each load. The ratio is 3.06 if the 60.0 segment-matrix operations a frame are also loads (this second reading is not verified). This lane gives MODELLED figures.
- **The bar.** Exact on the host and on the lane at `SR.S` 0 and 1. Also, C whole + load / 4.27 fewer MODELLED cycles than A whole (one `sh2o_gte_mvmva` call) on the same vectors.

## Record

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compiler: GCC 16.2.1.
- **Host, exact:** the test `prepared` (`tests/test_prepared.c`). The 71 MVMVA `cmd` vectors of `tests/gte_oracle.h` (21 on the kernel path): 0 differ from the oracle and from `sh2o_gte_mvmva`. The digest over the 113,471 vectors of the k suite (cmd `80012h`): `0x63A08AA3`, the pin. It equals the digest of the C reference. 50,970 vectors are on the kernel path. 4,096 random images and words (seed `0x53504B31`): 0 differ, 2,297 on the kernel path (pin). Partial loads (the rotation matrix, then the translation, changed and loaded again alone): 0 differ. The fallback (sf 0, mx 3, cv 2, a translation outside the gate): 0 differ, return value 0. `make check && make test` pass.
- **Mutants:** the test builds the source again under other names with each planted bug of `SH2OI_PREPARED_MUTANT`. 1, the preload t SHL 12: 15 of 71 oracle vectors and 1,563 of 4,096 runs differ. 2, the rows R SHL 3: 21 of 71 and 2,297 of 4,096. 3, the gate with `<=`: 2 of 71 oracle vectors and 0 of 4,096 runs (the digest equals the pin). The test fails when a mutant gives no difference.
- **SH-2 cross build:** `make cross`, green. The spec `tests/sh2/prepared_static.txt`. The manifest `tests/sh2/objects.sha256` has the lane ELFs of the new family.
- **SH-2 model, exact:** `make sh2-model`, family `prepared` ([benchmarks/prepared-model/README.md](../../benchmarks/prepared-model/README.md)). In the native and the portable program, the harness runs the 71 MVMVA vectors. It loads the shadow with all parts from the vector's start image. It checks the 64 words against the oracle and the return value against the expected path. There are four runs (`SR.S` at the load, then at the call): 0 0, 0 1, 1 1, 1 0. 71 of 71 exact in each run. Kernel runs, native: 21, 0, 0, 21. Kernel runs, portable: 21, 21, 21, 21. The digest `0x63A08AA3` with 50,970 on the kernel path, at `SR.S` 0. The 4,096 random runs: 0 differ, 2,297 kernel runs at `SR.S` 0. At `SR.S` 1: 0 differ, kernel runs native 0 (the fallback), portable 2,297. The model harness also stops when `SR.S`, the stack or R8-R14 change over a call. The planted known answer (`--plant-known 1`) stops the run with exit 1. `make sh2-model SH2O_ZAM=ON`: the same checks and counts.
- **MODELLED cycles:** one run, at `SR.S` = 0, over the 21 kernel-path vectors, each output checked before a figure. Three runs of `make sh2-model` gave the same figures. Program sha256: native `790c1d78…`, portable `89c5869d…`. These are not Saturn timings.

| Program | Call | Cycles min | Cycles max | Cycles mean | Inputs |
|---|---|---|---|---|---|
| native | A whole (`sh2o_gte_mvmva`) | 381 | 416 | 400.29 | 21 |
| native | C whole (`sh2o_gte_mvmva_prepared`) | 202 | 217 | 214.57 | 21 |
| native | A kernel (`sh2o_k_mvb3`, wrapper) | 133 | 133 | 133.00 | 21 |
| native | C kernel (`sh2oi_gte_prepared_mac3_l`, wrapper) | 74 | 74 | 74.00 | 21 |
| native | load RT\|TR | 354 | 354 | 354.00 | 21 |
| native | load RT | 283 | 283 | 283.00 | 21 |
| native | load TR | 157 | 157 | 157.00 | 21 |
| native | load ALL | 863 | 863 | 863.00 | 21 |
| portable | A whole | 457 | 492 | 476.29 | 21 |
| portable | C whole | 244 | 259 | 256.57 | 21 |
| portable | A kernel | 210 | 210 | 210.00 | 21 |
| portable | C kernel (`sh2oi_gte_prepared_mac3_c`, wrapper) | 115 | 115 | 115.00 | 21 |
| portable | load RT\|TR, RT, TR, ALL | 354, 283, 157, 863 | the same | 354.00, 283.00, 157.00, 863.00 | 21 |

For information: C whole over all 71 MVMVA vectors, mean 461.96 (native) and 494.24 (portable). C whole at `SR.S` = 1 over the 21 vectors (native: the fallback to `sh2o_gte_mvmva`), mean 551.29 (native) and 256.57 (portable).

- **Ratios (MODELLED, native, the means above).** The cost of one call at r calls for each load is C whole + load / r. The load of the bar is RT|TR (one rotation matrix and one translation).
  - Load RT|TR: at r = 4.27, 214.57 + 354.00 / 4.27 = 297.48, against A whole 400.29. At r = 3.06 (not verified), 330.26 against 400.29. Break-even ratio: 354.00 / (400.29 - 214.57) = 1.91.
  - Load ALL: at r = 4.27, 416.68 against 400.29. At r = 3.06, 496.60. Break-even ratio: 863.00 / 185.72 = 4.65.
  - The run's own ratio is 1: for each vector the harness times one load and one call apart. At r = 1, C whole + load RT|TR = 568.57, more than A whole.
  - Portable, the same quantities: RT|TR 339.48 and 372.26 against 476.29, break-even 1.61. Load ALL: 458.68 and 538.60, break-even 3.93.
- **Ratios (emulator).** The earlier prototype, measured on an emulator, not on this lane (emulator-observed). That build had the delegate on, so its figure for today's op is an upper bound. Today's whole 628.6, its kernel 108.1. Arm C whole 298.5, kernel 71.1. Load 323.2. At r = 4.27: 298.5 + 323.2 / 4.27 = 374.2 against 628.6. At r = 3.06: 404.1. Break-even ratio: 323.2 / (628.6 - 298.5) = 0.98. This record does not add these figures to the MODELLED figures.
- **Bar:** exact at SR.S 0 and 1, and C whole + load / 4.27 < A whole: **met**. The MODELLED figures: native 297.48 < 400.29, portable 339.48 < 476.29. With load ALL in place of RT|TR, the sum at 4.27 (416.68) is not less than A whole. So a caller that loads all parts at each matrix write does not get the gain.
- **Lane programs:** the new family adds the programs `prepared` (native `790c1d78…`, portable `89c5869d…`, and with `SH2O_ZAM=ON` native `9db50667…`, portable `ea01e2cf…`). The new library source changed the sha256 of the other programs without the option (the family READMEs list the values). The 5 new MVMVA vectors of the oracle moved the gte family's `12h` rows and its kernel-path rows (21 vectors: whole 400.29 and 476.29). No other figure moved.
- **Not done:** a console run, and an emulator run of this library's code. No measurement of the consumer's ratio with the library's counters. The default path does not call this kernel. A change of the default is a separate commit.
