# list family model bridge

The model harness calls `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_mvmva` and `sh2o_gte_mvmva_list` directly. It also calls `sh2o_list_test_counts` of the list suite. `bridge.c` has one wrapper, `sh2o_model_list_mac3(w, v, t, out)`: one out-of-line call of the MVMVA kernel `sh2o_k_mvb3` (`_w2` on the native build, C on the portable build), as in `benchmarks/k-model/bridge.c`. The harness puts its inputs and outputs in the scratch bytes of the buffer. The `sh2o_model_list_io` buffer holds one `sh2o_gte_t` image, the 16 items of a list, the 16 out rows and the 5 suite counts. The bridge also holds the write order of `tests/test_list.c` (`sh2o_model_list_order`), because the gte suite is not in the ELF.

`sh2o_gte_mvmva_list` has five arguments. The SH-2 calling convention puts the fifth argument (`out`) on the stack. The harness writes it at the address in R15 at the start of the call. Thus the timed call is the list call itself.

`make sh2-model` links the bridge, the list suite (`tests/suite/sh2o_list_test_suite.c`) and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers: `sh2o_gte_write`, `sh2o_gte_read`, `sh2o_gte_mvmva`, `sh2o_gte_mvmva_list`, `sh2o_list_test_counts`, the write order and `sh2o_model_list_mac3`. The table keeps these functions in the ELF. It also makes the build fail when a signature in `include/sh2o/gte.h`, `include/sh2o/list.h` or `tests/suite/sh2o_list_test.h` changes and the table does not.

The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of its function.

## Checks

For each ELF, the harness does these steps before it gives a figure:

1. It runs the 88 vectors of `tests/list_oracle.h`, as `tests/test_list.c` does: the 64 register words through `sh2o_gte_write` in the write order on a zeroed image, then one `sh2o_gte_mvmva_list` call, then the 16 out rows and the 64 words of `sh2o_gte_read`. All result bytes are equal to the oracle in each ELF.
2. For each vector, it runs the singles from the same image: it stores each item in VXY0 and VZ0, as `src/sh2o_list.c` does, and calls `sh2o_gte_mvmva` one time. The out rows and the 64 words that the singles give are equal to the result of the vector in each ELF.
3. It computes the suite counts in the ELF with one `sh2o_list_test_counts` call: 256 runs, 2,144 items, 0 prefixes that differ, 238 of 238 planted reorders caught. These are the pins of `tests/suite/sh2o_list_test.h` in each ELF. The call takes 18,530,470 instructions and 21,526,999 cycles (native), and 19,301,513 instructions and 22,170,947 cycles (portable). The limit of the model harness is 100,000,000 instructions, thus one call is sufficient.
4. It runs the 88 vectors again with `SR.S` = 1. The library checks `SR.S`: any difference fails the run. Each ELF gives 0 of 88.
5. The MVMVA kernel path, with `SR.S` = 0: each vector with opcode `12h`, sf 1, mx < 3, cv != 2 and each T word in the range of the kernel path of `sh2o_gte_mvmva` (`src/sh2o_gte.c`, `sh2oi_mac3`): 173 items. For each item, as in step 2: one `sh2o_gte_mvmva` call (whole), whose out row must equal the vector's (and after the last item, the 64 words of `sh2o_gte_read`). Then one `sh2o_model_list_mac3` call (kernel) on the same matrix words (the image from register 32, 40 or 48, by mx), V and T. Each `out[i]` must equal `(int64)T[i] << 12 + M[i].V`, which the harness computes from the image words, and `(int32)(out[i] >> 12)` must equal the item's MAC1-3. Any difference fails the run. Each ELF gives 0 differences.

The target also runs the harness with `--plant 0` (bit 0 of the first expected result byte of vector 0 changes) and with `--plant-count` (the harness compares the counts with the pins where the item count is XOR 1). Each time, the harness must stop with exit code 1. Step 5 has no plant switch. A copy of the harness with one expected bit changed (the whole out row, the kernel's int64 or the MAC word) stopped with exit 1 each time (2026-11-22).

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `736c36cbe58788cd9df3fd4efc119b3db9018c0da8fe3d4b77e6da38fad9f255`, portable `337aa9a834e947a1941b528ee2633ca04a45c96668e6ab349fd3d9f91d093891` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `2e600e2d…`, portable `fe365682…`; 2026-11-22: the prepared MVMVA source `src/sh2o_gte_prepared.c` joined the library and changed the hashes, not the figures; before it: native `c784ae24…`, portable `acb1c908…`; 2026-11-22: the wrapper `sh2o_model_list_mac3` joined the bridge and its table and changed the hashes; the step 5 rows are new, the other figures did not change; before it: native `903d8258…`, portable `57569bf6…`; 2026-11-22: the native `sh2o_gte_dpct` selects the 32-bit DPCT kernel, which changed the native hash, not the figures; before it: native `b1beb333…`; 2026-11-22: the 32-bit DPCT kernel `src/sh2o_gte_dpct32.c` joined the library and changed the hashes, not the figures; before it: native `997524c1…`, portable `2f0e8454…`; 2026-11-22: DPCT added to `src/sh2o_gte.c` changed the hashes, not the figures; before it: native `37772464…`, portable `a6ae56bd…`; before the SR.S-checking MAC.W kernels (`_w2`), when the figures changed too: native `9cd5a8a5…`, portable `08294765…`).

One list item is the cycles (or instructions) of one `sh2o_gte_mvmva_list` call, from its entry to its return, divided by its n. The minimum, maximum and mean are over the 84 oracle vectors with n of 1 or more (4 vectors have n = 0). One single is one `sh2o_gte_mvmva` call, over the 656 items of the singles of the 88 vectors.

| Program | Call | Cycles min | Cycles max | Cycles mean | Instructions min | Instructions max | Instructions mean | Count |
|---|---|---|---|---|---|---|---|---|
| native | list item | 398.40 | 663.00 | 519.30 | 351.20 | 610.00 | 465.44 | 84 |
| native | `sh2o_gte_mvmva` | 354 | 633 | 482.11 | 309 | 578 | 430.49 | 656 |
| portable | list item | 474.40 | 663.00 | 559.02 | 442.20 | 610.00 | 513.06 | 84 |
| portable | `sh2o_gte_mvmva` | 430 | 631 | 522.35 | 400 | 578 | 478.72 | 656 |

The list item figures include the call of the list and its loop. The single figures do not include the stores of VXY0 and VZ0 or the reads of the out rows.

### MVMVA: whole call and kernel (step 5)

Over the 173 kernel-path items of step 5, with `SR.S` = 0. Whole: one `sh2o_gte_mvmva` call. Kernel: one `sh2o_model_list_mac3` call on the same inputs. The kernel figure includes the call of the wrapper, thus the overhead is slightly low. Overhead: the whole mean minus the kernel mean, and its part of the whole mean.

| Program | Call | Cycles min | Cycles max | Cycles mean | Inputs |
|---|---|---|---|---|---|
| native | whole | 381 | 407 | 396.08 | 173 |
| native | kernel | 133 | 133 | 133.00 | 173 |
| portable | whole | 457 | 483 | 472.08 | 173 |
| portable | kernel | 210 | 210 | 210.00 | 173 |

Overhead: 263.08 cycles, 66.4% of the whole (native); 262.08 cycles, 55.5% of the whole (portable).

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
