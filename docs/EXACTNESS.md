# Exactness and oracle tests

This guide tells how sh2o shows that a result is byte-exact. The `fixed` family is the sample. The GTE families use the same layout.

## The rule

A result is correct only when each of its bytes is equal to the reference bytes. The tolerance is zero. FLAG bits are part of the result.

## The flow

1. A reference writes a vector file: `tests/vectors/<family>.vec`.
2. `scripts/oracle_header.py` converts the vector file to `tests/<family>_oracle.h`.
3. `tests/test_<family>.c` runs each vector through the library and compares each result byte.
4. The same test runs a set of mutants. Each mutant must give a difference.

Git keeps the vector file and the header. CI writes both again and runs `git diff --exit-code` on them (`make oracles-check`). A difference stops the job.

For the sample family, the reference is `scripts/vectors_fixed.py`, a Python model of the contracts in `include/sh2o/fixed.h`. The model does not use the C source. For a GTE family, the reference is the PS1 reference that the project selects for it. A reference that is not a script writes the vector file directly. Then `make oracles` only writes the header again.

## Vector file format, version 1

The file is ASCII text with one item on each line. A line that starts with `#` is a comment.

| Line | Meaning |
|---|---|
| `sh2o-vec 1` | The format and its version. It is the first line that is not a comment. |
| `family <name>` | The family name. It gives the names in the header. |
| `op <name> in <fields> out <fields>` | One operation and the layout of its inputs and results. |
| `v <op> <in-hex> <out-hex>` | One vector: the input bytes and the result bytes. |

A field is `<name>:<type>` or `<name>:<type>[<n>]`. The types are `i8`, `u8`, `i16`, `u16`, `i32` and `u32`. The second form is an array of `<n>` values. `<n>` is a decimal number, 1 or more. An array field has `<n>` times the bytes of its type. For example, `regs:u32[64]` is 64 `u32` values, value 0 first, in 256 bytes. The fields are packed in order, with no padding. Each value is little-endian, as in PS1 memory. The converter makes sure that each vector has the byte count of its op line.

The inputs of an op have a maximum of 65535 bytes, and its results have the same maximum. A file has a maximum of 256 ops. If an op is larger or if there are more ops, the converter stops and gives the line number.

Example from `tests/vectors/fixed.vec`:

```
op lim_ir in value:i32 lm:u8 out r:i32
v lim_ir 0000008001 00000000
```

The input is `value` = -2147483648 and `lm` = 1. The result is 0.

## The oracle header

`tests/<family>_oracle.h` has one enum value per op, the op names, and an array of vectors. Each vector holds the op (8 bits), the byte counts (16 bits each), the input bytes and the result bytes. The header has no code. A freestanding SH-2 program can include it.

Do not edit the header. Change the vector file or its generator, then run `make oracles`.

## The test

The test decodes each input from its bytes with shifts. It calls the function and encodes the result to bytes with shifts. It does not copy host memory. Thus the bytes are the same on a little-endian host and on the big-endian SH-2.

At the first difference, the test prints the vector number, the op, the byte number, the input bytes, the expected bytes and the result bytes. Then it stops with a failure.

A private input is never in git. A test gets its path from an environment variable that names a path outside the repository: `SH2O_PS1_LIBGTE_OBJECTS` (the folder of libgte objects of a PS1 build), `SH2O_PS1_LINK_MAP` (the link map of that build), `SH2O_PS1_DATA` (the folder of data files of that build) and `SH2O_PS1_GTE_RECORDINGS` (the folder of PS1 GTE recordings). CMake registers the test with `sh2o_add_private_test`. If the variable is not set or the path is not there, the test prints why and stops with exit code 77, and CTest shows it as skipped. A path in the repository makes the test fail.

## Mutants

A mutant is a variant of a function with one known bug. The test runs each mutant through the same comparison. The test fails if a mutant gives no difference, because then the vectors do not find that bug.

The `fixed` mutants:

| Mutant | Bug |
|---|---|
| `mul_truncate` | The shift rounds toward zero. |
| `mul_saturate` | The product saturates and does not wrap. |
| `lim_ignore_lm` | The `lm` flag has no effect. |
| `big_endian_result` | The test writes the result bytes in big-endian order, not in the PS1 little-endian order of the vector files. |

Add a mutant for each bug that a family can have: rounding, saturation, a FLAG bit, an aliasing error and the byte order.

Two checks plant a fault in the same way. `global_names` adds a global function, a global variable and `main` to the library objects, and must find each one. `zam_copy` plants 6 lines copied from SH2ZAM, which it must find, and 5 lines, which it must not find.

## A big family: k

The `k` family (`include/sh2o/k.h`) has seven kernels. Its proof has four parts.

