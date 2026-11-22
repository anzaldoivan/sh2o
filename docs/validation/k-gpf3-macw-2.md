# Validation record: 2026-11-22 (SH-2 model, k-gpf3-macw-2)

Kernel `sh2o_k_gpf3_w2` in `sh2o/inline/sh2/k_sh2_macw2.inl.h`, native SH-2 build only. It replaces [k-gpf3-macw-1](k-gpf3-macw-1.md). Each `SH2O_MVB3_SWITCH` mode 3 site calls it; the entry `sh2o_k_gpf3` calls the C reference. Its C reference has the record [k-gpf3-1](k-gpf3-1.md). The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host:** no new host path. The kernel is SH-2 code, and the host builds do not have it. The C reference `sh2o_k_gpf3_c` does not change.
- **SH-2 cross build:** `make cross` (`tests/sh2/k_static.txt`): no undefined symbol, 3 `MAC.W`, 1 `STC SR`, no SR write, no `SETS`, no GBR, no divide step, no constant in the SH-2 register area.
- **SH-2 model, exact:** `make sh2-model`, native program. The 20 `gpf3` vectors give zero differences through `sh2o_k_gpf3_w2`, with SR.S = 0 and with SR.S = 1. A difference with SR.S = 1 fails the run. With SR.S = 1, the -1 kernel `sh2o_k_gpf3_w` still gives 15 of 20 differences; the harness only counts them.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and gives the first difference. The same comparison checks each vector.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_gpf3` | w | 38 | 38 | 38.00 | 31.00 | 20 |
| native | `sh2o_k_gpf3` | w2 | 42 | 42 | 42.00 | 34.00 | 20 |

- **Cost of the SR.S check:** `w2` less `w`, mean MODELLED cycles with SR.S = 0: +4.00. This kernel takes the place of the -1 kernel without a speed win.
- **Method:** SH2ZAM's method "SR.S, read and use a path without `MAC.L`". The kernel reads SR one time with `STC SR` and does not write it. When S is set, it runs the C reference (no `MAC`). When S is clear, it runs the -1 `MAC.W` body, and the SR.S = 0 assumption of that body is then true.
- **Source:** guide `saturn/BATCH_TRANSFORMS.md:201-206`, kernel `saturn/src/sh2zam_batch_sh2.c:17` (`sh2zi_mac_s_clear`), at SH2ZAM `7f99eb70`. The code is sh2o code; SH2ZAM is not linked.
- **Method not used:** "SR.S, save, clear and restore". That method writes SR, and the object specs of the kernels forbid an SR write.
- **Not done:** a console run; a run on an emulator with caches and wait states.
