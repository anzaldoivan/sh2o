# Validation record: 2026-11-21 (host, zam-ratan2-1)

Function `sh2o_zam_ratan2` in `sh2o/zam_experimental.h`: a candidate for the body `sh2o_psyq_ratan2` (record [psyq-ratan2-1](psyq-ratan2-1.md)) through `sh2z_atan2` of SH2ZAM. Its status is refused: it does not give the results of the body. No body calls it. The delegate is described in [ZAM.md](../ZAM.md). The other records are listed in [VALIDATION.md](../VALIDATION.md).

- **Pins:** SH2ZAM `7f99eb70`, Yabause `7e38821d`, sh2eb-elf-gcc 13.2.0 in the pinned Yaul image (`ijacquez/yaul@sha256:b6d27220…`). Host compilers: GCC 16.2.1 and Clang 23.1.6.
- **Host, exact:** `make test SH2O_ZAM=ON` passes with GCC and with Clang. The test `zam` compares the candidate with `sh2o_psyq_ratan2` (4,096 per turn) and pins each count. Grid (y and x in -2,048 to 2,048): 2,342,014 of 16,785,409 cases differ, the largest difference is 1. Xorshift: 146,687 of 1,048,576 cases differ, the largest difference is 1. Edges: 37 of 169 cases differ, the largest difference is 2,048. The test fails when no case differs, because the status is refused. These cases are not in the digest of the suite. This function has no mutant.
- **The option:** `SH2O_ZAM` on (`make test SH2O_ZAM=ON`). Without the option the function does not exist.
- **SH-2 cross build:** `make cross`. `tests/sh2/zam_static.txt`: in the default build `src/sh2o_zam.c` has no symbol and no instruction. The object with the option on has no static check.
- **SH-2 model, exact:** not done. `make sh2-model SH2O_ZAM=ON` does not run this comparison.
- **SR.S:** not measured. The model does not run this function.
- **Planted difference:** none for this function. The test `zam` checks each count against its pin.
- **MODELLED cycles:** none. The model does not time this function. The figures of the delegate are in [benchmarks/zam-model/README.md](../../benchmarks/zam-model/README.md).
- **Not done:** a run on the SH-2 model; a console run; a domain where the candidate gives the results of the body.
