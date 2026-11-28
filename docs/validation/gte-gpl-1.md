# Validation record: 2026-11-21 (host and SH-2 model, gte-gpl-1)

Function `sh2o_gte_gpl` in `sh2o/gte.h`, opcode `3Eh` (GPL). The dispatch `sh2o_gte_command` (record [gte-command-1](gte-command-1.md)) calls it. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `gte` runs the 7 `cmd` vectors of opcode `3Eh` (GPL) of `tests/gte_oracle.h` through `sh2o_gte_write`, `sh2o_gte_command` and `sh2o_gte_read`, with zero differences. That file has 177 vectors for all GTE functions. The suite gives the digest `0xA50D51B1` over 3,840 cases, which is its pin. Of these cases, 256 are GPL cases. The test `gte_model` makes the case set again in Python. The gte mutants that change results of the 7 vectors of this opcode: `dropped_colour_push` (5), `shift_13` (1). Source: each mutant program (`--driver`) against the test program on these vectors.
- **Host, other sources:** the test `gte_sources` runs 72,397 cases of all the commands through `tests/test_gte.c --driver` and the Python model. The sources of the cases: edges 13,553, LCG 57,344, ps1-tests 1,500. The result: 0 differences on all 64 registers, FLAG included. The output shows the total only, not a count for each command. The test `gte_recordings` (private recordings, 75,261 cases) gave 0 differences in the record "GTE against the model" of [VALIDATION.md](../VALIDATION.md). It did not run again for this record.
- **SH-2 cross build:** `make cross`. `tests/sh2/gte_static.txt` (native, at `-Os` and `-O2`): no undefined symbol, and 9 `MAC.W`, all through `sh2o_k_mvb3`. The same spec: no `SETS`, no GBR, no SR write, no divide step, no constant in the SH-2 divider area. `tests/sh2/gte_switch_static.txt` (the test build with `SH2O_MVB3_SWITCH=1`): 15 `MAC.W`, the same forbidden instructions. `tests/sh2/gte_portable_static.txt` (`SH2O_FORCE_PORTABLE=1`): no `MAC.W`. `tests/sh2/gte_suite_static.txt`: the suite object needs only `sh2o_gte_read`, `sh2o_gte_write` and `sh2o_gte_command`. The check finds the planted count of `gte` and `gte_switch`.
- **SH-2 model, exact:** `make sh2-model`. In the native and the portable program, the 7 `cmd` vectors of opcode `3Eh` have zero differences through `sh2o_gte_command`, with SR.S = 0. In each program, 177 of 177 vectors are exact. Each program gives the pinned digest `0xA50D51B1` over the 3,840 suite cases. The known answer of one GPL case holds (14 of 14 known answers hold). Program sha256: native `d44e4d29…`, portable `9e32dfd4…`.
- **SR.S:** not affected. GPL does not use `MAC.W` in the native program: the 9 `MAC.W` of the native build are all in `sh2o_k_mvb3`, which GPL does not call. The harness counts for SR.S = 1 (493 of 3,840 suite cases, 5 of 147 `cmd` vectors) are totals for all the commands.
- **2026-11-22:** with SR.S = 1 the count is now 0. The library checks SR.S: record [k-mvb3-macw-2](k-mvb3-macw-2.md).
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (an RTPS `cmd` vector). The run stops with exit 1 and gives the first difference. With `--plant-digest` the harness compares the digest with the pin XOR 1, and the run stops with exit 1.
- **MODELLED cycles:** one call of `sh2o_gte_command`, from its entry to its return, without the register writes and reads. Source: [benchmarks/gte-model/README.md](../../benchmarks/gte-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Opcode | Command | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `3Eh` | GPL | 291 | 408 | 339.71 | 308.29 | 7 |

The portable row is the same as the native row.

- **Not done:** a console run, and a run on an emulator with caches and wait states.
