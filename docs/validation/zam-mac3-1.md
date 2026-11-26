# Validation record: 2026-11-21 (host and SH-2 model, zam-mac3-1)

Function `sh2o_zam_mac3` in `sh2o/zam_experimental.h`: the MAC stage of the GTE core through the prepared MAC executor of SH2ZAM. It is experimental. The GTE core calls it only in a build with the option `SH2O_ZAM` on. It calls it for MVMVA with sf 1, mx < 3 and cv != 2, and for RTPS and RTPT. [ZAM.md](../ZAM.md) describes the delegate. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test SH2O_ZAM=ON` passes with GCC and with Clang. The test `zam` runs the suite `tests/suite/sh2o_zam_test_suite.c`: 345,321 cases, 101,380 delegated, 243,941 refused, digest `0xD444928C`, rc 0, and 154,694 delegated and 162,410 refused `sh2o_zam_mac3` calls. These are the pins of `tests/suite/sh2o_zam_test.h`. For each delegated case the 64 words of the image are equal to the GTE core's own result. The edge rows of the gate hold. The gate delegates the largest admitted row, 2^47 - 16, and refuses the next, 2^47. The mutants of `src/sh2o_zam.c` that change this function: `rows_shift_3` (rc `0x1001`), `gate_2_49` (rc `0x1101`), `tr_dropped` (rc `0x1001`). Each test `zam_mutant_<name>` passes only when the rc equals its pin. The test `zam_isolation` finds the tree clean and catches 8 of 8 planted leaks of the option.
- **The option:** `SH2O_ZAM` on (`make test SH2O_ZAM=ON`). It is off by default, so the default build and the install do not use the delegate.
- **SH-2 cross build:** `make cross`. `tests/sh2/zam_static.txt` (`src/sh2o_zam.c` in the default build, at `-Os` and `-O2`): no undefined symbol, no global symbol, no instruction. The check finds the planted count. The suite `tests/suite/sh2o_zam_test_suite.c` compiles in the native and in the portable build.
- **SH-2 model, exact:** `make sh2-model SH2O_ZAM=ON`. In the native program and in the portable program, the suite over all 118,347 items gives the pins. These are rc 0, 345,321 cases, 101,380 delegated, 243,941 refused, digest `0xD444928C`, and 154,694 / 162,410 `sh2o_zam_mac3` calls. Set (A) has 20,558 MVMVA, 52,821 RTPS and 26,515 RTPT delegated cases. For each of these cases, the 64 words after the command with the delegate equal the 64 words with the GTE core's own kernel. Program sha256: native `aa8a9e19…`, portable `8602b1fd…`.
- **SR.S:** the harness runs each suite item again in the native program with SR.S = 1. 49,236 of the 118,347 items end in a different state. The harness reports this total for all the items, not a count for each function. The library does not check SR.S. This count is not a pass or a failure.
- **2026-11-22:** with SR.S = 1 the count is now 0 (core 0 of 58,293, delegated 0 of 60,054). The library checks SR.S: record [k-mvb3-macw-2](k-mvb3-macw-2.md).
- **Planted difference:** with `--plant-known 1` the harness changes bit 0 of the expected result of edge row 1 of set (C). The run stops with exit 1 ("the known answer of edge row 1 does not hold"). With `--plant-digest` the harness compares the digest with the pin XOR 1, and the run stops with exit 1.
- **MODELLED cycles:** one call of `sh2o_gte_command`, from its entry to its return, on the delegated set (A) cases. The model times the delegate and the GTE core's own kernel on the same image. Source: [benchmarks/zam-model/README.md](../../benchmarks/zam-model/README.md), with the program sha256 values (native `aa8a9e199c67eb27fa72b16f216be764bf8fed32e227b169b45be3625ef90700`, portable `8602b1fdaccdca8083cc576ee5741689fb8e084ab6cf2f1b435a2225eba21c60`). In this model the delegate takes more cycles than the core for each command. These are not Saturn timings.

| Program | Command | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Count |
|---|---|---|---|---|---|---|---|
| native | MVMVA | delegate | 1464 | 1632 | 1517.92 | 1364.86 | 20558 |
| native | MVMVA | core | 482 | 758 | 508.62 | 458.92 | 20558 |
| native | RTPS | delegate | 1679 | 1889 | 1750.04 | 1563.32 | 52821 |
| native | RTPS | core | 677 | 1032 | 740.03 | 659.20 | 52821 |
| native | RTPT | delegate | 4863 | 5434 | 5051.23 | 4510.59 | 26515 |
| native | RTPT | core | 1846 | 2890 | 2025.52 | 1802.34 | 26515 |
| portable | MVMVA | delegate | 1596 | 1764 | 1649.92 | 1492.86 | 20558 |
| portable | MVMVA | core | 575 | 751 | 599.87 | 562.03 | 20558 |
| portable | RTPS | delegate | 1811 | 2021 | 1882.04 | 1691.32 | 52821 |
| portable | RTPS | core | 770 | 1025 | 829.52 | 760.41 | 52821 |
| portable | RTPT | delegate | 5259 | 5830 | 5447.23 | 4894.59 | 26515 |
| portable | RTPT | core | 2125 | 2869 | 2291.78 | 2103.58 | 26515 |

- **Not done:** a console run and a run on an emulator with caches and wait states. Also not done: a static check of the SH-2 object with the option on, and a check of SR.S. Also not done: a count for each function of the SR.S = 1 differences.
- **2026-11-22:** `make cross` checks the object of `src/sh2o_zam.c` with the option on: `tests/sh2/zam_on_static.txt` (`-DSH2O_ZAM=1`, SH2ZAM's headers, `-Os` and `-O2`). Its undefined symbols are exactly these: `sh2o_gte_write`, `sh2z_atan2`, `sh2z_mat33_transpose`, `sh2z_mesh_view_init`, `sh2z_muldiv`, `sh2z_transform_plan_execute_mac` and `sh2z_transform_plan_init`. None of them is an internal SH2ZAM name or a libgcc name. The object defines 8 global symbols, each `sh2o_zam_`. It has no `mac.l` and no `mac.w` instruction. A planted count and a planted extra undefined name each make the check fail. The model now counts the SR.S = 1 differences for each command too. The delegate calls give 0 of 20,558 MVMVA, 0 of 52,821 RTPS and 0 of 26,515 RTPT. Program sha256: native `311211c5…`, portable `3fc5310d…` (the bridge and the suite changed, but no MVMVA, RTPS or RTPT figure moved).
