# Code and comment style

This guide gives the sh2o code style. `scripts/style_check.py` checks it, and `make check` and CI run that script. SH2ZAM has no written style. sh2o uses the form that most SH2ZAM files use. Where SH2ZAM files do not agree, the majority form is the rule.

The check reads only the sh2o files: `include/`, `src/`, `tests/`, `examples/`, `benchmarks/`, `scripts/`, the documents in `docs/` and the public files at the root.

## The banner

Each C file starts with this 5-line banner:

```c
/* <Title of the file>.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
```

Line 1 is a short title that ends with a full stop. Line 4 credits SH2ZAM and SH4ZAM. Tests and examples use the same banner. The GPL files in `benchmarks/sh2-model/` start with `/* SPDX-License-Identifier: GPL-2.0-or-later`, then ` * <Title>.`, then ` * Copyright (c) 2026 sh2o Contributors.`. A script in `scripts/` has the copyright line in its first lines.

## Comments

- Use `/* */` only. Do not use `//`.
- Doc comments are only in public headers (`include/`). Use `/** \brief <text> */` before an item and `/**< <text> */` after a struct member. Do not use other Doxygen tags.
- Write a contract as sentences: the range, the rounding, the saturation, the FLAG bits, the aliasing, the failure result and the hardware assumptions.
- A comment in a source file tells why, not what. Give a manual reference with its name, revision and section.
- Do not write TODO, FIXME or XXX.

## Names

- Public functions and types: `sh2o_`. Public macros: `SH2O_`.
- Internal functions and macros: `sh2oi_` and `SH2OI_`. A `static` function in `src/` is internal.
- A function that a public header gives as `static inline` is public and uses `sh2o_`: the entry (`sh2o_k_dot3b`), its C reference (`sh2o_k_dot3b_c`) and its SH-2 kernel (`sh2o_k_dot3b_w`). A helper that only these functions call is internal and uses `sh2oi_`.
- A macro that a consumer sets or reads is public. A macro that only the sh2o files read is internal, also in a public header. `SH2OI_NATIVE` is the only macro that selects the SH-2 path. `SH2O_FORCE_PORTABLE` and `SH2O_REQUIRE_SH2` are the public switches that control it.
- Do not define a macro that no code reads.
- A hook is a public macro that a consumer can define before it includes the header, for example `SH2O_K_ENTER()` and `SH2O_K_LEAVE()`. The default of a hook is empty. The header gives its contract. The proofs apply to the empty default.
- A function that has the contract of a PSY-Q library function keeps the PSY-Q spelling after the prefix: `sh2o_psyq_SetRotMatrix`.
- Header guard: `SH2O_<NAME>_H`, where `<NAME>` is the file name in capitals without `sh2o_`. `sh2o.h` uses `SH2O_H`.
- Files: `src/sh2o_<module>.c`, `src/sh2o_<module>_internal.h`, `include/sh2o/<family>.h`, `include/sh2o/<family>_experimental.h` for a family that no default path uses, `tests/test_<family>.c`, `tests/<family>_oracle.h`.
- `include/sh2o/cdefs.h` is not a family. It holds the selection of the SH-2 path and the attribute macros that the public headers share.
- A family of `static inline` functions has its bodies in two sub-headers: `include/sh2o/inline/sw/<family>_sw.inl.h` for the C references and `include/sh2o/inline/sh2/<family>_sh2.inl.h` for the SH-2 kernels. Only the family header includes them. The guard of `<name>.inl.h` is `SH2O_<NAME>_INL_H`.
- A test source that a consumer also compiles into its program is `tests/suite/sh2o_<family>_test_suite.c`, with the header `tests/suite/sh2o_<family>_test.h`. Its entry points are `sh2o_<family>_test_<name>` and its macros use `SH2O_`. It obeys the rules of library code. It is not in the library and it is not installed.

## Layout

- Indent with 4 spaces. Do not use tabs. Do not put spaces at the end of a line.
- Write `if(`, `for(`, `while(` and `switch(`, with no space before the parenthesis.
- Put the `{` on the line of the function or the statement.
- Do not put a space after a comma: `sh2o_mul_q12(sh2o_q12_t a,sh2o_q12_t b)`. Inline assembly operand lists are an exception.
- Put the `*` next to the name: `const uint8_t *bytes`, not `uint8_t* bytes`.
- Short expressions have no spaces around operators: `(int64_t)a*b`.
- A public header includes `<stdbool.h>`, `<stddef.h>` and `<stdint.h>` first, then its sh2o headers, then the `extern "C"` block.

## Inline assembly

- Write `__asm__ volatile(`.
- Put one instruction in each string, one string on each line: `"dmuls.l %[a],%[b]\n\t"`.
- Use named operands. Give exact constraints: `"+&r"` for a pointer that the code increments, `"=&r"` for a scratch register, `"m"` for memory that the code reads.
- When the code uses MACH or MACL, put `"mach","macl"` in the clobbers.
- `MAC.L` and `MAC.W` use the SR.S bit. Before them, read SR (`stc sr`) and check S, or set it (`clrs`, `sets`), as the contract tells.
- Gate the assembly on `SH2OI_NATIVE` and keep a portable C path that gives the same bytes.

## Library code

Library code is the code in `include/` and `src/`.

- Do not call the C library. Include only `<stdbool.h>`, `<stddef.h>` and `<stdint.h>`.
- Do not use floating point.
- Do not allocate memory.

`make cross` also makes sure that the SH-2 objects need no `memcpy`, `memset` or `memmove`. The compiler can add such calls for a struct copy, so copy such structs one field at a time.

## Documents

Write the documents in ASD-STE100 Simplified Technical English: short sentences, active voice, one instruction in each sentence, and one meaning for each word. Do not use em dashes or emoji.
