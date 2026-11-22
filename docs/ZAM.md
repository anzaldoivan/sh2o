# The SH2ZAM delegate (experimental)

This guide tells how the experimental delegate gives a part of the GTE work to [SH2ZAM](https://github.com/lamb2k/SH2ZAM), and how the tests prove that the bytes stay the same.

**Status: experimental, off by default.** The default library does not link SH2ZAM and does not use the delegate. The installed package never has the delegate. In the SH-2 instruction model, the delegate takes more cycles than sh2o's own GTE core for five of its six operations, so it stays off. The list takes fewer cycles; it is a candidate, and it stays off too (see [The decision](#the-decision-delegate-versus-core)).

## Build with the option

The delegate needs the SH2ZAM submodule:

```sh
git submodule update --init external/SH2ZAM
```

The CMake option `SH2O_ZAM` (default `OFF`) builds the host library with the delegate. The Makefile gives each build its own folder:

```sh
make test SH2O_ZAM=ON        # build/<cc>-zam
make sh2-model SH2O_ZAM=ON   # build/sh2-model-zam
```

With the option, `CMakeLists.txt` does these steps:

- It adds SH2ZAM with `add_subdirectory(external/SH2ZAM)` and links the target `SH2ZAM::sh2zam` to `sh2o` as PRIVATE.
- It defines `SH2O_ZAM=1` for `sh2o` as PRIVATE, so consumers and mutant programs do not get it.
- On the host, it sets `SH2Z_FORCE_PORTABLE`, so SH2ZAM uses its portable C path.

`make sh2-model SH2O_ZAM=ON` links SH2ZAM's `saturn/src/*.c` (without changes) as `libsh2zam.a` into each SH-2 program, native and portable.

## The fence

Only `src/sh2o_zam.c` includes SH2ZAM. Without `SH2O_ZAM=1`, it compiles to an empty unit. The header `include/sh2o/zam_experimental.h` is not in `sh2o/sh2o.h` and is not installed.

- **`zam_isolation`** (CTest, `tests/zam_isolation_check.py`). It checks that no other file names SH2ZAM, that `sh2o/sh2o.h` never includes `sh2o/zam_experimental.h`, that `CMakeLists.txt` names SH2ZAM only inside `if(SH2O_ZAM)` blocks with the option `OFF` by default, and that a default `libsh2o.a` has no `sh2z` or `sh2o_zam` symbol. Each rule has a plant that the check must catch.
- **`make install-check`.** It installs the default build and checks that the prefix has no `zam_experimental.h`, no `sh2zam` path or text, and no `sh2z` or `sh2o_zam` symbol in `libsh2o.a`.

## What the GTE core gives to the delegate

With `SH2O_ZAM=1`, `src/sh2o_gte.c` calls `sh2o_zam_mac3` for the MAC stage of these commands:

- MVMVA with sf = 1, mx < 3 and cv other than 2.
- RTPS and RTPT with sf = 1.

When the delegate refuses an input, the GTE core runs its own kernel. The other commands never go to the delegate.

## The domain and the gate

Contract tag `zam-mac3-1`. The delegate takes an input only when all these are true:

- Each |m[i][j]| and |v[j]| is at most 0x8000.
- For each row i, with S = sum_j |m[i][j]| x |v[j]| and T = t[i]: 16S + 65536|T| <= 2^47 - 1.
- SH2ZAM's plan reports its fast path.

The delegate checks the domain before SH2ZAM runs. Inside it, the delegate returns 1 and gives mac[i] = (t[i] x 4096 + m[i].v) SAR 12. Outside it, the delegate returns 0 and does not change mac.

The proof is in `src/sh2o_zam.c`:

```
Row i, R = m[i], S = sum_j |R_j||v_j|, x = R.v, T = t[i].
SH2ZAM is handed the row R*16 with translation T and the point v (raw integers in its Q16.16 slots); its fast path
gives
  z = floor(16x / 65536) + T = floor(x / 4096) + T = floor((4096T + x) / 4096) = (4096T + x) SAR 12,
which is the GTE core's MAC with sf = 1 provided its 44-bit partials do not wrap. The gate 16S + 65536|T| <= 2^47-1
gives S + 4096|T| <= 2^43-1 (both sides of 16*(..) are multiples of 16), and every partial the GTE core forms
(4096T, then + R_0 v_0, then + R_1 v_1, then + R_2 v_2) is bounded in magnitude by S + 4096|T| < 2^43: no FLAG
30-25 bit, no truncation, and the result fits int32. So the GTE core's tail, fed full = mac * 4096 (whose SAR 12
is mac), writes the same 64 words.
The left side is always a multiple of 16, so the largest admitted row is exactly 2^47-16; the next, 2^47, is refused.
```

## The other entry points

The header has four more entry points. No sh2o body calls them. They run only in the delegate's suite, in a build with `SH2O_ZAM=ON`.

| Entry point | Contract tag | Result |
|---|---|---|
| `sh2o_zam_TransposeMatrix` | `zam-transposematrix-1` | Proven. 8,198 cases, 0 mismatches. |
| `sh2o_zam_SetFogNearFar` | `zam-setfognearfar-1` | Proven where it delegates. 24,832 cases: 5,122 delegated, 19,710 refused, 0 mismatches. |
| `sh2o_zam_mvmva_list` | `zam-mvmva-list-1` | 1,024 runs, 12,544 items: 549 delegated, 475 refused, 0 mismatches; 547 of 547 planted swaps caught. |
| `sh2o_zam_ratan2` | `zam-ratan2-1` | Refused. `sh2z_atan2` gives other bytes: 2,342,014 of 16,785,409 grid cases differ, by at most 1. |

`sh2o_zam_mvmva_list` takes the contract of `sh2o_gte_mvmva_list` ([GTE.md](GTE.md#the-mvmva-list)) for MVMVA with sf 1, mx < 3, V0 and cv other than 2, n <= 32, and the row gate of `zam-mac3-1` on the largest |v| of each axis.

## Sources and counts

Build the tests with `make test CC=gcc SH2O_ZAM=ON` (the folder `build/gcc-zam`). Each item gives the command that runs it again.

- **Suite.** `tests/suite/sh2o_zam_test_suite.c` runs 118,347 items in three sets: (A) the k suite's vectors, (B) 256 RTPS, 256 RTPT, 256 MVMVA and each MVMVA selector 16 times, from the gte suite's seed, and (C) 11 edge rows and the domain checks. Each command runs with the delegate and with the GTE core's own kernel, and the 64 registers must be equal. The pins (`tests/suite/sh2o_zam_test.h`): 345,321 cases, 101,380 delegated, 243,941 refused, FNV-1a 32 digest `0xD444928C`. `sh2o_zam_mac3` delegates 154,694 calls and refuses 162,410. The test also runs the four trials of the table above and pins their counts. Command: `ctest --test-dir build/gcc-zam -R '^zam$'`.
- **Mutants.** `src/sh2o_zam.c` has 5 mutants (`-DSH2O_MUTANT=k`). Each must make the suite fail with its pinned code. Command: `ctest --test-dir build/gcc-zam -R '^zam_mutant_'`.

| k | Mutant | Planted bug | Code |
|---|---|---|---|
| 1 | `rows_shift_3` | Rows shifted by 3, not 4. | 0x1001 |
| 2 | `gate_2_49` | Gate widened to 2^49 - 1, fast-path check dropped. | 0x1101 |
| 3 | `tr_dropped` | Translation dropped. | 0x1001 |
| 4 | `transpose_twice` | TransposeMatrix through SH2ZAM twice (no transpose). | 0x5001 |
| 5 | `fog_no_wrap_gate_dropped` | SetFogNearFar's no-wrap gate dropped. | 0x6001 |

- **Other families.** `make test SH2O_ZAM=ON` runs all the families again with the delegate on the host. `make sh2-model SH2O_ZAM=ON` runs them again on the SH-2 model. They give the same oracle results, digests and counts as without the option.
- **Isolation.** `ctest --test-dir build/gcc -R '^zam_isolation$'` (default build) and `make install-check`. See [The fence](#the-fence).
- **SH-2.** `make sh2-model SH2O_ZAM=ON` runs the suite and the trials in the native and the portable program. Both programs give the pins exactly. For each set (A) case that the delegate takes, the 64 registers are equal with the delegate and with the core. With `SR.S` = 1, 0 of the 118,347 items end in a different state in the native program (core 0 of 58,293, delegated 0 of 60,054). The library checks `SR.S`.

## MODELLED figures

Mean cycles of one `sh2o_gte_command` call over the cases that the delegate takes, from [benchmarks/zam-model/README.md](../benchmarks/zam-model/README.md). These are MODELLED, not Saturn timings.

| Program | Command | Delegate | Core |
|---|---|---|---|
| native | MVMVA | 1517.92 | 522.38 |
| native | RTPS | 1750.04 | 753.53 |
| native | RTPT | 5051.23 | 2065.73 |
| portable | MVMVA | 1649.92 | 597.01 |
| portable | RTPS | 1882.04 | 826.80 |
| portable | RTPT | 5447.23 | 2283.80 |

The delegate figures include its gate. In this model, the delegate takes more cycles than the core for each command.

Mean cycles of one call of the other entry points, from the same run, on the cases that the delegate takes: each case runs the delegate entry and the core entry on the same input, and the outputs are equal before the cycles count. The list core is `sh2o_gte_mvmva_list` with `sh2o_zam_bypass` 1 (the GTE core's own kernel). One list item is the cycles of the call divided by its n (n from 1 to 32).

| Program | Entry point | Cases | Delegate | Core |
|---|---|---|---|---|
| native | TransposeMatrix | 8,198 | 142.00 | 41.00 |
| native | SetFogNearFar | 5,122 | 960.73 | 339.73 |
| native | list, one call | 549 runs | 2,874.72 | 4,664.79 |
| native | list, one item | 549 runs | 339.64 | 529.58 |
| portable | TransposeMatrix | 8,198 | 142.00 | 41.00 |
| portable | SetFogNearFar | 5,122 | 942.73 | 339.73 |
| portable | list, one call | 549 runs | 3,293.04 | 5,333.01 |
| portable | list, one item | 549 runs | 388.21 | 605.58 |

The delegate list takes fewer cycles than the core on these runs. Its cost varies more: one item costs from 224.78 to 1,433.00 cycles (native), and its smallest run costs 1,398 cycles against the core's 569. SH2ZAM prepares its plan once for each list, so a long run spreads that cost and a short run does not.

## The decision: delegate versus core

This is the record of the choice (four lines: source, alternatives, inputs, bar).

- **Source.** SH2ZAM's public functions at the pin `7f99eb70`, as `src/sh2o_zam.c` calls them: `sh2z_mesh_view_init`, `sh2z_transform_plan_init` and `sh2z_transform_plan_execute_mac` (MVMVA, RTPS, RTPT and the list), `sh2z_mat33_transpose` (TransposeMatrix), `sh2z_muldiv` (SetFogNearFar) and `sh2z_atan2` (`ratan2`, refused). `tests/sh2/zam_on_static.txt` checks that the object with the option on needs only these names and `sh2o_gte_write`.
- **Alternatives.** For each operation: the GTE core and the PSY-Q bodies (adopted: the default); the delegate (adapted: it takes only the inputs where its proof gives the core's bytes, else the core runs); for MVMVA and its list, sh2o's own kernels with the method of SH2ZAM's prepared plan (the matrix prepared once at the load, the range check moved to the load, MAC.L rows), which are planned beside the current kernels and have their own records.
- **Inputs.** SH2ZAM's contract is Q16.16 with saturation and rounding down, with no GTE operation; the consumer needs the GTE's exact bytes, flags included. The delegate gives those bytes only inside its gate (see [The domain and the gate](#the-domain-and-the-gate)). In the consumer's frame, MVMVA is the most frequent operation (about 650 calls a frame, about 4.3 for each rotation matrix load, about one in five in lists with a mean of 8.6 items; a host count of the consumer's port, 2026-11, not measured on the SH-2); it has no count for TransposeMatrix or SetFogNearFar.
- **Bar.** An operation of the delegate stays off by default unless both hold: it is exact at SR.S 0 and at SR.S 1 on every case it takes (each delegate call runs again with SR.S = 1 in the native program and must give the same result), and its mean MODELLED cycles of one call are fewer than the core's on the same cases. An operation that passes is a candidate. A default build with SH2ZAM is a change of scope, so a candidate goes to the plan, not to the default.

| Operation | SR.S 1 differences | Delegate | Core | Result |
|---|---|---|---|---|
| MVMVA | 0 of 20,558 | 1,517.92 | 522.38 | Off: slower |
| RTPS | 0 of 52,821 | 1,750.04 | 753.53 | Off: slower |
| RTPT | 0 of 26,515 | 5,051.23 | 2,065.73 | Off: slower |
| TransposeMatrix | 0 of 8,198 | 142.00 | 41.00 | Off: slower |
| SetFogNearFar | 0 of 5,122 | 960.73 | 339.73 | Off: slower |
| list (one call) | 0 of 549 | 2,874.72 | 4,664.79 | Candidate, off by default |

Native program, MODELLED cycles. The list is the only candidate. Before it can be the default, three things are open: the cost of a refused list (the delegate takes 549 of the 1,024 trial runs; a refusal pays the gate and then the core), the share of the consumer's lists that the gate takes and their lengths, and a comparison with sh2o's own list kernel on the prepared matrix.

The records are [validation/zam-mac3-1.md](validation/zam-mac3-1.md), [validation/zam-ratan2-1.md](validation/zam-ratan2-1.md), [validation/zam-transposematrix-1.md](validation/zam-transposematrix-1.md), [validation/zam-setfognearfar-1.md](validation/zam-setfognearfar-1.md) and [validation/zam-mvmva-list-1.md](validation/zam-mvmva-list-1.md).

## Not done

- No console run. The delegate did not run on a Saturn.
- No emulator run with caches. The figures come from a model with flat RAM.
- `ratan2` is refused. SH2ZAM's `sh2z_atan2` does not give the `libgte` bytes.
- No default. The delegate is slower than the core in five of its six operations in the SH-2 instruction model; the list is faster on the runs it takes, and it stays off until the open points of [The decision](#the-decision-delegate-versus-core) are closed.
- The `ratan2` sweep and the delegate mutants run on the host only, as in the original port.

License: MIT, as for all of sh2o. SH2ZAM is MIT; see [NOTICE.md](../NOTICE.md).
