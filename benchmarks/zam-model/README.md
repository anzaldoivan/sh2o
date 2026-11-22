# zam family model bridge

The zam family is the experimental SH2ZAM delegate (`include/sh2o/zam_experimental.h`, `src/sh2o_zam.c`) on the SH-2 instruction model. It runs only with `make sh2-model SH2O_ZAM=ON`. The delegate stays off by default: the library and `make sh2-model` without the option do not use it. These figures are MODELLED. They make no claim about the speed of the delegate on a Saturn.

`bridge.c` has no wrapper. The line `sh2_model-option: SH2O_ZAM` in it tells `scripts/sh2_model.sh` to build this family only with the option. The model harness calls these functions directly: `sh2o_zam_test_seed`, `sh2o_zam_test_digest_part` and `sh2o_zam_test_vector_image` of the zam suite, the trials `sh2o_zam_transpose_trial`, `sh2o_zam_fog_trial_part` and `sh2o_zam_list_trial`, the trials' case draws `sh2o_zam_transpose_case`, `sh2o_zam_fog_case` and `sh2o_zam_list_case`, `sh2o_gte_command` and `sh2o_gte_read`, and the delegate and core entries `sh2o_zam_TransposeMatrix`, `sh2o_psyq_TransposeMatrix`, `sh2o_zam_SetFogNearFar`, `sh2o_psyq_SetFogNearFar`, `sh2o_zam_mvmva_list` and `sh2o_gte_mvmva_list`. It reads and writes `sh2o_zam_bypass`, `sh2o_zam_delegated` and `sh2o_zam_refused`. The `sh2o_model_zam_io` buffer (4,096 bytes) holds the suite's tally, one set (A) image and two copies of it, its three commands, the trial counts, and the inputs and outputs of the timed trial calls (matrices, (a, b, h), the list's command, v and out rows, the case draws' generators).