1. Oracle vectors. `scripts/vectors_k.py` is a Python model of the seven contracts. It writes approximately 500 hand-picked and edge vectors to `tests/vectors/k.vec`. A 64-bit result is two fields: the low word (`u32`), then the high word (`i32`). `tests/test_k.c` runs each vector through the public entry and through its C reference (`_c`). The test must give zero differences on both paths.
2. Bulk suite. `tests/suite/sh2o_k_test_suite.c` makes 113,471 vectors: 109,375 edge vectors, then 4,096 random vectors from a fixed seed. It computes an FNV-1a 32 digest of the results of four kernels and compares each digest with its pinned value in `tests/suite/sh2o_k_test.h`. It also compares two digests of the first 15,625 vectors. The suite uses no C library, so a program on the SH-2 can also compile it. `make cross` compiles it for the SH-2.
3. Bulk check. `tests/k_bulk_check.py` makes the 113,471 vectors again with the Python model. It compares each line of `test_k --dump` with the line that it expects, and it computes the six pinned digests from the model.
4. Mutants. For each mutant, `tests/mutant.py` copies `src/` and `include/` into the build folder and changes one text in the copy of the C references. The old text must occur exactly one time, or the build fails. CMake builds `test_k` and the copied `src/*.c` with the copy. The test runs with `--mutant <name>`, prints the count of oracle vectors that differ, and passes only when the count is more than 0.

| Mutant | Bug |
|---|---|
| `shift_11` | The translation is shifted left by 11, not 12. |
| `shift_13` | The translation is shifted left by 13, not 12. |
| `dropped_sign_extension` | The high word of a negative translation has no sign bits. |
| `swapped_index` | The third product uses the second element of the vector. |
| `lost_carry` | The carry out of the low word is lost. |

`make test` runs each test that does not have the label `proof`. `make proofs` runs all tests. Give the label `proof` to a test that runs for more than 60 seconds. Today no test has this label.

A check of private inputs can also be a script. Register it with `sh2o_add_private_test(<name> COMMAND <command>...)`. CMake gives no argument to the command, so put each argument in the command. `tests/private_inputs.py` is the script form of `tests/private_inputs.c`. Its second test runs CTest on it three times and must see Skipped, Skipped and Failed.

## A big family: gte

The `gte` family (`include/sh2o/gte.h`) has one register image and 14 commands. [GTE.md](GTE.md) gives all its sources and counts. Its vector file `tests/vectors/gte.vec` has two ops:

```
op cmd in regs:u32[64] cmd:u32 out rc:i32 regs:u32[64]
op write in regs:u32[64] reg:u32 val:u32 out regs:u32[64] rd:u32
```

`regs` in the input is 64 register words, register 0 first. The test writes them with `sh2o_gte_write` in the order `sh2o_gte_order` on a zeroed image. That order has every register but 15, 29 and 31, and it writes 28 before IR1-3. For `cmd`, the test calls `sh2o_gte_command` and compares the return code and the 64 reads of `sh2o_gte_read`. For `write`, it does one more write of `val` to `reg`, then compares the 64 reads and the read of `reg`.

The mutants work as in the `k` family: `tests/mutant.py` plants each bug in a copy of `src/` and `include/`. Each of the 8 gte mutants must give an oracle-vector difference. `tests/gte_sources_check.py` also pins the count of cases that each mutant changes.

## A big family: psyq

The `psyq` family (`include/sh2o/psyq.h`) has 19 PSY-Q `libgte` bodies. [PSYQ.md](PSYQ.md) gives all its sources and counts.

The oracle is the PS1 library code itself. The tests run the code bytes of each body from a private PS1 build in a Python R3000 model, `scripts/psyq_model.py`, with the GTE commands of `scripts/gte_model.py`. The code is never in the repository, so the family has no `.vec` file: a vector file would hold the results of that code. The three variables `SH2O_PS1_LIBGTE_OBJECTS`, `SH2O_PS1_LINK_MAP` and `SH2O_PS1_DATA` name the build. The five private tests skip when a variable is not set.

The public checks are the suite, the table check and the mutants. The suite `tests/suite/sh2o_psyq_test_suite.c` makes 1,216 cases and compares their digest with its pin. It also has 6 `ratan2` known answers. The private test `psyq_model_suite` computes the same digest from the PS1 code, so the pin comes from the oracle. `psyq_table` compares the `ratan2` table with its formula. Each of the 19 psyq mutants must make the suite fail. The suite also counts the calls of the break hook: over the seeded cases the count must be 0, and the test `psyq_plant_break`, with a planted zero divisor, must give a count more than 0.

