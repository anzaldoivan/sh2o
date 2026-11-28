# Validation record: 2026-11-21 (host and SH-2 model, k-dot3b-1)

Function `sh2o_k_dot3b` in `sh2o/k.h`. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `k` runs the 92 `dot3b` vectors of `tests/k_oracle.h` (518 for all k functions) through the public entry and the C reference. It finds zero differences. All five k mutant tests (`k_mutant_<name>`, `tests/k_mutant.py`) change the results of this function. With `shift_11`, 77 of 92 vectors differ. The counts for the other mutants are `shift_13` (76), `dropped_sign_extension` (40), `swapped_index` (65) and `lost_carry` (69).
- **Host, bulk:** the test `k` gives the digest `0x61683DE4` over the 113,471 bulk vectors of `tests/suite/sh2o_k_test.h`, which is its pin. The test `k_bulk` makes the bulk vectors again in Python, compares each result line of the C dump, and checks the six pinned digests.
- **SH-2 model, exact:** `make sh2-model`. In the native program and in the portable program, the 92 vectors give zero differences through the public entry and the C reference. These runs use SR.S = 0. Each program also gives the pinned digest `0x61683DE4` over the 113,471 bulk vectors through the public entry.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and reports the first difference. The same comparison checks each vector.
- **SR.S:** the SH-2 code of this function has no `MAC.W` or `MAC.L` instruction (the disassembly of `make cross`). So SR.S does not change its result.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_dot3b` | entry | 56 | 56 | 56.00 | 55.00 | 92 |
| native | `sh2o_k_dot3b` | c | 56 | 56 | 56.00 | 55.00 | 92 |
| portable | `sh2o_k_dot3b` | entry | 56 | 56 | 56.00 | 55.00 | 92 |

- **Not done:** a console run and a run on an emulator with caches and wait states.
