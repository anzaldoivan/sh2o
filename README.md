<div align="center">
  <h1><strong>sh2o</strong></h1>
  <p>
    A replacement layer for the PS1 GTE and libgte in PS1 game ports to the Sega Saturn
    <br />
    <a href="docs/README.md"><strong>Read the guides »</strong></a>
    <br /><br />
    Byte-exact · Fixed-point · Freestanding C11
  </p>
</div>
<hr>

# Overview

sh2o is a replacement layer for the PlayStation Geometry Transformation Engine (GTE) and the PSY-Q `libgte` calls. Use it when you port a PS1 game to the Sega Saturn. It runs the GTE commands and the `libgte` calls of the game on the SH-2, in the PS1 formats. Each result must be byte-exact against a PS1 reference: the same values, the same saturation and the same FLAG bits.

sh2o is not a general-purpose maths library. For Saturn maths that does not need PS1-exact results, use [SH2ZAM](https://github.com/lamb2k/SH2ZAM). See [sh2o or SH2ZAM](#sh2o-or-sh2zam).

sh2o uses the layout, the build options and the SH-2 kernel methods of SH2ZAM, but it does not replace SH2ZAM. sh2o does not need SH2ZAM to build or to run. An experimental delegate gives work to SH2ZAM: the CMake option `SH2O_ZAM`, off by default. It needs the SH2ZAM submodule (`git submodule update --init external/SH2ZAM`). See [docs/ZAM.md](docs/ZAM.md).

> [!IMPORTANT]
> **Status: ported and validated, not measured on a console.** This version has the fixed-point primitives, the k kernels, the GTE register image and its 15 commands (DPCT included), the fixed-word entries `sh2o_gte_nrtps` and `sh2o_gte_nrtpt`, 19 PSY-Q `libgte` bodies, the MVMVA list, and a second MVMVA path and list with a prepared shadow (`sh2o/gte_prepared.h`; no default path uses them). The SH-2 `MAC.W` kernels read the saturation bit (SR.S) and run the C path when it is set, and DPCT has a 32-bit SH-2 kernel. The option `SH2O_COUNT` counts the calls of each command, entry and body. Each family is byte-exact against its oracle on the host and in an SH-2 instruction model, with SR.S clear and set. The figures of this version are in [the record](docs/VALIDATION.md#2026-11-22-saturation-safe-kernels-counters-dpct-and-the-prepared-mvmva). The port was validated on 2026-11-21: see [the record](docs/VALIDATION.md#2026-11-21-port-validation). Public proofs against psyz at its pin compare the `ratan2` table, a `ratan2` sweep, the psyz `libgte` known answers and the GTE results, and the SH-2 objects build again to the same bytes: see [the public proofs](docs/VALIDATION.md#2026-11-21-public-proofs-against-psyz). An optional job, `make psyq47`, gets the PSY-Q 4.7 library at test time and runs its code against the 19 bodies. The library has 561 `libgte` functions. sh2o has 19 of them. On 2026-11-21, all 19 match (see [the record](docs/VALIDATION.md#2026-11-21-the-psy-q-47-job)). No result has been measured on a Saturn console.

# Features

- C11 API with the `sh2o_` prefix. No C library calls, no floating point and no heap allocation in library code.
- A portable C path beside each SH-2 kernel. Both paths give the same bytes.
- Oracle tests that compare each result byte for byte and print the first difference.
- A mutant check: each test must catch a set of known bugs.
- A written style, checked by `make check`.
- Installable CMake package `sh2o::sh2o`. The SH-2 cross build uses a pinned Yaul image.

# APIs

| Header | Contents | Status |
|---|---|---|
| [`sh2o/sh2o.h`](include/sh2o/sh2o.h) | Version, backend name, and all stable headers. | Stable |
| [`sh2o/cdefs.h`](include/sh2o/cdefs.h) | Selection of the SH-2 path and the attribute macros of the public headers. | Stable |
| [`sh2o/fixed.h`](include/sh2o/fixed.h) | Q12 multiply with the GTE shift, IR limit. | Stable |
| [`sh2o/k.h`](include/sh2o/k.h) | Fixed-point kernels of the GTE: dot product, matrix x vector, GPF and GPL, CLZ, UNR reciprocal and divide. On the SH-2, only `sh2o_k_mvb3` calls its `MAC.W` kernel that reads SR.S (`sh2o_k_mvb3_w2`). The other `_w2` kernels are validated, but no default path calls them. | Stable |
| [`sh2o/gte.h`](include/sh2o/gte.h) | The GTE register image, its 15 commands (DPCT: `sh2o_gte_dpct`, with the SH-2 kernel `sh2o_gte_dpct32`) and the fixed-word entries `sh2o_gte_nrtps` and `sh2o_gte_nrtpt`. See [GTE.md](docs/GTE.md). | Stable |
| [`sh2o/psyq.h`](include/sh2o/psyq.h) | 19 PSY-Q `libgte` bodies over the GTE image. See [PSYQ.md](docs/PSYQ.md). | Stable |
| [`sh2o/list.h`](include/sh2o/list.h) | The MVMVA list: n of one MVMVA command in one call. See [GTE.md](docs/GTE.md#the-mvmva-list). To decide between a list and single calls, see [When to batch](docs/GTE.md#when-to-batch). | Stable |
| [`sh2o/gte_prepared.h`](include/sh2o/gte_prepared.h) | MVMVA (`sh2o_gte_prepared_load`, `sh2o_gte_mvmva_prepared`) and the MVMVA list (`sh2o_gte_mvmva_list_prepared`) with a prepared shadow of the matrices and translations, beside `sh2o_gte_mvmva` and `sh2o_gte_mvmva_list`. Not on the default path; not in `sh2o/sh2o.h`. See [GTE.md](docs/GTE.md#mvmva-with-a-prepared-shadow). To decide when to use them, see [When to batch](docs/GTE.md#when-to-batch). | New |
| [`sh2o/count.h`](include/sh2o/count.h) | The call counters: read by index or name, reset. Only with `SH2O_COUNT=ON`; not in `sh2o/sh2o.h`. See [COUNT.md](docs/COUNT.md). | Stable |
| [`sh2o/zam_experimental.h`](include/sh2o/zam_experimental.h) | The SH2ZAM delegate. Only with `SH2O_ZAM=ON`; not installed, not in `sh2o/sh2o.h`. See [ZAM.md](docs/ZAM.md). | Experimental |

Not done: the delegate `ratan2` sweep and the delegate mutants run on the host only. The prepared MVMVA and its list are not on a default path: that change waits until a consumer confirms their speed on its own emulator and measures its MVMVA calls per matrix load with `SH2O_COUNT`.

A family that the package does not install goes in `sh2o/<family>_experimental.h`. A kernel that the package installs and that no default path uses yet has its own header, which `sh2o/sh2o.h` does not include.

# Usage

Include one header and link `sh2o::sh2o`:

```c
#include <sh2o/sh2o.h>

sh2o_q12_t half=SH2O_Q12_ONE/2;
int32_t ir=sh2o_lim_ir(sh2o_mul_q12(INT32_C(100000),half),false);
```

# Installation

## CMake

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSH2O_STRICT=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /your/prefix
```

`make test` does the same steps in `build/<compiler>`. Set `CC=gcc` or `CC=clang` to select the compiler. An installed copy is found with `find_package(sh2o 0.1 CONFIG REQUIRED)`.

| Option | Default | Effect |
|---|---|---|
| `SH2O_BUILD_TESTS` | ON at top level | Builds and registers the host tests. |
| `SH2O_BUILD_EXAMPLES` | ON at top level | Builds the examples and runs them as tests. |
| `SH2O_FORCE_PORTABLE` | OFF | Uses the C path also on the SH-2. |
| `SH2O_SANITIZE` | OFF | Host build with ASan and UBSan. |
| `SH2O_STRICT` | OFF | Stops the build on a warning. |
| `SH2O_COUNT` | OFF | Counts the calls of each GTE command, entry and PSY-Q body, and the register writes. Without it, the objects do not change. See [COUNT.md](docs/COUNT.md). |

## Fixed-point primitives

`sh2o/fixed.h` is the sample family. It shows the contract form, the test form and the kernel form that each family uses.

- `sh2o_mul_q12(a,b)` makes the 64-bit product, shifts it right by 12 and keeps the low 32 bits. The shift rounds toward negative infinity. The result wraps, as the GTE MAC registers do. On the SH-2 it uses `DMULS.L`.
- `sh2o_lim_ir(value,lm)` limits a value to the IR range: [-32768, 32767], or [0, 32767] when `lm` is true.

Test: [`tests/test_fixed.c`](tests/test_fixed.c), with vectors from [`tests/vectors/fixed.vec`](tests/vectors/fixed.vec).

## Link only what you call

Each function is in its own section (`-ffunction-sections`). Link with `--gc-sections` to remove the functions that you do not call.

## Git submodule

```sh
git submodule add https://github.com/anzaldoivan/sh2o.git external/sh2o
```

```cmake
add_subdirectory(external/sh2o)
target_link_libraries(your_game PRIVATE sh2o::sh2o)
```

The tests and examples are off when sh2o is not the top-level project.

## Sega Saturn / Yaul

Use a bare-metal SH-2 GCC, for example Yaul's `sh2eb-elf-gcc`:

```sh
cmake -S . -B build-sh2 -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchains/yaul-sh2.cmake"
cmake --build build-sh2 --parallel
```

`make cross` does a cross build in the pinned Yaul image. It also makes sure that the library objects need no `memcpy`, `memset` or `memmove`.

# Examples

| Example | Shows |
|---|---|
| [`examples/fixed_basics.c`](examples/fixed_basics.c) | One Q12 multiply and one IR limit. |

See [examples/README.md](examples/README.md).

# Conventions

| Item | sh2o |
|---|---|
| Matrices | Q3.12 `int16_t` elements, row-major, as the GTE keeps them |
| Maths vectors | `int16_t` or `int32_t` vector components, as the PS1 call uses them |
| Results | Byte-exact against the PS1 reference: the same values, the same saturation and the same FLAG bits |
| Byte order | The API uses native C integers. It builds each packed PS1 word with shifts. Only the test vector files keep PS1 little-endian bytes. |
| Rounding and saturation | As the GTE command does. Each function states them. |

SH2ZAM uses Q16.16. A function name that sh2o and SH2ZAM share does not mean the same format or the same results.

# sh2o or SH2ZAM

Use sh2o where ported PS1 code must give the exact results of the PS1: game state, collision, and all results that a replay or the PS1 output must match. sh2o takes and gives the PS1 formats.

Use [SH2ZAM](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md) for new Saturn code and for maths that does not need PS1-exact results. SH2ZAM is a general-purpose fixed-point maths library for the Saturn, with Q16.16 arithmetic, vectors, matrices and quaternions ([README.md:25](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L25), [:51-59](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L51-L59)). It also has batch transforms ([:116-132](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L116-L132)), SCU DSP kernels ([:314-326](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L314-L326)) and sprite kernels ([:353-362](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L353-L362), [:395-409](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L395-L409)).

The two libraries use different formats. sh2o uses the PS1 1.3.12 and 1.19.12 fields. SH2ZAM uses signed Q16.16 ([README.md:35](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L35)). A value that goes from one library to the other needs an explicit conversion. After a conversion, the result is not PS1-exact.

sh2o does not replace SH2ZAM. A port can link the two libraries: `sh2o::sh2o` and `SH2ZAM::sh2zam` ([README.md:456-465](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L456-L465)).

Read [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/STYLE.md](docs/STYLE.md) before you send a change. Read [AGENTS.md](AGENTS.md) before you use sh2o in another project. Use [sh2o issues](https://github.com/anzaldoivan/sh2o/issues) for bugs and hardware measurements.

# Acknowledgements

- **SH2ZAM Contributors:** [SH2ZAM](https://github.com/lamb2k/SH2ZAM), the Saturn maths library whose layout, build options and SH-2 methods sh2o follows.
- **Falco Girgis and SH4ZAM Contributors:** [SH4ZAM](https://github.com/gyrovorbis/sh4zam), the Dreamcast library that SH2ZAM adapts.
- **psx-spx:** the public PS1 hardware reference for the GTE.

See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).
