# prepared family model bridge

`bridge.c` keeps the library functions `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_mvmva`, `sh2o_gte_prepared_load` and `sh2o_gte_mvmva_prepared` (tag `gte-mvmva-prepared-1`) in the ELF. The two kernels are inline, so the bridge has one out-of-line wrapper for each: `sh2o_model_prepared_mac3` (the prepared kernel: `sh2oi_gte_prepared_mac3_l` on the SH-2 build, `sh2oi_gte_prepared_mac3_c` on the portable build) and `sh2o_model_prepared_mac3_a` (the kernel of `sh2o_gte_mvmva`: `sh2o_k_mvb3`, as `sh2o_model_gte_mac3` in [`../gte-model/bridge.c`](../gte-model/bridge.c)). It also has two checks that run in the ELF, ported from `tests/test_prepared.c` with the same seeds and the same draw order: `sh2o_model_prepared_digest_part` (the digest over the vectors of the k suite) and `sh2o_model_prepared_op_part` (the random images and words). Each takes a part of the work and keeps its state in the bridge buffer, so that no call gets to the instruction limit of the model. The harness puts one `sh2o_gte_t` image and one shadow in the bridge buffer `sh2o_model_prepared_io`.

`make sh2-model` links the bridge and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers to these nine functions. The table keeps them in the ELF. It also makes the build fail when a signature in `sh2o/gte.h` or `sh2o/gte_prepared.h` changes and the bridge does not. The model harness [`../sh2-model/prepared_harness.c`](../sh2-model/prepared_harness.c) reads the table from the ELF and makes sure that each entry is the address of its function.

## Checks

The family has no oracle file of its own. Its known answers are the 71 MVMVA (`12h`) `cmd` vectors of `tests/gte_oracle.h`. All comparisons come before any figure. One difference stops the run. For each ELF:

1. For each vector, the harness writes the 64 registers through `sh2o_gte_write` on a zeroed image, loads all parts of the shadow with `sh2o_gte_prepared_load`, and calls `sh2o_gte_mvmva_prepared` with the vector's word. The 64 words of `sh2o_gte_read` must equal the vector's result. The return value must be 1 exactly when the word is on the kernel path and (native ELF only) `SR.S` is 0 at the call. Four runs, `SR.S` at the load then at the call: 0 0, 0 1, 1 1, 1 0. Each run: 71 of 71 vectors exact. Kernel runs: native 21, 0, 0, 21; portable 21 in each run. These counts are pins.
2. The digest at `SR.S` = 0 over the 113,471 vectors of the k suite: `0x63A08AA3`, with 50,970 vectors on the kernel path (pins).
3. 4,096 random images and words against `sh2o_gte_mvmva`: at `SR.S` = 0, 0 differ and 2,297 runs on the kernel path (pin); at `SR.S` = 1, 0 differ, with 0 kernel runs in the native ELF (the fallback) and 2,297 in the portable ELF.
4. The timing step checks each output before it gives a figure: the two whole calls against the vector's result, the two kernels against `T*1000h + M*V` and against the vector's MAC1-3, and each load's shadow against the shadow that the harness calculates (the parts that the load does not name must not change).

The target also runs the harness with `--plant-known 1`: the harness flips bit 0 of a copy of the first result word of MVMVA vector 1. The run must stop with exit code 1 and "known answer". It does: `prepared: native: SR.S 0 then 0: the known answer of MVMVA vector 1 (oracle vector 26, word 00000012) does not hold: register 0 expected 03b605d9, got 03b605d8`.

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `5322dca7580c311bb6fcbd48e49f25f799b14fb46cfc3e4df2a5b1429b180b8b`, portable `5f39f9dd982d768e4ca6e593dd2c415649f4f85661a09977e1a8e3e7c2b354c3` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `790c1d78…`, portable `89c5869d…`). Three runs gave the same figures. These are not Saturn timings: the model has flat RAM, no caches, no wait states and no bus contention.

At `SR.S` = 0, over the 21 MVMVA vectors on the kernel path (sf 1, mx < 3, cv other than 2, each t inside the gate). A whole: one `sh2o_gte_mvmva` call. C whole: one `sh2o_gte_mvmva_prepared` call (it gives 1) after a load of all parts. A kernel: one `sh2o_model_prepared_mac3_a` call. C kernel: one `sh2o_model_prepared_mac3` call. The kernel rows include the call of the wrapper. Load: one `sh2o_gte_prepared_load` call with the parts named, on the same start image. The last two rows are for information: C whole over all 71 MVMVA vectors (run 0 0 of check 1), and C whole at `SR.S` = 1 over the 21 vectors (run 0 1; native: the fallback to `sh2o_gte_mvmva`).

| Program | Call | Cycles min | Cycles max | Cycles mean | Instructions mean | Inputs |
|---|---|---|---|---|---|---|
| native | A whole | 381 | 416 | 400.29 | 360.05 | 21 |
| native | C whole | 202 | 217 | 214.57 | 182.43 | 21 |
| native | A kernel | 133 | 133 | 133.00 | 113.00 | 21 |
| native | C kernel | 74 | 74 | 74.00 | 55.00 | 21 |
| native | load RT\|TR | 354 | 354 | 354.00 | 330.00 | 21 |
| native | load RT | 283 | 283 | 283.00 | 263.00 | 21 |
| native | load TR | 157 | 157 | 157.00 | 148.00 | 21 |
| native | load ALL | 863 | 863 | 863.00 | 792.00 | 21 |
| native | C whole, all MVMVA | 202 | 691 | 461.96 | 409.80 | 71 |
| native | C whole, SR.S 1 | 532 | 567 | 551.29 | 520.86 | 21 |
| portable | A whole | 457 | 492 | 476.29 | 451.05 | 21 |
| portable | C whole | 244 | 259 | 256.57 | 231.43 | 21 |
| portable | A kernel | 210 | 210 | 210.00 | 204.00 | 21 |
| portable | C kernel | 115 | 115 | 115.00 | 103.00 | 21 |
| portable | load RT\|TR | 354 | 354 | 354.00 | 330.00 | 21 |
| portable | load RT | 283 | 283 | 283.00 | 263.00 | 21 |
| portable | load TR | 157 | 157 | 157.00 | 148.00 | 21 |
| portable | load ALL | 863 | 863 | 863.00 | 792.00 | 21 |
| portable | C whole, all MVMVA | 244 | 690 | 494.24 | 448.01 | 71 |
| portable | C whole, SR.S 1 | 244 | 259 | 256.57 | 231.43 | 21 |

The harness also prints the cost of one call at r calls for each load (C whole + load / r) and the break-even r (load / (A whole - C whole)). Native, load RT|TR: 297.48 at r = 4.27, 330.26 at r = 3.06, A whole 400.29, break-even 1.91. Native, load ALL: 416.68, 496.60, break-even 4.65. Portable, load RT|TR: 339.48, 372.26, A whole 476.29, break-even 1.61; load ALL: 458.68, 538.60, break-even 3.93. [docs/validation/gte-mvmva-prepared-1.md](../../docs/validation/gte-mvmva-prepared-1.md) gives the verdict.

With `make sh2-model SH2O_ZAM=ON` the checks and the counts are the same, and so are the C whole, kernel and load rows. The A whole row and the informational rows are different, because there `sh2o_gte_mvmva` is the delegate: A whole 1491.81 (native) and 1623.81 (portable), C whole at `SR.S` = 1 1703.81 (native). ELF sha256 with the option: native `9db50667…`, portable `ea01e2cf…`.

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
