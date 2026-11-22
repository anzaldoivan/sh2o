# SH-2 kernels

This guide lists the SH-2 kernels, their portable C paths and their status. To add a kernel, read [CONTRIBUTING-KERNELS.md](../CONTRIBUTING-KERNELS.md).

| Function | Header | Tag | SH-2 path | Portable path | Host test | SH-2 run |
|---|---|---|---|---|---|---|
| `sh2o_mul_q12` | `sh2o/fixed.h` | none | `DMULS.L`, `STS MACH`, `STS MACL` | 64-bit C product, floor shift | `fixed` | Exact, 617 of 783 vectors, native and portable (model) |
| `sh2o_lim_ir` | `sh2o/fixed.h` | none | C only | C | `fixed` | Exact, 166 of 783 vectors, native and portable (model) |
| `sh2o_k_dot3b` | `sh2o/k.h` | `k-dot3b-1` | C only | C | `k` | Exact, 92 of 518 vectors, native and portable (model) |
| `sh2o_k_mvb3` | `sh2o/k.h` | `k-mvb3-1` | `sh2o_k_mvb3_w2` (`MAC.W`; SR.S is checked) | C | `k` | Exact, 30 of 518 vectors, native and portable (model) |
| `sh2o_k_gpf3` | `sh2o/k.h` | `k-gpf3-1` | C only | C | `k` | Exact, 20 of 518 vectors, native and portable (model) |
| `sh2o_k_gpl3` | `sh2o/k.h` | `k-gpl3-1` | C only | C | `k` | Exact, 40 of 518 vectors, native and portable (model) |
| `sh2o_k_clz32` | `sh2o/k.h` | `k-clz32-1` | C only | C | `k` | Exact, 130 of 518 vectors, native and portable (model) |
| `sh2o_k_unr_recip` | `sh2o/k.h` | `k-unr-recip-1` | C only | C | `k` | Exact, 128 of 518 vectors, native and portable (model) |
| `sh2o_k_unr_div` | `sh2o/k.h` | `k-unr-div-1` | C only | C | `k` | Exact, 78 of 518 vectors, native and portable (model) |
| `sh2o_k_dot3b_w` | `sh2o/k.h` | `k-dot3b-macw-1` | 3 `MAC.W`; SR.S = 0 is assumed and not checked | none | none | Exact, 92 of 518 vectors, native (model) |
| `sh2o_k_mvb3_w` | `sh2o/k.h` | `k-mvb3-macw-1` | 9 `MAC.W`; SR.S = 0 is assumed and not checked | none | none | Exact, 30 of 518 vectors, native (model) |
| `sh2o_k_gpf3_w` | `sh2o/k.h` | `k-gpf3-macw-1` | 3 `MAC.W`; SR.S = 0 is assumed and not checked | none | none | Exact, 20 of 518 vectors, native (model) |
| `sh2o_k_gpl3_w` | `sh2o/k.h` | `k-gpl3-macw-1` | 3 `MAC.W`; SR.S = 0 is assumed and not checked | none | none | Exact, 40 of 518 vectors, native (model) |
| `sh2o_k_dot3b_w2` | `sh2o/k.h` | `k-dot3b-macw-2` | 3 `MAC.W`; SR.S is checked, C reference when S = 1 | none | none | Exact, 92 of 518 vectors, SR.S 0 and 1, native (model) |
| `sh2o_k_mvb3_w2` | `sh2o/k.h` | `k-mvb3-macw-2` | 9 `MAC.W`; SR.S is checked, C reference when S = 1 | none | none | Exact, 30 of 518 vectors, SR.S 0 and 1, native (model) |
| `sh2o_k_gpf3_w2` | `sh2o/k.h` | `k-gpf3-macw-2` | 3 `MAC.W`; SR.S is checked, C reference when S = 1 | none | none | Exact, 20 of 518 vectors, SR.S 0 and 1, native (model) |
| `sh2o_k_gpl3_w2` | `sh2o/k.h` | `k-gpl3-macw-2` | 3 `MAC.W`; SR.S is checked, C reference when S = 1 | none | none | Exact, 40 of 518 vectors, SR.S 0 and 1, native (model) |
| `sh2o_gte_dpct32` | `sh2o/gte.h` | `gte-dpct32-1` | C in 32-bit words (no 64-bit value, no call, no `MAC.L` or `MAC.W`); selected by `sh2o_gte_dpct` (and `sh2o_gte_command` op `2Ah`) on the SH-2 build, the portable build and the host keep the C path | the same C | `dpct32` | Exact, 13 DPCT vectors of 190, SR.S 0 and 1, native and portable (model) |
| `sh2o_gte_prepared_load` | `sh2o/gte_prepared.h` | `gte-mvmva-prepared-1` | C: the shadow of the matrices (rows R SHL 4) and translations (t SHL 16 as MACH, MACL; the range check per translation), constant shifts only, no call | the same C | `prepared` | Exact (loads checked byte by byte), native and portable (model) |
| `sh2o_gte_mvmva_prepared` | `sh2o/gte_prepared.h` | `gte-mvmva-prepared-1` | the inline kernel `sh2oi_gte_prepared_mac3_l` (`MAC.L` rows closed by `XTRCT`), after one read of SR.S (SR.S 1: `sh2o_gte_mvmva`); outside its gate `sh2o_gte_mvmva`; no default path calls it | `sh2oi_gte_prepared_mac3_c` (C) | `prepared` | Exact, 71 MVMVA vectors of 195, SR.S 0 and 1 at the load and at the call, native and portable (model) |
| `sh2o_gte_mvmva_list` | `sh2o/list.h` | `gte-mvmva-list-1` | C over the GTE; MVMVA's MAC stage is `sh2o_k_mvb3_w2` | C | `list` | Exact, 88 of 88 vectors, native and portable (model) |
| `sh2o_gte_mvmva_list_prepared` | `sh2o/gte_prepared.h` | `gte-mvmva-list-prepared-1` | one `sh2o_gte_prepared_load` of a stack shadow per list, then the inline kernel `sh2oi_gte_prepared_mac3_l` per item, after one read of SR.S (SR.S 1: `sh2o_gte_mvmva_list`); outside its gate `sh2o_gte_mvmva_list`; no default path calls it | `sh2oi_gte_prepared_mac3_c` (C) | `listprep` | Exact, 88 of 88 vectors and 1,024 random lists, SR.S 0 and 1, native and portable (model) |
| `sh2o_zam_*` (experimental) | `sh2o/zam_experimental.h` | `zam-mac3-1` and 4 more | SH2ZAM's kernels, linked only with `SH2O_ZAM` | SH2ZAM's portable C (`SH2Z_FORCE_PORTABLE`) | `zam` (option on only) | Exact: the pinned counts and digest, native and portable (model, option on only) |

