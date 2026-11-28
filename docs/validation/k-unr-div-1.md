# Validation record: 2026-11-21 (host and SH-2 model, k-unr-div-1)

Function `sh2o_k_unr_div` in `sh2o/k.h`. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `k` runs the 78 `unr_div` vectors of `tests/k_oracle.h` (518 for all k functions) through the public entry and the C reference. It finds zero differences. No k mutant of `tests/mutant.py` changes the results of this function.
- **Host, UNR proofs:** The sweep is ctest `gte_unr_sweep` (label `proof`, `make proof-tests`). It compares `sh2o_k_unr_div(h, sz)` with (((h << z) * recip + 8000h) >> 16), clamped to 1FFFFh. It does this for all 3,221,192,704 pairs (SZ 1..FFFFh, h 0..min(2 SZ - 1, FFFFh)) and finds 0 differences. The sweep takes 5.1 s (GCC) and 4.4 s (Clang) in a Release build. Command: `make proof-tests`. The mutant `unr_div_clamp_bound` (clamp at FFFFh in place of 1FFFFh) gives 1 oracle vector difference, in less than 0.1 s. The sweep also catches it: it stops at the second pair (h 1, SZ 1).
- **SH-2 model, exact:** `make sh2-model`. In the native program and in the portable program, the 78 vectors give zero differences through the public entry and the C reference. These runs use SR.S = 0.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and reports the first difference. The same comparison checks each vector.
- **SR.S:** the SH-2 code of this function has no `MAC.W` or `MAC.L` instruction (the disassembly of `make cross`). So SR.S does not change its result.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_unr_div` | entry | 77 | 78 | 77.15 | 66.08 | 78 |
| portable | `sh2o_k_unr_div` | entry | 77 | 78 | 77.15 | 66.08 | 78 |

- **Not done:** a console run and a run on an emulator with caches and wait states.
