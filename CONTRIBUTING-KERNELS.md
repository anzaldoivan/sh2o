# Contributing kernels and families

This guide tells how to add a family or an SH-2 kernel. Read [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/STYLE.md](docs/STYLE.md) first.

The rules follow the kernel guide of SH2ZAM (`external/SH2ZAM/saturn/CONTRIBUTING-KERNELS.md`) by analogy. The words, the files and the tools are those of sh2o. SH2ZAM itself is used whole and unmodified wherever its contract fits; a kernel of sh2o's own exists only where SH2ZAM's contract cannot give the result. No SH2ZAM source is copied into sh2o's files, as SH2ZAM's NOTICE.md asks (whole-library use, no snippets); a fix to SH2ZAM goes upstream.

In the paths below, `<family>` is the name of the family (`fixed`), `<name>` is the name of a kernel that has its own files, and `<tag>` is the contract tag.

## The contract tag

Each kernel has one contract tag. The tag names one contract: the results, the ranges, the FLAG bits and the hardware assumptions.

- The form is `<name>-<n>`, in small letters, where `<n>` starts at 1. An example is `mvmva-prepared-1`.
- Write the tag in the header, in a comment that starts the section of the kernel: `Contract tag <tag>`.
- Write the tag in the first comment of the kernel, in the title of its validation record and in its line of [docs/KERNELS.md](docs/KERNELS.md).
- A change of the contract is a new tag with new files. Do not change the results of a tag that has a validation record.

The functions of `fixed` are older than this rule and have no tag.

## New files only

A kernel adds files. It does not change the files of a different kernel. Two kernels can then be written at the same time without a conflict.

This rule also applies to a second kernel of an operation that has a kernel. The second kernel gets its own source file, its own test file and its own model suite. It does not go into the file of the first kernel.

A kernel does not change `CMakeLists.txt`, the `Makefile` or the CI workflows, because CMake finds the new files.

| File | Purpose | Found by |
|---|---|---|
| `include/sh2o/<family>.h` or `<family>_experimental.h` | The public API and its contracts, in a section for each contract tag. | Include it from `sh2o/sh2o.h` when it is stable. |
| `src/sh2o_<family>.c` | The portable C path and the first SH-2 kernel of the family. | CMake builds each `src/*.c`. `make cross` builds each `src/*.c` for the SH-2. |
| `src/sh2o_<name>.c` | A second kernel of an operation, with its own contract tag. | The same. |
| `tests/vectors/<family>.vec` | The reference vectors. | `make oracles`. |
| `scripts/vectors_<family>.py` | The reference model, if the reference is a script. | `make oracles`. |
| `tests/<family>_oracle.h` | Generated from the vector file. Do not edit it. | `make oracles`. |
| `tests/test_<family>.c` or `tests/test_<name>.c` | The byte-exact test with its mutants. | CMake registers the CTest `<family>` or `<name>`. |
| `benchmarks/<family>-model/bridge.c` and `README.md` | The model bridge (MIT). It keeps the functions in the SH-2 program. | The model harness. |
| `benchmarks/sh2-model/<family>_harness.c` | The model harness of the family (GPL-2.0-or-later). | `make sh2-model`. |
| `docs/validation/<tag>.md` | The validation record. | One line in [docs/VALIDATION.md](docs/VALIDATION.md). |
| `docs/<GUIDE>.md` | The guide of the family. Write a new guide. Do not add sections to the guide of a different family. | One line in `docs/README.md`. |

## Lines that you add to shared files

Add at most one line to each of these lists. Keep the order of the list. Do not change the other lines.

| File | Line |
|---|---|
| `include/sh2o/sh2o.h` | One `#include`, when the header is stable. |
| `tests/test_headers.c` | One `#include` of the new header. |
| [docs/KERNELS.md](docs/KERNELS.md) | One table row for each function, with its contract tag. |
| [docs/VALIDATION.md](docs/VALIDATION.md), list `Kernel records` | One line: the tag, the link to `docs/validation/<tag>.md`, the date and the scope. |
| [docs/README.md](docs/README.md) | One line for a new guide. |
| [docs/SH2ZAM_MAP.md](docs/SH2ZAM_MAP.md) | One row for an operation that is not in the map. |
| [benchmarks/README.md](benchmarks/README.md) | One line that points to the figures in `benchmarks/<family>-model/README.md`. |
| [NOTICE.md](NOTICE.md) | One line, if the kernel uses a method from SH2ZAM that the file does not name. |

## Files that are shared

These files are not found by a search. A change to one of them is a separate commit, and two kernels cannot change it at the same time.

