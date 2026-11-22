# gte family model bridge

The model harness calls the library functions `sh2o_gte_write`, `sh2o_gte_command`, `sh2o_gte_read`, `sh2o_gte_nrtps`, `sh2o_gte_nrtpt`, `sh2o_gte_dpct` and `sh2o_gte_mvmva` directly, and the resumable digest `sh2o_gte_test_digest_part` of the gte suite. `bridge.c` has one wrapper, `sh2o_model_gte_mac3(w, v, t, out)`: one out-of-line call of the MVMVA kernel `sh2o_k_mvb3` (`_w2` on the native build, C on the portable build), as in `benchmarks/k-model/bridge.c`. The harness puts one `sh2o_gte_t` register image, the digest state and the wrapper's inputs and outputs in the `sh2o_model_gte_io` buffer (512 bytes).

`make sh2-model` links the bridge, the gte suite (`tests/suite/sh2o_gte_test_suite.c`) and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers: `sh2o_gte_test_digest_part`, `sh2o_gte_write`, `sh2o_gte_command`, `sh2o_gte_read`, the write order `sh2o_gte_order`, `sh2o_gte_nrtps`, `sh2o_gte_nrtpt`, `sh2o_gte_dpct` and `sh2o_model_gte_mac3`. The table keeps these items in the ELF. It also makes the build fail when a signature in `include/sh2o/gte.h` or `tests/suite/sh2o_gte_test.h` changes and the table does not (`include/sh2o/k.h` for the wrapper).

The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of its item. It reads the write order from the ELF.

## Checks

For each ELF, the harness does these steps before it gives a figure:

1. It runs each of the 195 vectors of `tests/gte_oracle.h` (165 `cmd`, 30 `write`), as `tests/test_gte.c` does: it writes the 61 words in `sh2o_gte_order` on a zeroed image, then it does the command (`cmd`) or one more write (`write`), then it reads the 64 registers. All 195 results are exact in each ELF.
2. It computes the digest of the 3,840 suite cases in the ELF, as a chain of 15 calls of `sh2o_gte_test_digest_part` with 256 cases each. Each call also gives the known answers of its cases. Each ELF gives the pin `0xA50D51B1`, and the 14 known answers hold. The most instructions of one call are 2,403,127 (native) and 2,542,903 (portable). The limit of the model harness is 100,000,000.
3. It runs each suite case again with `SR.S` = 1, from the state that the `SR.S` = 0 chain has before that case, and each `cmd` vector again with `SR.S` = 1. The library checks `SR.S`: any difference fails the run. Each ELF gives 0 of 3,840 cases and 0 of 160 vectors.
4. For each RTPS and RTPT `cmd` vector (7 and 6), it loads the vector's input image and calls the fixed-word entry `sh2o_gte_nrtps` or `sh2o_gte_nrtpt`, then loads the image again and calls `sh2o_gte_command` with the entry's word (`0x4A180001`, `0x4A280030`). The 64 registers must be equal, with `SR.S` = 0 and with `SR.S` = 1. Where the vector's own word has sf 1 and lm 0 (1 RTPS and 1 RTPT vector), the 64 registers must also equal the vector's result. Any difference fails the run. Each ELF gives 0 differences.
5. For each DPCT `cmd` vector (13), it loads the vector's input image and calls `sh2o_gte_dpct` with the vector's word, then loads the image again and calls `sh2o_gte_command` with the same word. The 64 registers must be equal, and equal to the vector's result, with `SR.S` = 0 and with `SR.S` = 1. Each ELF gives 0 of 13 differences.
6. The MVMVA kernel path, with `SR.S` = 0: each `cmd` vector with opcode `12h`, sf 1, mx < 3, cv != 2 and each T word in the range of the kernel path of `sh2o_gte_mvmva` (`src/sh2o_gte.c`, `sh2oi_mac3`): 21 of the 71 MVMVA vectors. On the vector's input image, one `sh2o_gte_mvmva` call (whole); the 64 registers must equal the vector's result. Then one `sh2o_model_gte_mac3` call (kernel) on the same matrix words (the image from register 32, 40 or 48, by mx), V and T. Each `out[i]` must equal `(int64)T[i] << 12 + M[i].V`, which the harness computes from the image words, and `(int32)(out[i] >> 12)` must equal the vector's MAC1-3. Any difference fails the run. Each ELF gives 0 differences.