`make sh2-model SH2O_ZAM=ON` links the bridge, the zam suite (`tests/suite/sh2o_zam_test_suite.c`), the SH-2 build of the library with `SH2O_ZAM=1` and `libsh2zam.a` (SH2ZAM's `saturn/src/*.c`, without changes) into one ELF for each build mode (native and portable). The link uses `--gc-sections`, with each global symbol of the bridge as a root.

The bridge has a table of typed pointers to these functions and variables. The table keeps them in the ELF. It also makes the build fail when a signature in `include/sh2o/gte.h`, `include/sh2o/zam_experimental.h` or `tests/suite/sh2o_zam_test.h` changes and the table does not. The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of its symbol.

## Checks

For each ELF, the harness does these steps before it gives a figure:

1. It computes the zam suite in the ELF over all 118,347 items (set (A) 113,471, set (B) 4,864, set (C) 12): one `sh2o_zam_test_seed` call, then `sh2o_zam_test_digest_part` calls over consecutive ranges of each set, 512 items in each call. The most instructions of one call are 15,579,165 (native) and 16,057,005 (portable); the limit of the model harness is 100,000,000. The result in each ELF is the pins of `tests/suite/sh2o_zam_test.h`: rc 0, 345,321 cases, 101,380 delegated, 243,941 refused, digest `D444928C`, and 154,694 delegated and 162,410 refused `sh2o_zam_mac3` calls.
2. It runs the trials in the ELF, and their counts are the pins in each ELF. zam-3 (`TransposeMatrix`): 8,198 cases, 8,198 delegated, 0 mismatches. zam-4 (`SetFogNearFar`, 2,048 items in each `sh2o_zam_fog_trial_part` call): 24,832 cases, 5,122 delegated, 19,710 refused, 0 mismatches. zam-5 (`mvmva_list`, one call): 1,024 runs, 12,544 items, 549 delegated, 475 refused, 0 mismatches, 547 of 547 planted swaps caught. zam-2 (a measurement) does not run.
3. Native only: it runs each item again with `SR.S` = 1, from the state that the `SR.S` = 0 chain of one-item calls has before that item, and counts the items that end in a different state, split by the path of the `SR.S` = 0 run: delegated (it raised `sh2o_zam_delegated`) or core. A core difference fails the run; the delegated count is printed only. Core: 0 of 58,293; delegated: 0 of 60,054.
4. For each vector of set (A), the ELF writes its image and its three commands (`sh2o_zam_test_vector_image`). Each command runs through `sh2o_gte_command` on a copy of the image with `sh2o_zam_bypass` 0 (the delegate). When the delegate takes the case, the command runs again on a second copy with `sh2o_zam_bypass` 1 (the GTE core's own kernel). The 64 words of `sh2o_gte_read` of the two copies are equal for each of these cases in each ELF. Native only: the delegate's call runs again with `SR.S` = 1 on a third copy; its result and 64 words must be those of the `SR.S` = 0 run.
5. It runs the case draws of the trials as a chain in the ELF (`sh2o_zam_transpose_case`, `sh2o_zam_fog_case`, `sh2o_zam_list_case`, each from its seed; the trials run on the same draws). Each case runs the delegate entry: `sh2o_zam_TransposeMatrix` (each source apart, into m1, and aliased), `sh2o_zam_SetFogNearFar`, `sh2o_zam_mvmva_list` with `sh2o_zam_bypass` 0 (its fifth argument on the stack). When the delegate accepts, the core entry runs on the same input: `sh2o_psyq_TransposeMatrix`, `sh2o_psyq_SetFogNearFar`, `sh2o_gte_mvmva_list` with `sh2o_zam_bypass` 1. The outputs must be equal before the cycles count: the return and both matrices; the 64 words of `sh2o_gte_read`; those and the n out rows. A refused `SetFogNearFar` must leave the image unchanged. The counts of the chain are the trials' pins in each ELF: zam-3 8,198 calls, 8,198 delegated; zam-4 24,832 calls, 5,122 delegated, 19,710 refused; zam-5 1,024 runs, 12,544 items, 549 delegated, 475 refused; 0 mismatches. Native only: each accepted delegate call runs again with `SR.S` = 1 from the same input; its result and outputs must be those of the `SR.S` = 0 run. The harness prints the differing calls per op, and a difference fails the run: 0 of 20,558 MVMVA, 0 of 52,821 RTPS, 0 of 26,515 RTPT, 0 of 8,198 TransposeMatrix, 0 of 5,122 SetFogNearFar, 0 of 549 list calls.

The target also runs the harness with `--plant-known 1` (bit 0 of the expected result `want` of edge row 1 of set (C) changes in the RAM of the ELF, at the local symbol `_sh2o_zam_edges`) and with `--plant-digest` (the harness compares the digest with the pin XOR 1). Each time, the harness must stop with exit code 1, with "known answer" or "the digest is not the pin".

## MODELLED figures

Source: `make sh2-model SH2O_ZAM=ON`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`.

One call is the cycles (or instructions) of one `sh2o_gte_command` call, from its entry to its return, the call that the gte family times. The cases are the set (A) cases that the delegate takes: 20,558 MVMVA, 52,821 RTPS and 26,515 RTPT in each ELF. Each case runs with the delegate and with the GTE core's own kernel, on the same image.

| Program | Command | Path | Cycles min | Cycles max | Cycles mean | Instructions min | Instructions max | Instructions mean | Count |
|---|---|---|---|---|---|---|---|---|---|
| native | MVMVA | delegate | 1464 | 1632 | 1517.92 | 1321 | 1434 | 1364.86 | 20558 |
| native | MVMVA | core | 496 | 758 | 522.38 | 445 | 676 | 470.71 | 20558 |
| native | RTPS | delegate | 1679 | 1889 | 1750.04 | 1501 | 1667 | 1563.32 | 52821 |
| native | RTPS | core | 691 | 1032 | 753.53 | 621 | 915 | 670.77 | 52821 |
| native | RTPT | delegate | 4863 | 5434 | 5051.23 | 4322 | 4787 | 4510.59 | 26515 |
| native | RTPT | core | 1888 | 2890 | 2065.73 | 1688 | 2558 | 1836.81 | 26515 |
| portable | MVMVA | delegate | 1596 | 1764 | 1649.92 | 1449 | 1562 | 1492.86 | 20558 |
| portable | MVMVA | core | 572 | 756 | 597.01 | 536 | 675 | 560.10 | 20558 |
| portable | RTPS | delegate | 1811 | 2021 | 1882.04 | 1629 | 1795 | 1691.32 | 52821 |
| portable | RTPS | core | 767 | 1030 | 826.80 | 712 | 914 | 758.55 | 52821 |
| portable | RTPT | delegate | 5259 | 5830 | 5447.23 | 4706 | 5171 | 4894.59 | 26515 |
| portable | RTPT | core | 2116 | 2884 | 2283.80 | 1961 | 2555 | 2098.09 | 26515 |

In this model, the delegate path takes more cycles than the core for each command. The delegate figures include its gate.

The trial entries (step 5): one call is the cycles (or instructions) of one call of the entry, on the cases the delegate accepts, delegate (`sh2o_zam_`) against core (`sh2o_psyq_`, or `sh2o_gte_mvmva_list` with `sh2o_zam_bypass` 1) on the same input. TransposeMatrix: 8,198 calls (4,099 sources, apart and aliased); SetFogNearFar: the 5,122 accepted of 24,832; the list: the 549 accepted runs of 1,024, n from 1 to 32 (runs 0-511 n = 8), per call and per item (the cycles of the call divided by its n).

| Program | Entry | Path | Cycles min | Cycles max | Cycles mean | Instructions min | Instructions max | Instructions mean | Count |
|---|---|---|---|---|---|---|---|---|---|
| native | TransposeMatrix | delegate | 142 | 142 | 142.00 | 133 | 133 | 133.00 | 8198 |
| native | TransposeMatrix | core | 41 | 41 | 41.00 | 40 | 40 | 40.00 | 8198 |
| native | SetFogNearFar | delegate | 926 | 991 | 960.73 | 799 | 845 | 822.90 | 5122 |
| native | SetFogNearFar | core | 337 | 346 | 339.73 | 318 | 323 | 319.44 | 5122 |
| native | list call | delegate | 1398 | 7193 | 2874.72 | 1252 | 5696 | 2380.53 | 549 |
| native | list call | core | 569 | 17080 | 4664.79 | 515 | 15530 | 4219.70 | 549 |
| native | list item | delegate | 224.78 | 1433.00 | 339.64 | 174.00 | 1281.00 | 282.29 | 549 |
| native | list item | core | 520.94 | 580.00 | 529.58 | 469.94 | 527.00 | 478.92 | 549 |
| portable | TransposeMatrix | delegate | 142 | 142 | 142.00 | 133 | 133 | 133.00 | 8198 |
| portable | TransposeMatrix | core | 41 | 41 | 41.00 | 40 | 40 | 40.00 | 8198 |
| portable | SetFogNearFar | delegate | 908 | 973 | 942.73 | 781 | 827 | 804.90 | 5122 |
| portable | SetFogNearFar | core | 337 | 346 | 339.73 | 318 | 323 | 319.44 | 5122 |
| portable | list call | delegate | 1528 | 8470 | 3293.04 | 1379 | 7203 | 2865.98 | 549 |
| portable | list call | core | 645 | 19436 | 5333.01 | 606 | 18351 | 5019.81 | 549 |
| portable | list item | delegate | 264.69 | 1563.00 | 388.21 | 222.53 | 1408.00 | 338.36 | 549 |
| portable | list item | core | 596.94 | 656.00 | 605.58 | 560.94 | 618.00 | 569.92 | 549 |

In this model, TransposeMatrix and SetFogNearFar take more cycles with the delegate than with the core. The list takes fewer cycles per item with the delegate on these accepted runs (mean 339.64 against 529.58 native), and its per-item figure varies more (224.78 to 1433.00) than the core's.

An instruction count is not a speed.

## ELF sha256 with SH2O_ZAM=ON

| ELF | native | portable |
|---|---|---|
| fixed | `edf525d5da11045e40d93722eabef9919caeae84c17066ee6ffc719a90978091` | `11c15d3bc5daa5b9b507a1aa7bcab82b26b8570e56f620cb8de56363e9b8fc15` |
| gte | `14999fa2e11ea0f2ad488f2ed4915e3a03e9f18d4f5df9d9bcbc41130312e310` | `0245a4b1420d16df9dad3b0a49891f578a192d967b35b0af928a1d215bc22873` |
| k | `ec846574f7fb23b03f61d302d63f579020aeb5b3088fce200dbcb8514d26d364` | `5455f97d7f5aba553539303b66b890306ac7948c74238eba7556f16d1acc97e3` |
| list | `2519d67fdf3251fb9f2883e7af3e4734c74f2d01c1168a9d35f46c28b7502bdc` | `c720c3914e09f5a04abc30d20a7624dcd46b08202d8a047e723cac8f8813e4b6` |
| psyq | `c2c65a2e05b495299696b39518dc656ecfd7d0636abcbeec2452f9d751852e5e` | `71018e26756efe24cfddfd26e57bab7249cd47ef1894c1c6b7faf360cb2966c3` |
| zam | `311211c5bf01c0ac1920975c392bd4daba2b07fa82b83196ce471f80eeb2481a` | `3fc5310d36919eeb8ad4c607925de8427e5e5bc2b5ac5757edaf6bab10706cfe` |
| prepared | `9db50667d0f470a9721d959c464c8f468be6873253ce43fdeecc735434589343` | `ea01e2cfb1f9889d1cecc68ab36229f5b2ab50e00d8eeffcb5f122ca27b19d4e` |
| listprep | `f291bb630c3b96d273ebef25b48d4c9f8cad7428354b133fa68283601469a78f` | `daaa5503b6d22c12ea193b83c45bd8ae3a2f7411ca64c7a0f63d1cd57a1fdd9e` |

The listprep row is new (2026-11-22): with the option the listprep program gives the same checks and counts as without it. The prepared MVMVA list source did not change the other rows: with the option the link drops its unreferenced sections. The gte row changed when DPCT (`sh2o_gte_dpct`) joined the library and the bridge table (before: `1a622752…`, `0822a2ca…`); with the option the gte program gives the 195 vectors (190 before the 5 MVMVA vectors of 2026-11-22) and the DPCT entry check, as without it. The prepared row is new (2026-11-22): with the option the prepared program gives the same checks and the same arm C figures as without it; its `sh2o_gte_mvmva` row is the delegate (mean 1491.81 native, 1623.81 portable). The list, psyq and zam rows changed too (2026-11-22: DPCT added to `src/sh2o_gte.c`, which they link; their figures did not move); before: list `677c66ed…`, `8a9d6d19…`; psyq `3e8765ca…`, `8702c1f7…`; zam `f6d964aa…`, `ed1be10c…`. The fixed and k rows did not change. 2026-11-22: the native `sh2o_gte_dpct` selects the 32-bit DPCT kernel `sh2o_gte_dpct32`, which the native gte, list, psyq and zam programs now link; their native rows changed (before: gte `a8958251…`, list `5be5b09a…`, psyq `d7484ed3…`, zam `22c3f924…`), the portable rows did not; only the native DPCT figures moved (gte and dpct32), as without the option. 2026-11-22: the zam row changed when the bridge gained the case draws and the trial entries and its buffer grew to 4,096 bytes (before: `d91858e5…`, `754a4093…`); the set (A) figures did not move; the other rows did not change. 2026-11-22: the gte and list rows changed when their bridges gained the kernel wrapper `sh2o_model_<fam>_mac3` and the harnesses the whole-call and kernel step (before: gte `4af91f59…`, `c1e0f1c6…`; list `94465cfd…`, `265b89c4…`); the other rows did not change.

With the option, the other families give the same oracle results, digests (gte `A50D51B1`, psyq `E430E70B`) and counts as without it. Their `SR.S` = 1 runs give 0 differences, as without it (each a pass or a failure).

## Run

```sh
make sh2-model SH2O_ZAM=ON
```

The ELFs and the harness are in `build/sh2-model-zam`. The output of the plant runs is in `build/sh2-model-zam/zam-plant-known.out` and `.err`, and `zam-plant-digest.out` and `.err`.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