The SH-2 run is `make sh2-model`: the SH-2 instruction model runs the 783 vectors of `fixed` (617 for `sh2o_mul_q12`, 166 for `sh2o_lim_ir`) with zero differences in the native and the portable program. For the k family, it runs the 518 vectors of `tests/k_oracle.h` with zero differences: each public entry and C reference in the native and the portable program, and each `_w` and `_w2` kernel in the native program. The entries and the `_w2` kernels are also exact with SR.S = 1. No console run exists. [VALIDATION.md](VALIDATION.md) has the records, one for each k contract tag.

No speed is given here. A speed goes in [benchmarks/README.md](../benchmarks/README.md), with its source.

## sh2o_mul_q12

`DMULS.L` writes the signed 64-bit product to MACH and MACL. The kernel takes bits 12 to 43 of the product. `DMULS.L` does not use the SR.S bit, so the kernel does not check it. Reference: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5, section 6.1.20 (DMULS.L).

## sh2o_k_*

`MAC.W` multiplies two signed 16-bit words and adds the product to MACH:MACL. When SR.S = 1, `MAC.W` saturates the sum to 32 bits in MACL. The `_w` kernels assume SR.S = 0. They do not read or set SR. With SR.S = 1 their results differ for some vectors; the validation records give the counts. The `_w2` kernels read SR and do not write it. When S is set, they run the C reference. When S is clear, they run the `_w` body. The SH-2 path of `sh2o_k_mvb3` is `sh2o_k_mvb3_w2`. Reference: Renesas SH-1/SH-2/SH-DSP Software Manual, rev. 5.00, section 6.1.32 (MAC.W).

