# listprep family model bridge

`bridge.c` keeps the library functions `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_mvmva_list`, `sh2o_gte_mvmva_list_prepared` (tag `gte-mvmva-list-prepared-1`) and `sh2o_gte_prepared_load` in the ELF. The two lists have five arguments: the harness puts the fifth (`out`) on the stack. The bridge also has one check that runs in the ELF, ported from check 2 of `tests/test_list_prepared.c` with the same seed and the same draw order: `sh2o_model_listprep_check_part`. It takes a part of the runs and keeps its state in the bridge buffer `sh2o_model_listprep_io`, so that no call gets to the instruction limit of the model.

`make sh2-model` links the bridge and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers to these six functions. The table keeps them in the ELF. It also makes the build fail when a signature in `sh2o/gte.h`, `sh2o/list.h` or `sh2o/gte_prepared.h` changes and the bridge does not. The model harness [`../sh2-model/listprep_harness.c`](../sh2-model/listprep_harness.c) reads the table from the ELF and makes sure that each entry is the address of its function.

## Checks

The family has no oracle file of its own. Its known answers are the 88 vectors of `tests/list_oracle.h`. All comparisons come before any figure. One difference stops the run. For each ELF:

1. For each vector, the harness writes the 61 words through `sh2o_gte_write` in the write order of `tests/test_list.c` on a zeroed image and calls `sh2o_gte_mvmva_list_prepared` on its n items. The 16 out rows and the 64 words of `sh2o_gte_read` must equal the vector's result. Two runs: `SR.S` = 0 and `SR.S` = 1. Each run: 88 of 88 vectors exact, 10 in the gate (n > 0, sf 1, v 0, mx < 3, cv other than 2, each t inside the gate). These counts are pins.
2. 1,024 random lists against `sh2o_gte_mvmva_list` (8 chunks of 128 runs), at `SR.S` = 0 and at `SR.S` = 1: 0 differ, 587 runs in the gate (pin), and on those runs items 0 and 1 swapped differ from the reference 587 times (pin). In the native ELF at `SR.S` = 1 the list falls back to `sh2o_gte_mvmva_list`: the most instructions of one chunk are 1,637,817 against 1,174,072 at `SR.S` = 0.
3. The timing step checks each output before it gives a figure: each pair of calls (today, arm C) starts from the same image, and the out rows and the 64 words of the two must be equal; the load's `ok[2]` must be 1.

The target also runs the harness with `--plant-known 1`: the harness flips bit 0 of a copy of the first result byte of vector 1. The run must stop with exit code 1 and "known answer". It does: `listprep: native: SR.S 0: the known answer of vector 1 (cmd 00082012, n 6) does not hold: result byte 0 expected 77, got 76`.

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `18de5e5c8a22324a5db5639bb3dff33b6927983cb5d2dcebd953938e01378e0c`, portable `4ed269cf851e39822ebdc05a11ce7881a069dedeb55a2ad3902f6f31d49a8708`. These are not Saturn timings: the model has flat RAM, no caches, no wait states and no bus contention.

At `SR.S` = 0. Whole: one list call over its n items, in cycles (mean). Today: `sh2o_gte_mvmva_list`. Arm C: `sh2o_gte_mvmva_list_prepared`, with its load of the shadow included. Per item: whole / n. The n rows use `cmd` `4A486012h` (RT, V0, no translation, sf 1) on 32 images and items that the harness draws from the seed `4C503031h` (each in the gate). The oracle rows are the mean of cycles / n over the vectors of `tests/list_oracle.h` with n > 0: all 84, and the 10 in the gate.

| Program | n | Today whole | Arm C whole | Today per item | Arm C per item |
|---|---|---|---|---|---|
| native | 1 | 454.12 | 605.00 | 454.12 | 605.00 |
| native | 2 | 881.44 | 731.00 | 440.72 | 365.50 |
| native | 3 | 1307.78 | 857.00 | 435.93 | 285.67 |
| native | 4 | 1731.44 | 983.00 | 432.86 | 245.75 |
| native | 5 | 2157.72 | 1109.00 | 431.54 | 221.80 |
| native | 6 | 2586.75 | 1235.00 | 431.12 | 205.83 |
| native | 7 | 3007.78 | 1361.00 | 429.68 | 194.43 |
| native | 8 | 3436.47 | 1487.00 | 429.56 | 185.88 |
| native | oracle, 84 | | | 519.30 | 515.28 |
| native | oracle in the gate, 10 | | | 433.31 | 285.64 |
| portable | 1 | 530.12 | 655.75 | 530.12 | 655.75 |
| portable | 2 | 1033.44 | 824.56 | 516.72 | 412.28 |
| portable | 3 | 1535.78 | 994.75 | 511.93 | 331.58 |
| portable | 4 | 2035.44 | 1163.50 | 508.86 | 290.88 |
| portable | 5 | 2537.72 | 1332.12 | 507.54 | 266.43 |
| portable | 6 | 3042.75 | 1497.38 | 507.12 | 249.56 |
| portable | 7 | 3539.78 | 1673.31 | 505.68 | 239.04 |
| portable | 8 | 4044.47 | 1840.12 | 505.56 | 230.02 |
| portable | oracle, 84 | | | 559.02 | 552.88 |
| portable | oracle in the gate, 10 | | | 509.31 | 332.24 |

The load inside the call (`sh2o_gte_prepared_load` with parts RT|NONE, the parts of `4A486012h`): 327 cycles, 294 instructions, in each ELF. Arm C's whole call grows by 126 cycles per item in the native ELF (today: about 426), and its fixed part is about 479 cycles, the load included. Break-even n: 2 in each ELF (the first n where arm C's whole call is under today's).

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
