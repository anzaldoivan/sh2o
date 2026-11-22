# Validation record: 2026-11-22 (host and SH-2 model, gte-dpct32-1)

## 2026-11-22: selected on the SH-2

On the SH-2 build `sh2o_gte_dpct` (and so `sh2o_gte_command` op `2Ah`) now calls `sh2o_gte_dpct32` after the command count and returns; the portable build and the host keep the C path. The kernel and the tests are unchanged. The text below this section is the record from before the selection; its "`sh2o_gte_command` does not call it" no longer holds on the SH-2 build.

- **Host:** `make check && make test` pass (99 tests; the tests that need the private inputs skip).
- **SH-2 cross build:** `make cross`, green; the specs `tests/sh2/gte_static.txt` and `tests/sh2/gte_switch_static.txt` (`-Os` and `-O2`) now allow exactly one undefined symbol, `sh2o_gte_dpct32` (still no libgcc helper). The manifest `tests/sh2/objects.sha256` is regenerated and matches (54 files).
- **SH-2 model, exact:** `make sh2-model` and `make sh2-model SH2O_ZAM=ON`: every family exact; family `dpct32`: 13 of 13 DPCT vectors exact through `sh2o_gte_dpct` and `sh2o_gte_dpct32` at `SR.S` 0 and 1 in the native and the portable program; family `gte`: 190 of 190 vectors and the DPCT entry check (0 of 13 differ) in each program. The planted known answers and digests stop each run.
- **MODELLED cycles, one call at `SR.S` = 0 over the 13 DPCT vectors, same pins.** Before the selection (the run of the record below): the C path 1,688.38 cycles mean, the kernel 813.23. After, native program: `sh2o_gte_dpct` 817.23 (the kernel plus the entry's call), `sh2o_gte_dpct32` 813.23, `sh2o_gte_command` row DPCT 833.23 (was 1,704.38). The portable program keeps the C path: `sh2o_gte_dpct` 1,688.38, row DPCT 1,704.38. With `SH2O_ZAM=ON` the same figures. These are not Saturn timings. Sources: [benchmarks/dpct32-model/README.md](../../benchmarks/dpct32-model/README.md), [benchmarks/gte-model/README.md](../../benchmarks/gte-model/README.md).

| Program | Function | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|
| native | `sh2o_gte_dpct` (selects the kernel) | 727 | 880 | 817.23 | 737.77 | 13 |
| native | `sh2o_gte_dpct32` | 723 | 876 | 813.23 | 734.77 | 13 |
| portable | `sh2o_gte_dpct` (C path) | 1560 | 1770 | 1688.38 | 1537.85 | 13 |
| portable | `sh2o_gte_dpct32` | 723 | 876 | 813.23 | 734.77 | 13 |

- **Lane programs:** the selection changed the sha256 of the native programs that link the gte object (the portable ones did not change); no MODELLED figure other than the native DPCT rows moved (each figure line of the run compared with the run before). Native: gte `50135e43…` -> `01107c86…`; dpct32 `1c4f1149…` -> `4f72e64e…`; k `d6199920…` -> `cf7eeb8e…`; list `b1beb333…` -> `903d8258…`; psyq `86535166…` -> `bc40dc0c…`; fixed `6134b5b9…` -> `41094b30…`. With `SH2O_ZAM=ON`, native: gte `a8958251…` -> `4af91f59…`; list `5be5b09a…` -> `94465cfd…`; psyq `d7484ed3…` -> `c2c65a2e…`; zam `22c3f924…` -> `d91858e5…`; dpct32 `521cbced…` -> `f5961295…`.

## Before the selection

Function `sh2o_gte_dpct32` in `sh2o/gte.h`, source `src/sh2o_gte_dpct32.c`: a second kernel of DPCT (opcode `2Ah`), beside `sh2o_gte_dpct` (record [gte-dpct-1](gte-dpct-1.md)). It gives the same 64 register words for every image and command word, in 32-bit words only: the 44-bit step-1 sum `(FC SHL 12) - (B SHL 16)` is `4096 (FC - 16 B)`, whose FLAG bit 27-i is the signed overflow of `FC - 16 B`; the step-2 sum `IR * IR0 + (B SHL 16)` always fits 32 bits. `sh2o_gte_command` does not call it. The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compiler: GCC 16.2.1.
- **Host, exact:** the test `dpct32` (`tests/test_dpct32.c`). The 13 DPCT `cmd` vectors of `tests/gte_oracle.h`, written through `sh2o_gte_write` and run through `sh2o_gte_dpct32` with the vector's word: 0 of 13 differ on the 64 words of `sh2o_gte_read`. Differential: 1,048,576 images from an LCG (seed `0x44504354`), edge-biased (FC near `INT32_MIN`, `INT32_MAX` and the step-1 overflow bound `INT32_MIN + 16 B`, IR0 `-8000h` and `7FFFh`, RGB bytes 0 and `FFh`, the CODE byte, every sf and lm), through `sh2o_gte_dpct` and `sh2o_gte_dpct32`: 0 differ on all 64 words of the image; 525,588 of them set a step-1 overflow FLAG bit. `make test` passes.
- **Mutants:** the test builds the kernel source again under other names with each planted bug of `SH2OI_DPCT32_MUTANT`. 1, the step-1 overflow FLAG bit dropped: 2 of 13 oracle vectors and 525,588 of 1,048,576 images differ. 2, the step-2 SAR on sf 0 in place of sf 1: 11 of 13 and 1,048,576 of 1,048,576. 3, RGB0 read once in place of at each step: 6 of 13 and 1,046,331 of 1,048,576. The test fails when a mutant gives no difference.
- **SH-2 cross build:** `make cross`, green; the manifest `tests/sh2/objects.sha256` matches (54 files). The spec `tests/sh2/dpct32_static.txt` (`-Os` and `-O2`): no undefined symbol, the one function `sh2o_gte_dpct32`, no call, no `mac.l` (count 0, the plant raises it to 1 and fails), no `MAC.L` or `MAC.W`, no SR write.
- **SH-2 model, exact:** `make sh2-model`, family `dpct32`. In the native and the portable program, the 13 DPCT vectors through `sh2o_gte_dpct` and through `sh2o_gte_dpct32`, with `SR.S` = 0 and with `SR.S` = 1: 13 of 13 exact for each function. The planted known answer (`--plant-known 1`) stops the run with exit 1.
- **MODELLED cycles:** one call of each function with the vector's word, at `SR.S` = 0, over the 13 DPCT vectors, in one run with the same pins. Source: [benchmarks/dpct32-model/README.md](../../benchmarks/dpct32-model/README.md). These are not Saturn timings.

| Program | Function | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|
| native | `sh2o_gte_dpct` (C) | 1560 | 1770 | 1688.38 | 1537.85 | 13 |
| native | `sh2o_gte_dpct32` | 723 | 876 | 813.23 | 734.77 | 13 |

The portable rows are the same as the native rows. The kernel moves no work to another call.

- **Lane programs:** the new source changed the sha256 of every program without the option that links the library; no MODELLED figure of the other families moved (each mean in `benchmarks/{gte,k,list,psyq}-model/README.md` and `benchmarks/README.md` is in the new run). gte `ad5d4335…` -> `50135e43…`, `2c82a705…` -> `ac23d441…`; k `039a433d…` -> `d6199920…`, `d987599d…` -> `531a0bcd…`; list `997524c1…` -> `b1beb333…`, `2f0e8454…` -> `57569bf6…`; psyq `7fc4b52b…` -> `86535166…`, `55292bf7…` -> `07e4457c…`; fixed `aef1b305…` -> `6134b5b9…`, `7ab5999e…` -> `5eef44d0…`. With `SH2O_ZAM=ON` (`--gc-sections`) only the new `dpct32` programs are added.
- **Not done:** `make sh2-model SH2O_ZAM=ON` for this family (its ELFs are built and pinned in the manifest); a console run; a run on an emulator with caches and wait states; a selection of the kernel by `sh2o_gte_command` (a separate change).
