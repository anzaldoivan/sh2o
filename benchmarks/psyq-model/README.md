# psyq family model bridge

`bridge.c` has one helper, `sh2o_model_psyq_args`. The model harness calls the 19 PSY-Q bodies (`sh2o_psyq_<name>`) directly. It also calls the resumable digest `sh2o_psyq_test_digest_part` of the psyq suite and the suite's case functions `sh2o_psyq_test_case_draw` and `sh2o_psyq_test_case_hash`. The `sh2o_model_psyq_io` buffer holds the digest state, the four argument words of a body and one drawn case (`sh2o_psyq_test_case_t`).

`sh2o_model_psyq_args` gives the four argument words of the body of a case, as the suite gives them: the register image, then the address of each region, then the integer arguments.

`make sh2-model` links the bridge, the psyq suite (`tests/suite/sh2o_psyq_test_suite.c`) and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed pointers: `sh2o_psyq_test_digest_part`, `sh2o_psyq_test_case_draw`, `sh2o_psyq_test_case_hash`, `sh2o_model_psyq_args` and the 19 bodies in the case order of the suite. The table keeps these functions in the ELF. It also makes the build fail when a signature in `include/sh2o/psyq.h` or `tests/suite/sh2o_psyq_test.h` changes and the table does not.

The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of its function.

## Checks

The psyq family has no oracle file. The checks are the digest and the known answers of the suite. For each ELF, the harness does these steps before it gives a figure:

1. It computes the digest of the 1,216 suite cases in the ELF, as a chain of 5 calls of `sh2o_psyq_test_digest_part` with 256 cases or fewer each. The last call also runs the 6 `ratan2` known answers. Each ELF gives the pin `0xE430E70B`, and the 6 known answers hold. The most instructions of one call are 2,208,285 (native) and 2,214,109 (portable). The limit of the model harness is 100,000,000.
2. It runs each case again and calls the body directly. The ELF draws the case and gives the argument words. The harness calls the body, changes its result to the return word of the suite, and the ELF adds the case to the hash. This chain also gives the pin in each ELF. Thus the timed calls are the cases of the suite.
3. It runs each suite case again with `SR.S` = 1, from the state that the `SR.S` = 0 chain has before that case. The library checks `SR.S`: any difference fails the run. Each ELF gives 0 of 1,216.

The target also runs the harness with `--plant-known 1` (bit 0 of the expected value of known answer 1 changes in the RAM of the ELF) and with `--plant-digest` (the harness compares the digest with the pin XOR 1). Each time, the harness must stop with exit code 1.

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `6359e424fae26e02d5ef6deb777201e457cf9144b88b5296c4738a0d60a72697`, portable `b549c8f5d96b6f1085560bebe73777fe61b4e295a887b20cbdba6973a455a191` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `f65a9435…`, portable `cb48997f…`; 2026-11-22: the prepared MVMVA source `src/sh2o_gte_prepared.c` joined the library and changed the hashes, not the figures; before it: native `bc40dc0c…`, portable `07e4457c…`; 2026-11-22: the native `sh2o_gte_dpct` selects the 32-bit DPCT kernel, which changed the native hash, not the figures; before it: native `86535166…`; 2026-11-22: the 32-bit DPCT kernel `src/sh2o_gte_dpct32.c` joined the library and changed the hashes, not the figures; before it: native `7fc4b52b…`, portable `55292bf7…`; 2026-11-22: DPCT added to `src/sh2o_gte.c` changed the hashes, not the figures; before it: native `ce73d538…`, portable `86119f60…`; the programs link every library object; before the SR.S-checking MAC.W kernels (`_w2`), when the figures changed too: native `0fe2eaeb…`, portable `f3018762…`; the hashes changed when the list source `src/sh2o_list.c` was added and again when the delegate source `src/sh2o_zam.c` was added (with the option `SH2O_ZAM` off it compiles to no symbol; only its file name joins the symbol table), the figures did not; before the delegate source: native `70f26222…`, portable `33c04808…`; before the list source: native `57620478…`, portable `b88d9745…`). The figures are for one call of each body, from its entry to its return, over the 64 suite cases of that body. The mean is the mean over these 64 cases.

| Body | Native cycles min | Native cycles max | Native cycles mean | Native instructions mean | Portable cycles min | Portable cycles max | Portable cycles mean | Portable instructions mean |
|---|---|---|---|---|---|---|---|---|
| `ApplyMatrix` | 475 | 487 | 483.81 | 439.81 | 551 | 563 | 559.81 | 530.81 |
| `ApplyMatrixLV` | 1342 | 1370 | 1356.41 | 1184.66 | 1494 | 1522 | 1508.41 | 1366.66 |
| `CompMatrix` | 2420 | 2471 | 2427.88 | 2137.38 | 2724 | 2775 | 2731.88 | 2501.38 |
| `InitGeom` | 169 | 169 | 169.00 | 134.00 | 169 | 169 | 169.00 | 134.00 |
| `PopMatrix` | 23 | 260 | 256.30 | 208.98 | 23 | 260 | 256.30 | 208.98 |
| `PushMatrix` | 28 | 323 | 189.33 | 161.17 | 28 | 323 | 189.33 | 161.17 |
| `RotTrans` | 609 | 609 | 609.00 | 542.00 | 685 | 685 | 685.00 | 633.00 |
| `SetBackColor` | 89 | 89 | 89.00 | 74.00 | 89 | 89 | 89.00 | 74.00 |
| `SetColorMatrix` | 196 | 196 | 196.00 | 165.00 | 196 | 196 | 196.00 | 165.00 |
| `SetFarColor` | 89 | 89 | 89.00 | 74.00 | 89 | 89 | 89.00 | 74.00 |
| `SetFogNearFar` | 19 | 342 | 168.36 | 158.75 | 19 | 342 | 168.36 | 158.75 |
| `SetGeomOffset` | 59 | 59 | 59.00 | 49.00 | 59 | 59 | 59.00 | 49.00 |
| `SetGeomScreen` | 23 | 23 | 23.00 | 18.00 | 23 | 23 | 23.00 | 18.00 |
| `SetLightMatrix` | 196 | 196 | 196.00 | 165.00 | 196 | 196 | 196.00 | 165.00 |
| `SetRotMatrix` | 37 | 37 | 37.00 | 36.00 | 37 | 37 | 37.00 | 36.00 |
| `SetTransMatrix` | 80 | 80 | 80.00 | 65.00 | 80 | 80 | 80.00 | 65.00 |
| `TransMatrix` | 9 | 9 | 9.00 | 8.00 | 9 | 9 | 9.00 | 8.00 |
| `TransposeMatrix` | 41 | 41 | 41.00 | 40.00 | 41 | 41 | 41.00 | 40.00 |
| `ratan2` | 124 | 140 | 131.89 | 123.06 | 124 | 140 | 131.89 | 123.06 |

The suite digest over the 1,216 cases takes 8,300,734 instructions and 9,971,992 cycles in the native ELF, and 8,347,326 instructions and 10,010,904 cycles in the portable ELF. These figures include the case set generator and the hash.

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
