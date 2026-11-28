# Validation record: 2026-11-21 (host and SH-2 model, k-mvb3-1)

Function `sh2o_k_mvb3` in `sh2o/k.h`. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `k` runs the 30 `mvb3` vectors of `tests/k_oracle.h` (518 for all k functions) through the public entry and the C reference. It finds zero differences. The k mutant tests (`k_mutant_<name>`, `tests/k_mutant.py`) that change the results of this function: `lost_carry` (29 of 30 vectors differ).
- **Host, bulk:** the test `k` gives the digest `0x9D49BB58` over the 113,471 bulk vectors of `tests/suite/sh2o_k_test.h`, which is its pin. The test `k_bulk` makes the bulk vectors again in Python, compares each result line of the C dump, and checks the six pinned digests.
- **SH-2 model, exact:** `make sh2-model`. In the native program and in the portable program, the 30 vectors give zero differences through the public entry and the C reference. These runs use SR.S = 0. Each program also gives the pinned digest `0x9D49BB58` over the 113,471 bulk vectors. The native program gives it through the public entry, the C reference and the `MAC.W` kernel. The portable program gives it through the public entry and the C reference.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and reports the first difference. The same comparison checks each vector.
- **SR.S:** in the native program the public entry is the `MAC.W` kernel `sh2o_k_mvb3_w` (record [k-mvb3-macw-1](k-mvb3-macw-1.md)). It assumes SR.S = 0 and does not check it. The harness did no run with SR.S = 1 through the entry. The portable program has no `MAC.W` (`make cross`, `tests/sh2/k_portable_static.txt`).
- **2026-11-22:** the native entry is now `sh2o_k_mvb3_w2`, which checks SR.S. With SR.S = 1 the entry gives 0 of 30 differences: record [k-mvb3-macw-2](k-mvb3-macw-2.md).
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_mvb3` | entry | 114 | 114 | 114.00 | 95.00 | 30 |
| native | `sh2o_k_mvb3` | c | 210 | 210 | 210.00 | 204.00 | 30 |
| portable | `sh2o_k_mvb3` | entry | 210 | 210 | 210.00 | 204.00 | 30 |

- **Not done:** a console run, a run on an emulator with caches and wait states, and a check of SR.S in the native path. With SR.S = 1, the `MAC.W` kernel of the native path gives different results.