## sh2o_gte_*

The GTE commands (`sh2o/gte.h`, tags `gte-regs-1`, `gte-command-1` and one tag for each command) are C. On the SH-2, the MAC stage of MVMVA (not mx = 3, not cv = 2), RTPS, RTPT, NCS, NCT, NCDS, NCDT and NCCS calls the entry `sh2o_k_mvb3`, so its SH-2 path is the kernel `sh2o_k_mvb3_w2` (`MAC.W`; SR.S is checked). The dispatch is in `src/sh2o_gte.c`. It uses the kernel only when no partial sum can pass 43 bits. Else the C path sets the FLAG bits. GPF and GPL call the entries `sh2o_k_gpf3` and `sh2o_k_gpl3`, which are C. RTPS and RTPT also call `sh2o_k_unr_div`, which is C. The fixed-word entries `sh2o_gte_nrtps` and `sh2o_gte_nrtpt` (tags `gte-nrtps-1`, `gte-nrtpt-1`) call `sh2o_gte_rtps` and `sh2o_gte_rtpt` with one word each, so they take the same path. DPCS calls no kernel. On the SH-2 build DPCT (tag `gte-dpct-1`, DPCS's step three times on the colour FIFO) selects the 32-bit kernel `sh2o_gte_dpct32` (tag `gte-dpct32-1`); the portable build and the host keep the C path. [GTE.md](GTE.md) gives the commands, their tests and the SH-2 run.

## sh2o_psyq_*

The PSY-Q bodies (`sh2o/psyq.h`, one tag for each body) are C over the GTE image. They have no kernel of their own. On the SH-2, CompMatrix, ApplyMatrix, ApplyMatrixLV and RotTrans run MVMVA through `sh2o_gte_command`, so their MAC stage is the k kernel `sh2o_k_mvb3_w2` (`MAC.W`; SR.S is checked). The psyq object itself has no `MAC.W`: its spec `tests/sh2/psyq_static.txt` has `count * mac.w 0`. With SR.S = 1, the suite hash of the native program differs for 0 of the 1,216 cases ([benchmarks/psyq-model/README.md](../benchmarks/psyq-model/README.md)). [PSYQ.md](PSYQ.md) gives the bodies, their tests and the SH-2 run.

## sh2o_gte_mvmva_list

The MVMVA list (`sh2o/list.h`, tag `gte-mvmva-list-1`) is C over the GTE image. It has no kernel of its own. For each item it calls `sh2o_gte_mvmva`, so on the SH-2 its MAC stage is the k kernel `sh2o_k_mvb3_w2` where MVMVA uses it (`MAC.W`; SR.S is checked). The list object itself has no `MAC.W` (`tests/sh2/list_static.txt`). It runs on the calling CPU only. With SR.S = 1, the native result differs for 0 of the 88 vectors ([benchmarks/list-model/README.md](../benchmarks/list-model/README.md)). [GTE.md](GTE.md#the-mvmva-list) gives its tests and the SH-2 run.

## sh2o_zam_*

The experimental delegate (`sh2o/zam_experimental.h`, tags `zam-mac3-1`, `zam-mvmva-list-1`, `zam-transposematrix-1`, `zam-setfognearfar-1` and `zam-ratan2-1`) calls SH2ZAM's kernels. sh2o links SH2ZAM only when the CMake option `SH2O_ZAM` is on. The option is off by default, and the default library and the installed package have no SH2ZAM code. On the host, SH2ZAM uses its portable C path (`SH2Z_FORCE_PORTABLE`). On the SH-2 model, SH2ZAM's `saturn/src/*.c` is linked without changes. In the model, the delegate takes more cycles than the GTE core ([benchmarks/zam-model/README.md](../benchmarks/zam-model/README.md)). [ZAM.md](ZAM.md) gives its domain, its tests and the SH-2 run.
