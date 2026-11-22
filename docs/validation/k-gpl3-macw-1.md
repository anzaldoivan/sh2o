# Validation record: 2026-11-21 (SH-2 model, k-gpl3-macw-1)

Kernel `sh2o_k_gpl3_w` in `sh2o/inline/sh2/k_sh2.inl.h`, native SH-2 build only. Its C reference has the record [k-gpl3-1](k-gpl3-1.md). The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host:** no host run. The kernel is SH-2 code, and the host builds do not have it. The k mutant tests change only the C references.
- **SH-2 cross build:** `make cross` (`tests/sh2/k_static.txt`) at `-Os` and `-O2`: inlined, no undefined symbol, 3 `MAC.W`, no `SETS`, no GBR, no SR write, no divide step, no constant in the SH-2 register area.
- **SH-2 model, exact:** `make sh2-model`, native program only. The 40 `gpl3` vectors give zero differences through the `MAC.W` kernel, with SR.S = 0. Over the 113,471 bulk vectors the kernel gives the pinned digest `0xCDB04460`. The two native test mutants of the kernel (a dropped `MAC.W` step, the wrong half order) give `0xE55706B3` and `0x75EAF60D`, which are not the pin.
- **SR.S = 1:** the harness runs the 40 vectors again with SR.S = 1: 24 of 40 results differ. The kernel does not read or set SR. This is the known gap of the contract.
- **2026-11-22:** the successor is [k-gpl3-macw-2](k-gpl3-macw-2.md). It checks SR.S, and with SR.S = 1 it gives 0 differences.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and gives the first difference. The same comparison checks each vector.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_gpl3` | w | 98 | 125 | 111.50 | 101.00 | 40 |

- **Not done:** a console run; a run on an emulator with caches and wait states; a check of SR.S (with SR.S = 1 the results differ, see above).