The SH-2 scripts are not shared files: a family adds files, not lines. `scripts/sh2_model.sh` (`make sh2-model`) finds each family from `benchmarks/<family>-model/bridge.c`; the family must also have `benchmarks/sh2-model/<family>_harness.c` and `tests/<family>_oracle.h`, or the script stops with an error that names the family and the file. `scripts/sh2_cross.sh` (`make cross`) builds each `src/*.c`, and then runs each `tests/sh2/<family>_static.txt`. That spec file names a probe source (the family's `src/` file, or a probe file that calls the kernel the way a caller does) and the checks of its object code: no undefined symbol, the exact function names, no calls, the count of an instruction in a function, and forbidden instructions. The header of `scripts/sh2_cross.sh` gives the format.

- The harness of SH2ZAM (`external/SH2ZAM/saturn/benchmarks/sh2-model/harness.c`) belongs to SH2ZAM. sh2o does not change it. If a suite needs a new harness mode, ask SH2ZAM for it and move the pin.
- `tests/vectors/<family>.vec`, when a second kernel needs more vectors. New vectors are added for all the kernels of the operation.

## A section for each entry and each table

A consumer compiles with `-ffunction-sections -fdata-sections` and links with `-Wl,--gc-sections`. The linker then removes each function and each table that the program does not use. The compiler makes the sections for C code. Assembly code and tables need these rules:

- Put each global assembly entry in its own section, `.text.<symbol>`, with `.pushsection`. Close it with `.popsection`. Do not write a bare `.text` in a top-level `__asm__`.
- An SH-2 branch (`BRA`, `BSR`, `BT`, `BF`) cannot go to a different section. If two entries share code, write the code one time for each entry, or go to it with `JSR` or `JMP` through a literal address.
- Put a read-only table that the assembly code defines in its own `.rodata.<symbol>` section.
- Put a read-only C table in its own `.rodata.<symbol>` section with a section attribute, so that the table has its own section also when a consumer compiles without `-fdata-sections`.
- `make cross` makes sure that the SH-2 objects need no `memcpy`, `memset` or `memmove`. Copy a struct one field at a time.

## A test with its oracle header

1. Write the contract in the header first. Give the range, rounding, saturation, FLAG bits, aliasing, failure result and hardware assumptions.
2. Get the vectors from the reference. Write `tests/vectors/<family>.vec` in the format of [docs/EXACTNESS.md](docs/EXACTNESS.md). Put edge values in it: the limits of each range, zero, -1, the largest and smallest values.
3. Run `make oracles`. Commit the vector file and the header. Do not edit the header.
4. Write the portable C path. Make the test pass with zero differences.
5. Add mutants to the test. Each mutant must give a difference. If a mutant gives no difference, add vectors.

## Steps for an SH-2 kernel

1. Read the row of the operation in [docs/SH2ZAM_MAP.md](docs/SH2ZAM_MAP.md). Read the SH2ZAM guide that the row names, at the pinned commit. If the map has no row for the operation, add the row first.
2. Decide if SH2ZAM is linked for this operation. The section [SH2ZAM](#sh2zam) gives the conditions. If SH2ZAM is not linked, write the reason.
3. Write the first comment of the kernel. It is the comment that follows the banner of the file. It gives the contract tag, the SH2ZAM guide with its file and lines and the pinned commit, and one of these: the SH2ZAM function that the kernel links, or the reason why SH2ZAM is not linked. If the map says "none", the comment says that SH2ZAM has no guide for the operation.
4. Write the kernel. It must give the same bytes as the portable path.
5. Write the model suite and run `make sh2-model`.
6. Write the validation record.
7. Run all targets of CONTRIBUTING.md, `make cross` included.

An example of a first comment:

```c
/* Contract tag mvmva-prepared-1.
 * SH2ZAM guide: saturn/BATCH_TRANSFORMS.md:193-206 at 7f99eb70 (MAC.L rows,
 * XTRCT, SR.S). SH2ZAM is not linked: its transform is Q16.16 and it
 * saturates, and this kernel must give the GTE bytes.
 */
```

## Rules for an SH-2 kernel

- Gate the kernel on `SH2OI_NATIVE`. Keep a portable C path that gives the same bytes. The first kernel of a family has its portable path in the same file.
- Follow the inline assembly rules in [docs/STYLE.md](docs/STYLE.md).
- Do not use the SH-2 divider unless the function name and contract say so.
- `MAC.L` and `MAC.W` use the SR.S bit. A kernel that uses them must give the same bytes when SR.S is 0 and when SR.S is 1. The contract tells how: the kernel reads SR and uses a path without `MAC` when S is set, or the kernel saves SR, clears S and restores SR. The kernel must leave SR.S as it was.
- If the kernel uses a method from SH2ZAM, name the method in the first comment of the kernel and in [NOTICE.md](NOTICE.md). Do not copy SH2ZAM text or code.
- Do not claim a speed without a measurement. Label each figure with its source. See [benchmarks/README.md](benchmarks/README.md).

## The model suite

Each family has one suite on the SH-2 instruction model. The suite runs the kernel as SH-2 machine code in the SH-2 interpreter of Yabause, which the build fetches at its pinned commit.

- `benchmarks/<family>-model/bridge.c` is MIT. It is a table of typed pointers to the functions, so that the functions stay in the SH-2 program and a changed signature stops the build.
- `benchmarks/sh2-model/<family>_harness.c` is GPL-2.0-or-later, with the banner of [docs/STYLE.md](docs/STYLE.md). It is never linked into the library and never installed.
- The harness includes the harness of SH2ZAM from the pinned submodule, not changed: `#define main sh2_model_main`, then `#include` of `harness.c`, then `#undef main`. It then includes `tests/<family>_oracle.h`. Do not define a name that the harness of SH2ZAM defines at file scope.
- The harness runs each vector of the oracle header in the native program and in the portable program. It compares each result byte with the oracle before it prints a figure. One difference stops the run.
- For a kernel that uses `MAC.L` or `MAC.W`, the harness runs each vector two times: with SR.S 0 and with SR.S 1 at the call. The harness of SH2ZAM stops the run if SR.S, the stack or R8 to R14 are different after a call.
- The suite has a planted difference: one changed expected byte must stop the run.
- The figures are MODELLED cycles. The model has flat RAM, no caches, no wait states and no bus contention. Write the figures in `benchmarks/<family>-model/README.md` with the pins: SH2ZAM, Yabause and the compiler.

[`benchmarks/sh2-model/fixed_harness.c`](benchmarks/sh2-model/fixed_harness.c) and [`benchmarks/fixed-model/`](benchmarks/fixed-model/README.md) are the example.

## The validation record

Each contract tag has one record, `docs/validation/<tag>.md`. A record is a new file. A new run with a new date is a new section at the top of that file.

The first line is `# Validation record: <date> (<scope>, <tag>)`. The record then gives:

- **Pins:** the SH2ZAM commit, the Yabause commit and the compiler.
- **Host, exact:** the test, the number of vectors, the number of differences and the mutants that the test catches.
- **SH-2 model, exact:** the number of vectors in the native and the portable program, the SR.S values of the runs, and the result of the planted difference.
- **MODELLED cycles:** the figures, or a link to them.
- **Not done:** each type of run that does not exist, for example a console run.

Add one line to the list `Kernel records` in [docs/VALIDATION.md](docs/VALIDATION.md), in the order of the tags. The records of the repository as a whole (a pin change, a new CI job) stay as dated sections in docs/VALIDATION.md.

## A kernel beside an existing kernel

An operation can get a second kernel, for example a kernel that prepares the matrix one time at the load. These rules apply:

- The second kernel has its own contract tag, its own function name and its own files. The first kernel and the portable path do not change.
- The second kernel is exact on the host and on the model suite, on the same vectors as the first kernel. Its test includes the oracle header of the family. If it needs more vectors, the vectors are added to the family, and the first kernel runs them also.
- If it uses `MAC.L` or `MAC.W`, it runs on the model suite with SR.S 0 and with SR.S 1.
- The model suite gives the MODELLED cycles of the two kernels in one run, on the same vectors, with the same pins. If the second kernel moves work to a different call (for example to the load of a matrix), the figures include that call. The record then gives the ratio of calls to loads in the run, the ratio at which the two kernels are equal, and the ratio of the consumer's measured workload. A win at a ratio above the consumer's ratio is not a win.
- The second kernel becomes the default only on a MODELLED win: fewer MODELLED cycles than the first kernel in that run. Until then, the default path does not call it.
- The change of the default is a separate commit. It changes only the selection of the kernel. It adds a section to the validation record with the two figures. The first kernel stays in the library.
- A kernel that removes a hardware assumption of the first kernel (for example SR.S = 0) takes the place of the first kernel in the selection without a speed win. Its record gives its cost.

## SH2ZAM

SH2ZAM is the submodule `external/SH2ZAM`, at the commit in `scripts/sh2zam_pin.txt`. sh2o links it or builds it as it is. sh2o does not change it and does not copy its source into sh2o's files; a fix to SH2ZAM is proposed upstream.

- **The library does not link SH2ZAM by default.** SH2ZAM comes into the build only for an optional delegate, which is a build option that is off by default. The delegate takes SH2ZAM as SH2ZAM's README tells: `add_subdirectory(external/SH2ZAM)` and the CMake target `SH2ZAM::sh2zam`. It uses only the public API of SH2ZAM. The delegate is the CMake option `SH2O_ZAM`; the CTest `zam_isolation` and `make install-check` keep SH2ZAM out of the default build and the installed package ([docs/ZAM.md](docs/ZAM.md)).
- **The GTE-exact operations stay in sh2o.** SH2ZAM calculates in Q16.16, saturates, rounds down, and has no GTE operation. An SH2ZAM function is a candidate for an operation only after a test proves that it gives the same bytes as `libgte` for that operation. Without that test, sh2o uses its own code. The delegate must refuse each input for which the results are not proven equal.
- **The model suite includes the harness of SH2ZAM, not changed.** Each family harness of sh2o includes `harness.c` from the pinned submodule. The harness files are GPL-2.0-or-later and stay in `benchmarks/sh2-model/` with their own LICENSE.
- **The map.** [docs/SH2ZAM_MAP.md](docs/SH2ZAM_MAP.md) gives, for each sh2o operation, the SH2ZAM guide, kernel and model suite that are related to it. Read the row before you write a kernel.
- **The pin.** A change of the pin is a separate commit, with a new date in `scripts/sh2zam_pin.txt`. All tests run again, and the map is read again at the new commit.
