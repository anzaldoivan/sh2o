# dpct32 family model bridge

`bridge.c` has no wrapper. The model harness calls the library functions `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_dpct` (DPCT: the kernel on the SH-2 build, the C path on the portable build) and `sh2o_gte_dpct32` (the 32-bit kernel, tag `gte-dpct32-1`) directly. The harness puts one `sh2o_gte_t` image at the start of the bridge buffer `sh2o_model_dpct32_io`.

`make sh2-model` links the bridge and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers: `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_dpct` and `sh2o_gte_dpct32`. The table keeps these functions in the ELF. It also makes the build fail when a signature in `sh2o/gte.h` changes and the bridge does not. The model harness [`../sh2-model/dpct32_harness.c`](../sh2-model/dpct32_harness.c) reads the table from the ELF and makes sure that each entry is the address of its function.

## Checks

The family has no oracle file of its own. Its known answers are the 13 DPCT (`2Ah`) `cmd` vectors of `tests/gte_oracle.h`. For each ELF, with `SR.S` = 0 and with `SR.S` = 1, the harness writes each vector's 64 registers through `sh2o_gte_write` in the write order of the gte suite on a zeroed image, calls `sh2o_gte_dpct` or `sh2o_gte_dpct32` with the vector's word, and reads the 64 registers with `sh2o_gte_read`. The 64 words must equal the vector's result, for each function. All comparisons come before any figure. One difference stops the run. Each ELF gives 13 of 13 vectors exact through each function at `SR.S` 0 and 1.

The target also runs the harness with `--plant-known 1`: the harness flips bit 0 of a copy of the first result word of DPCT vector 1. The run must stop with exit code 1 and "known answer". It does: `dpct32: native: SR.S 0: C: the known answer of DPCT vector 1 (oracle vector 115, word 0000042a) does not hold`.

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `84e36fb41940e6a1e815026a2c2d8bbe46fee4eb7151049cf654448a3745d3f9`, portable `246e2eb6a34bed84205d580bc88ac9bbbaba77911de52a1efd883b5db66f1c9a` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `ece5bba3…`, portable `410e6c18…`; 2026-11-22: the prepared MVMVA source `src/sh2o_gte_prepared.c` joined the library and changed the hashes, not the figures; before it: native `4f72e64e…`, portable `49f68f3a…`; 2026-11-22: the native `sh2o_gte_dpct` selects `sh2o_gte_dpct32`, which changed the native hash and the native `sh2o_gte_dpct` row; before it: native `1c4f1149…`). The figures are for one call of each function with the vector's word, from its entry to its return, at `SR.S` = 0, over the 13 DPCT vectors. They do not include the register writes and reads. These are not Saturn timings: the model has flat RAM, no caches, no wait states and no bus contention.

| Program | Function | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|
| native | `sh2o_gte_dpct` (selects the kernel) | 727 | 880 | 817.23 | 737.77 | 13 |
| native | `sh2o_gte_dpct32` | 723 | 876 | 813.23 | 734.77 | 13 |
| portable | `sh2o_gte_dpct` (C path) | 1560 | 1770 | 1688.38 | 1537.85 | 13 |
| portable | `sh2o_gte_dpct32` | 723 | 876 | 813.23 | 734.77 | 13 |

On the SH-2 build `sh2o_gte_dpct` (and `sh2o_gte_command` op `2Ah`) selects `sh2o_gte_dpct32`; the portable build and the host keep the C path. So the native `sh2o_gte_dpct` row is the kernel plus the entry's call (4 cycles more than the native `sh2o_gte_dpct32` row), and the portable `sh2o_gte_dpct` row is the C path (before the selection the native row was the same C path: 1688.38 cycles mean). The `sh2o_gte_dpct32` rows are the same in both programs: it uses neither `MAC.L` nor `MAC.W`. The `sh2o_gte_dpct` rows equal the rows eDPCT of [`../gte-model/README.md`](../gte-model/README.md). With `SH2O_ZAM=ON` the figures are the same.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