Some checks against the PS1 code are now public. They use psyz at its pin (`scripts/psyz_pin.txt`). The `ratan2` table of sh2o is equal to the table of psyz (test `psyz_table`). The test `psyz_ratan2_sweep` compiles the `ratan2` of psyz and compares it with sh2o on 17,834,154 pairs: 0 differences. The test `psyz_libgte` runs the known answers of psyz on sh2o: 115 pass, 1 differs and 109 do not run. In the case that differs, the model `scripts/gte_model.py` gives the result of sh2o. [PSYQ.md](PSYQ.md) gives the details.

The public oracle of the 19 bodies is the PSY-Q 4.7 job, `make psyq47` (CI job `4.7 archive = game libgte`). It is optional and needs the network. It gets the PSY-Q 4.7 archive at test time and checks its sha256. If the download fails, the job skips. The job proves two things. First, the code of each body in the 4.7 library is equal to the code of the private PS1 build (version byte 0x45, PSY-Q 4.5), apart from the version word and the relocation fields: 15 bodies are identical and 4 are equal after masking. Second, the 4.7 code, run in `scripts/psyq_model.py`, gives the same results as sh2o on all the seeded cases of the private checks, the suite digest and the `ratan2` pairs, and each of the 19 mutants gives its pinned count. The GTE commands of that run come from `scripts/gte_model.py`. The family still has no `.vec` file: its oracle is library code that the job gets at test time, and a vector file would hold the results of that code. The repository keeps only the digests of the bodies (`tests/psyq47/game_digests.txt`) and the seeds of the cases. [PSYQ.md](PSYQ.md#the-psy-q-47-job) gives the details.

The SH-2 model run has no oracle file for this family. It runs the suite from its digest: each program must give the pinned digest and the 6 known answers.

## A big family: list

The `list` family (`include/sh2o/list.h`) has one call, `sh2o_gte_mvmva_list`. [GTE.md](GTE.md#the-mvmva-list) gives its sources and counts. The oracle is the Python GTE model `scripts/gte_model.py`. `scripts/vectors_list.py` writes the 88 vectors of `tests/vectors/list.vec`, with one op:

```
op list in regs:u32[64] cmd:u32 n:u32 items:u32[32] out rows:u32[96] regs:u32[64]
```

The test writes `regs` in the gte suite's order on a zeroed image, as for `gte`. Then it calls the list on the n items (two words each; items n..15 are 0). It compares the 16 out rows (rows n..15 are 0) and the 64 reads of `sh2o_gte_read`. The suite also compares each list with its singles. Each of the 5 list mutants must give an oracle-vector difference.

## The delegate

The experimental SH2ZAM delegate ([ZAM.md](ZAM.md)) has no `.vec` file. Its oracle is sh2o's own code: the GTE core for `sh2o_zam_mac3` and `sh2o_zam_mvmva_list`, and the psyq bodies for `sh2o_zam_TransposeMatrix`, `sh2o_zam_SetFogNearFar` and `sh2o_zam_ratan2`. The suite `tests/suite/sh2o_zam_test_suite.c` runs each case with the delegate and without it, and the bytes must be equal.

With the option `SH2O_ZAM` on, every family runs again: `make test SH2O_ZAM=ON` on the host and `make sh2-model SH2O_ZAM=ON` on the SH-2 model. Each family must give its oracle bytes and pins as without the option.

The delegate's 5 mutants must make its suite fail (`zam_mutant_<name>`), not an oracle vector. The suite reaches each entry point. Mutants 4 and 5 are in `sh2o_zam_TransposeMatrix` and `sh2o_zam_SetFogNearFar`, which no vector reaches.

## The SH-2 side

On the host, the test runs the portable C path. `make cross` builds the SH-2 path. `make sh2-model` runs the SH-2 machine code in an SH-2 instruction model, the SH-2 interpreter of Yabause ([benchmarks/sh2-model](../benchmarks/sh2-model/README.md)). It builds a native and a portable program, and it compares all result bytes of the family's oracle header with the expected bytes, before it gives a cycle figure. A family is "exact on the SH-2" when this run passes for the native and the portable program. The cycle figures are MODELLED, not Saturn timings. This generation has no console run. [VALIDATION.md](VALIDATION.md) records each run.

The SH-2 build is reproducible. `tests/sh2/objects.sha256` holds the Yaul image digest and the sha256 of each SH-2 object, of both `libsh2o.a` and of each model ELF of both lanes. The file is generated: `make sh2-manifest` writes it again (it needs the SH2ZAM checkout, whose `model.ld` the ELFs link). `make cross` builds the same files and fails when a sha256 differs, a listed file is missing, a built file is not listed or the image is not the manifest's; without SH2ZAM it skips the ELF lines. A change to the code or the build regenerates the manifest in the same commit.
