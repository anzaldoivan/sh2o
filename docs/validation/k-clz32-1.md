# Validation record: 2026-11-21 (host and SH-2 model, k-clz32-1)

Function `sh2o_k_clz32` in `sh2o/k.h`. [VALIDATION.md](../VALIDATION.md) lists the other records.

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test` passes with GCC and with Clang. The test `k` runs the 130 `clz32` vectors of `tests/k_oracle.h` (518 for all k functions) through the public entry and the C reference. It finds zero differences. No k mutant of `tests/mutant.py` changes the results of this function.
- **Host, UNR proofs:** The recip check is `tests/unr_check.py recip` (ctest `gte_unr`, in `make test`). It compares the 65,535 rows of `gte --unr` (SZ 1..FFFFh) with the reciprocal of the Python model and finds 0 differences. It also compares the `unr[0x101]` table with `UNR` of `scripts/gte_model.py`. Each row shows z = `sh2o_k_clz32(sz)` - 16. The sweep is ctest `gte_unr_sweep` (label `proof`, `make proof-tests`). It compares `sh2o_k_unr_div(h, sz)` with (((h << z) * recip + 8000h) >> 16), clamped to 1FFFFh. It does this for all 3,221,192,704 pairs (SZ 1..FFFFh, h 0..min(2 SZ - 1, FFFFh)) and finds 0 differences. The sweep takes 5.1 s (GCC) and 4.4 s (Clang) in a Release build. Command: `make proof-tests`. The mutant `clz32_bound_off_by_one` (`v <= 0x1000000u` in place of `v < 0x1000000u`) gives no oracle vector difference. The recip check catches it in 0.1 to 0.2 s. The program stops with a fault at SZ 100h, because the wrong z puts the divisor out of the table range.
- **SH-2 model, exact:** `make sh2-model`. In the native program and in the portable program, the 130 vectors give zero differences through the public entry and the C reference. These runs use SR.S = 0.
- **Planted difference:** the harness changes one bit of the expected result of vector 0 (a `dot3b` vector). The run stops with exit 1 and reports the first difference. The same comparison checks each vector.
- **SR.S:** the SH-2 code of this function has no `MAC.W` or `MAC.L` instruction (the disassembly of `make cross`). So SR.S does not change its result.
- **MODELLED cycles:** one call of the bridge wrapper, call included. Source: [benchmarks/k-model/README.md](../../benchmarks/k-model/README.md), with the program sha256 values. These are not Saturn timings.

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_clz32` | entry | 26 | 28 | 27.01 | 23.55 | 130 |
| portable | `sh2o_k_clz32` | entry | 26 | 28 | 27.01 | 23.55 | 130 |

- **Not done:** a console run and a run on an emulator with caches and wait states.
