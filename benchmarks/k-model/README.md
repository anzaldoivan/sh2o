# k family model bridge

`bridge.c` has one wrapper for each k kernel path: the public entry, the C reference (`_c`) and, in the native build, the MAC.W kernels: `_w` (does not check `SR.S`) and `_w2` (checks `SR.S`). Each wrapper takes at most four arguments, and a wrapper with an `int64` result writes the result through a pointer. The harness reads only `R0`. The inputs go in the `sh2o_model_k_io` buffer.

`make sh2-model` links the bridge, the k suite (`tests/suite/sh2o_k_test_suite.c`) and the SH-2 build of the library into one ELF for each build mode (native and portable).

The bridge has a table of typed function pointers: the three resumable digest functions of the suite (`sh2o_k_test_dot3b_part`, `sh2o_k_test_mvb3_part`, `sh2o_k_test_gp3_part`), then the wrappers. The table keeps these functions in the ELF. It also makes the build fail when a signature in `include/sh2o/k.h` or `tests/suite/sh2o_k_test.h` changes and the table does not.

The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of its function.

## MODELLED figures

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2` in the Yaul image `ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`. ELF sha256: native `1b589ab0cf3d423f64deb76f2ac5a49b24eb7bd366cc34d420922528bbe085a4`, portable `e1c38efe9ac9d662c32797ebc01172c93bb06953a796d07525446525ac4a9301` (2026-11-22: the prepared MVMVA list source `src/sh2o_list_prepared.c` joined the library and changed the hashes, not the figures; before it: native `c927e445…`, portable `73fd166b…`; 2026-11-22: the prepared MVMVA source `src/sh2o_gte_prepared.c` joined the library and changed the hashes, not the figures; before it: native `cf7eeb8e…`, portable `531a0bcd…`; 2026-11-22: the native `sh2o_gte_dpct` selects the 32-bit DPCT kernel, which changed the native hash, not the figures; before it: native `d6199920…`; 2026-11-22: the 32-bit DPCT kernel `src/sh2o_gte_dpct32.c` joined the library and changed the hashes, not the figures; before it: native `039a433d…`, portable `d987599d…`; 2026-11-22: DPCT added to `src/sh2o_gte.c` changed the hashes, not the figures; before it: native `ff680949…`, portable `af3c5d75…`; the programs link every library object; before the SR.S-checking MAC.W kernels (`_w2`), when the figures changed too: native `ada129c0…`, portable `b81ed34a…`; the hashes changed when the GTE sources were added, again when the PSY-Q sources were added, again when the list source `src/sh2o_list.c` was added and again when the delegate source `src/sh2o_zam.c` was added (with the option `SH2O_ZAM` off it compiles to no symbol; only its file name joins the symbol table), the figures did not; before the delegate source: native `eb9629ed…`, portable `18f7f156…`; before the list source: native `532629e6…`, portable `a7008ccb…`; before the PSY-Q sources: native `82a005f6…`, portable `2f0f5c3f…`; before the GTE sources: native `f35ba52e…`, portable `e6986eb5…`). The figures are for one call of the bridge wrapper, from its entry to its return, so they include the call. All 518 vectors of `tests/k_oracle.h` are exact on each path before a figure is given. `entry` is the public function, `c` the C reference and `w` the MAC.W kernel that does not check `SR.S` and `w2` the one that does (native only).

| Program | Function | Path | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|---|
| native | `sh2o_k_dot3b` | entry | 56 | 56 | 56.00 | 55.00 | 92 |
| native | `sh2o_k_dot3b` | w | 31 | 31 | 31.00 | 24.00 | 92 |
| native | `sh2o_k_dot3b` | w2 | 35 | 35 | 35.00 | 28.00 | 92 |
| native | `sh2o_k_mvb3` | entry | 133 | 133 | 133.00 | 113.00 | 30 |
| native | `sh2o_k_mvb3` | c | 210 | 210 | 210.00 | 204.00 | 30 |
| native | `sh2o_k_mvb3` | w | 114 | 114 | 114.00 | 95.00 | 30 |
| native | `sh2o_k_mvb3` | w2 | 137 | 137 | 137.00 | 116.00 | 30 |
| native | `sh2o_k_gpf3` | entry | 40 | 40 | 40.00 | 37.00 | 20 |
| native | `sh2o_k_gpf3` | w | 38 | 38 | 38.00 | 31.00 | 20 |
| native | `sh2o_k_gpf3` | w2 | 42 | 42 | 42.00 | 34.00 | 20 |
| native | `sh2o_k_gpl3` | entry | 78 | 105 | 91.50 | 87.00 | 40 |
| native | `sh2o_k_gpl3` | w | 98 | 125 | 111.50 | 101.00 | 40 |
| native | `sh2o_k_gpl3` | w2 | 102 | 129 | 115.50 | 104.00 | 40 |
| native | `sh2o_k_clz32` | entry | 26 | 28 | 27.01 | 23.55 | 130 |
| native | `sh2o_k_unr_recip` | entry | 25 | 25 | 25.00 | 22.00 | 128 |
| native | `sh2o_k_unr_div` | entry | 77 | 78 | 77.15 | 66.08 | 78 |
| portable | `sh2o_k_mvb3` | entry | 210 | 210 | 210.00 | 204.00 | 30 |

The other rows are the same as a row above: in the native program `c` is the same as `entry` for every function but `sh2o_k_mvb3`. In the portable program every function has the native `c` figures. Only `sh2o_k_mvb3` calls its MAC.W kernel on the SH-2; the other entries call the C reference. The `sh2o_k_mvb3` entry and `w2` rows run the same kernel; their wrappers are compiled differently.

The harness also runs the suite's digests over all 113,471 vectors in each program. Each digest gives its pin from `tests/suite/sh2o_k_test.h`, and each of the six native test mutants gives a digest that is not its pin. The harness runs the entry, `w` and `w2` wrappers of the four MAC.W functions again with `SR.S` = 1. The entry and `w2` results are exact (a difference fails the run). The `w` kernels assume `SR.S` = 0, so their counts are printed only: dot3b 24 of 92, mvb3 16 of 30, gpf3 15 of 20, gpl3 24 of 40 differ. The `SR.S` check costs `w2` over `w`, in mean MODELLED cycles at `SR.S` = 0: dot3b +4.00, mvb3 +23.00, gpf3 +4.00, gpl3 +4.00.

An instruction count is not a speed.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
