# Fixed family model bridge

`bridge.c` is a small table of typed function pointers to `sh2o_mul_q12` and `sh2o_lim_ir`. `make sh2-model` links it with the SH-2 build of the library into one ELF for each build mode (native and portable).

The table has two functions:

- It keeps the two functions in the ELF.
- It makes the build fail when a signature in `include/sh2o/fixed.h` changes and the table does not.

The model harness in [`../sh2-model`](../sh2-model/README.md) reads the table from the ELF and makes sure that each entry is the address of the function.

License: MIT, as for all of sh2o. See [LICENSE](../../LICENSE).