The target also runs the harness with `--plant 0` (one bit of one expected result changes) with `--plant-digest` (the harness compares the digest with the pin XOR 1) and with `--plant-entry` (the harness compares each entry with `sh2o_gte_command` and a wrong word: sf 0, `0x4A100001`, for `sh2o_gte_nrtps`; lm 1, `0x4A280430`, for `sh2o_gte_nrtpt`; the vector's word with sf flipped for `sh2o_gte_dpct`: 13 of 13 differ). Each time, the harness must stop with a failure. Step 6 has no plant switch. A copy of the harness with one expected bit changed (the whole result, the kernel's int64 or the MAC word) stopped with exit 1 each time (2026-11-22).

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `b66a8ab50c25edac07bd2d23a8cebfd64b7394ad3720dcbbe0a8d9b206ae806f`, portable `ae5f10073f7d5fa9dc29dd99d93755e0abd65144713edef44dd23c81eebb769f` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `bce3ad31…`, portable `34d9588d…`; 2026-11-22: the prepared MVMVA source `src/sh2o_gte_prepared.c` joined the library and changed the hashes; the 5 new MVMVA vectors of the oracle changed the `12h` rows and the step 6 rows, the other figures did not change; before it: native `67225f9a…`, portable `3e62ab57…`; 2026-11-22: the wrapper `sh2o_model_gte_mac3` joined the bridge and its table and changed the hashes; the step 6 rows are new, the other figures did not change; before it: native `01107c86…`, portable `ac23d441…`; 2026-11-22: the native `sh2o_gte_dpct` selects the 32-bit DPCT kernel, which changed the native hash and the native DPCT and eDPCT figures; before it: native `50135e43…`; 2026-11-22: the 32-bit DPCT kernel `src/sh2o_gte_dpct32.c` joined the library and changed the hashes, not the figures; before it: native `ad5d4335…`, portable `2c82a705…`; the programs link every library object; the hashes changed when DPCT (`sh2o_gte_dpct`) joined the library and the bridge table: the DPCT rows are new, the other figures did not change; before DPCT: native `71f8f63f…`, portable `08e7fdbb…`; the hashes changed when the bridge table gained `sh2o_gte_nrtps` and `sh2o_gte_nrtpt`, the figures did not; an earlier value of this line: native `59994951…`, portable `41b0aeb3…`; before the SR.S-checking MAC.W kernels (`_w2`), when the figures changed too: native `19e5b667…`, portable `7bf74101…`; the hashes changed when the PSY-Q sources were added, again when the list source `src/sh2o_list.c` was added and again when the delegate source `src/sh2o_zam.c` was added (with the option `SH2O_ZAM` off it compiles to no symbol; only its file name joins the symbol table), the figures did not; before the delegate source: native `23e5458c…`, portable `e5e265f9…`; before the list source: native `88d8ec9f…`, portable `24658540…`; before the PSY-Q sources: native `d44e4d29…`, portable `9e32dfd4…`). The figures are for one call of `sh2o_gte_command`, from its entry to its return, over the `cmd` vectors of `tests/gte_oracle.h`. They do not include the register writes and reads. The rows nRTPS and nRTPT are for one call of `sh2o_gte_nrtps` or `sh2o_gte_nrtpt` over the same RTPS or RTPT vectors' input images, with the entry's fixed word in place of the vector's word. The row eDPCT is for one call of `sh2o_gte_dpct` with the vector's word, over the DPCT vectors. Opcode `0Ch` is not modelled: the call returns -1 and does not change the image.

