# Validation record: 2026-11-22 (host and SH-2 model, gte-nrtps-1)

Function `sh2o_gte_nrtps` in `sh2o/gte.h`: the RTPS command with the fixed word `0x4A180001` (sf 1, lm 0), as the nRTPS macro of the PSY-Q macro family. It calls `sh2o_gte_rtps` (record [gte-rtps-1](gte-rtps-1.md)) with that word. The macro's two NOPs wait for the load delay of the PS1 GTE; sh2o's image has none, so the entry is the command alone ([GTE.md](../GTE.md#the-fixed-word-entries)). The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compiler: GCC 16.2.1.
- **Host, exact:** `make test` passes. The test `gte` runs `sh2o_gte_nrtps` on the input image of each of the 7 `cmd` vectors of opcode `01h` (RTPS) of `tests/gte_oracle.h` and compares the 64 registers with `sh2o_gte_command` and the word `0x4A180001` on the same image: 0 differences (output: `gte: entries: nrtps 7 vectors (1 model), nrtpt 6 vectors (1 model), 0 differences`). For the 1 vector whose word has sf 1 and lm 0, the registers also equal the vector's result (the model's answer). The test `gte_model` gives the same vector's result through the Python model with the fixed word (`gte_model: entries: 1 RTPS and 1 RTPT vectors equal with the fixed words`). The test `gte_sources` runs the entries through `tests/test_gte.c --driver` and the model: `gte_sources: fixed words: 13 cases (RTPS 7, RTPT 6), 0 differences`.
- **Mutant:** `gte_entry_mutant_nrtps_sf0` changes the word in `src/sh2o_gte.c` to `0x4A100001` (sf 0). The test `gte` finds it: `gte: nrtps_sf0: entry nrtps differs in vector 0 (cmd 0x00000001), register 9: 0xFFFF8000, expected 0x00007FFF`.
- **SH-2 cross build:** `make cross`, green; the manifest `tests/sh2/objects.sha256` matches. The entry is in `src/sh2o_gte.c`, so the specs of the gte object hold for it (record [gte-rtps-1](gte-rtps-1.md)).
- **SH-2 model, exact:** `make sh2-model`. In the native and the portable program, for each of the 7 RTPS `cmd` vectors, the harness calls `sh2o_gte_nrtps` on the vector's input image and `sh2o_gte_command` with `0x4A180001` on the same image: the 64 registers are equal (0 of 7 differ), and equal to the vector's result for the 1 vector with sf 1 and lm 0. The bridge table holds the entry; program sha256: native `71f8f63f…`, portable `08e7fdbb…`.
- **SR.S:** the same comparison with SR.S = 1: 0 of 7 differ in each program, and the 1 vector result holds.
- **Planted word:** with `--plant-entry` the harness compares the entries with `sh2o_gte_command` and a wrong word (sf 0, `0x4A100001`, for nRTPS; lm 1, `0x4A280430`, for nRTPT). In the native program the planted word differs from the entry in 7 of 7 vectors at SR.S 0 and at SR.S 1. The run stops with exit 1: `gte: native: SR.S 0: nRTPS differs from the command 4a100001 in vector 0 (word 00000001), register 9: command ffff8000, entry 00007fff`.
- **MODELLED cycles:** one call of `sh2o_gte_nrtps`, from its entry to its return, over the input images of the 7 RTPS vectors, without the register writes and reads. The RTPS rows are one call of `sh2o_gte_command` with each vector's own word. Source: [benchmarks/gte-model/README.md](../../benchmarks/gte-model/README.md). These are not Saturn timings.

| Program | Opcode | Call | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `01h` | RTPS (`sh2o_gte_command`) | 659 | 852 | 704.00 | 618.00 | 7 |
| native | `01h` | nRTPS (`sh2o_gte_nrtps`) | 657 | 849 | 700.14 | 614.43 | 7 |
| portable | `01h` | RTPS (`sh2o_gte_command`) | 735 | 850 | 768.86 | 695.86 | 7 |
| portable | `01h` | nRTPS (`sh2o_gte_nrtps`) | 733 | 847 | 765.00 | 692.29 | 7 |

- **Not done:** a console run; a run on an emulator with caches and wait states; a game frame through the entry.
