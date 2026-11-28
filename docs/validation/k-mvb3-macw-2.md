# Validation record: 2026-11-22 (SH-2 model, k-mvb3-macw-2)

Kernel `sh2o_k_mvb3_w2` in `sh2o/inline/sh2/k_sh2_macw2.inl.h`, native SH-2 build only. It replaces [k-mvb3-macw-1](k-mvb3-macw-1.md) in the selection: the entry `sh2o_k_mvb3` calls it on the SH-2, and each `SH2O_MVB3_SWITCH` mode 3 site calls it. Its C reference has the record [k-mvb3-1](k-mvb3-1.md). [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host:** no new host path. The kernel is SH-2 code, and the host builds do not have it. The C reference `sh2o_k_mvb3_c` does not change.
- **SH-2 cross build:** `make cross` (`tests/sh2/k_static.txt`). The object has no undefined symbol, 9 `MAC.W` and 1 `STC SR`. It has no SR write, no `SETS`, no GBR, no divide step and no constant in the SH-2 register area.
- **SH-2 model, exact:** `make sh2-model`, native program. The 30 `mvb3` vectors give zero differences through `sh2o_k_mvb3_w2` and through the entry, with SR.S = 0 and with SR.S = 1. A difference with SR.S = 1 fails the run. With SR.S = 1, the -1 kernel `sh2o_k_mvb3_w` still gives 16 of 30 differences. The harness only counts them.
- **SR.S = 1, the other families:** the native and portable programs give zero differences. The counts are gte 0 of 3,840 suite cases and 0 of 147 `cmd` vectors, and psyq 0 of 1,216 suite cases. They are also list 0 of 88 vectors, zam core 0 of 58,293 items and delegated 0 of 60,054 items. Each harness fails the run on a difference.
- **Planted difference:** with the entry `sh2o_k_mvb3` set back to `sh2o_k_mvb3_w`, the runs fail. The counts are gte 493 of 3,840 suite cases and 5 of 147 `cmd` vectors, and k `mvb3` entry 16 of 30. They are also list 18 of 88 and psyq 199 of 1,216. The harness also changes one bit of the expected result of vector 0, and the run stops with exit 1.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_mvb3` | entry | 133 | 133 | 133.00 | 113.00 | 30 |
| native | `sh2o_k_mvb3` | w | 114 | 114 | 114.00 | 95.00 | 30 |
| native | `sh2o_k_mvb3` | w2 | 137 | 137 | 137.00 | 116.00 | 30 |

- **Cost of the SR.S check:** `w2` less `w`, mean MODELLED cycles with SR.S = 0: +23.00. The entry and the `w2` rows run the same kernel, but the compiler builds their wrappers differently. This kernel takes the place of the -1 kernel without a speed win.
- **Whole MVMVA, native, mean MODELLED cycles:** gte `12h` 491.92 (484.44 with the -1 kernel), source [benchmarks/gte-model/README.md](../../benchmarks/gte-model/README.md). The single `sh2o_gte_mvmva` of the list suite: 482.11 (475.02 with the -1 kernel), source [benchmarks/list-model/README.md](../../benchmarks/list-model/README.md).
- **Method:** SH2ZAM's method "SR.S, read and use a path without `MAC.L`". The kernel reads SR one time with `STC SR` and does not write it. When S is set, it runs the C reference (no `MAC`). When S is clear, it runs the -1 `MAC.W` body, and the SR.S = 0 assumption of that body is then true.
- **Source:** guide `saturn/BATCH_TRANSFORMS.md:201-206`, kernel `saturn/src/sh2zam_batch_sh2.c:17` (`sh2zi_mac_s_clear`), at SH2ZAM `7f99eb70`. The code is sh2o code, and the build does not link SH2ZAM.
- **Method not used:** "SR.S, save, clear and restore". That method writes SR, and the object specs of the kernels forbid an SR write.
- **Not done:** a console run and a run on an emulator with caches and wait states.
