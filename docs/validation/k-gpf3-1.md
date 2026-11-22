# Validation record: 2026-11-21 (host and SH-2 model, k-gpf3-1)

Function `sh2o_k_gpf3` in `sh2o/k.h`. The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `k` gives the 20 `gpf3` vectors of `tests/k_oracle.h` (518 for all k functions) through the public entry and the C reference, with zero differences. No k mutant test (`tests/k_mutant.py`) changes the results of this function.
- **Host, bulk:** the test `k` gives the digest `0x61BC75C3` over the 113,471 bulk vectors of `tests/suite/sh2o_k_test.h`, which is its pin. The test `k_bulk` makes the bulk vectors again in Python, compares each result line of the C dump, and checks the six pinned digests. The digest over the first 15,625 vectors is `0x7C2BE3B8`, which is its pin.
- **SH-2 model, exact:** `make sh2-model`. In the native program and in the portable program, the 20 vectors give zero differences through the public entry and the C reference, with SR.S = 0. Each program also gives the pinned digest `0x61BC75C3` over the 113,471 bulk vectors through the public entry, the C reference and the `MAC.W` kernel (native), the public entry and the C reference (portable). Through the public entry, each program gives the pinned digest `0x7C2BE3B8` over the first 15,625 vectors (tr = INT32_MIN).
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and gives the first difference. The same comparison checks each vector.
- **SR.S:** the SH-2 code of this function has no `MAC.W` or `MAC.L` instruction (the disassembly of `make cross`), so SR.S does not change its result; `make cross` counts zero `MAC.W` in its C reference.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_gpf3` | entry | 40 | 40 | 40.00 | 37.00 | 20 |
| portable | `sh2o_k_gpf3` | entry | 40 | 40 | 40.00 | 37.00 | 20 |

- **Not done:** a console run; a run on an emulator with caches and wait states.