| Program | Opcode | Command | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `01h` | RTPS | 659 | 852 | 704.00 | 618.00 | 7 |
| native | `01h` | nRTPS | 657 | 849 | 700.14 | 614.43 | 7 |
| native | `06h` | NCLIP | 161 | 165 | 164.43 | 153.14 | 7 |
| native | `0Ch` | (not modelled) | 12 | 12 | 12.00 | 9.00 | 1 |
| native | `10h` | DPCS | 588 | 618 | 599.33 | 546.83 | 6 |
| native | `12h` | MVMVA | 389 | 644 | 496.28 | 443.35 | 71 |
| native | `13h` | NCDS | 1284 | 1569 | 1336.17 | 1199.17 | 6 |
| native | `16h` | NCDT | 3744 | 4641 | 3958.00 | 3567.00 | 6 |
| native | `1Bh` | NCCS | 921 | 1205 | 981.50 | 883.33 | 6 |
| native | `1Eh` | NCS | 704 | 973 | 756.33 | 679.67 | 6 |
| native | `20h` | NCT | 2064 | 2862 | 2215.00 | 1972.50 | 6 |
| native | `28h` | SQR | 194 | 205 | 198.40 | 181.60 | 5 |
| native | `2Ah` | DPCT | 743 | 896 | 833.23 | 751.77 | 13 |
| native | `2Ah` | eDPCT | 727 | 880 | 817.23 | 737.77 | 13 |
| native | `2Dh` | AVSZ3 | 71 | 81 | 78.57 | 69.86 | 7 |
| native | `30h` | RTPT | 1803 | 2334 | 1935.00 | 1693.50 | 6 |
| native | `30h` | nRTPT | 1873 | 2383 | 1958.00 | 1720.50 | 6 |
| native | `3Dh` | GPF | 212 | 240 | 224.40 | 207.80 | 5 |
| native | `3Eh` | GPL | 291 | 408 | 339.71 | 308.29 | 7 |
| portable | `01h` | RTPS | 735 | 850 | 768.86 | 695.86 | 7 |
| portable | `01h` | nRTPS | 733 | 847 | 765.00 | 692.29 | 7 |
| portable | `12h` | MVMVA | 465 | 642 | 537.92 | 493.28 | 71 |
| portable | `13h` | NCDS | 1436 | 1643 | 1475.17 | 1365.83 | 6 |
| portable | `16h` | NCDT | 4200 | 4863 | 4375.00 | 4067.00 | 6 |
| portable | `1Bh` | NCCS | 1073 | 1279 | 1120.50 | 1050.00 | 6 |
| portable | `1Eh` | NCS | 856 | 1047 | 895.33 | 846.33 | 6 |
| portable | `20h` | NCT | 2520 | 3084 | 2632.00 | 2472.50 | 6 |
| portable | `2Ah` | DPCT | 1576 | 1786 | 1704.38 | 1551.85 | 13 |
| portable | `2Ah` | eDPCT | 1560 | 1770 | 1688.38 | 1537.85 | 13 |
| portable | `30h` | RTPT | 2031 | 2328 | 2124.00 | 1920.50 | 6 |
| portable | `30h` | nRTPT | 2101 | 2377 | 2147.00 | 1947.50 | 6 |

The other portable rows (`06h`, `0Ch`, `10h`, `28h`, `2Dh`, `3Dh`, `3Eh`) are the same as the native rows. On the native build `sh2o_gte_dpct` calls the 32-bit kernel `sh2o_gte_dpct32` (tag `gte-dpct32-1`), so the native DPCT and eDPCT rows are the kernel's (before the selection: 1704.38 and 1688.38 cycles mean, the portable rows); the portable build keeps the C path.

The suite digest over the 3,840 cases takes 26,108,665 instructions and 30,957,877 cycles in the native ELF, and 26,648,386 instructions and 31,408,633 cycles in the portable ELF. These figures include the case set generator and the hash.

### MVMVA: whole call and kernel (step 6)

Over the 21 kernel-path vectors of step 6, with `SR.S` = 0. Whole: one `sh2o_gte_mvmva` call. Kernel: one `sh2o_model_gte_mac3` call on the same inputs. The kernel figure includes the call of the wrapper, thus the overhead is slightly low. Overhead: the whole mean minus the kernel mean, and its part of the whole mean.

| Program | Call | Cycles min | Cycles max | Cycles mean | Inputs |
|---|---|---|---|---|---|
| native | whole | 381 | 416 | 400.29 | 21 |
| native | kernel | 133 | 133 | 133.00 | 21 |
| portable | whole | 457 | 492 | 476.29 | 21 |
| portable | kernel | 210 | 210 | 210.00 | 21 |

Overhead: 267.29 cycles, 66.8% of the whole (native); 266.29 cycles, 55.9% of the whole (portable). Before the 5 new MVMVA vectors (19 inputs): whole 399.58 and 475.58, overhead 266.58 and 265.58. The whole mean of step 6 (400.29) is less than the mean of the `12h` row (496.28), because the row also has the vectors that are not on the kernel path.

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
