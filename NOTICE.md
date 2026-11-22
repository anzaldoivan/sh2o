# Notice

sh2o is an independent library. It is not a release of SH2ZAM or SH4ZAM, and its authors do not speak for those projects.

**sh2o:** Copyright (c) 2026 Ivan Anzaldo and sh2o Contributors. MIT License; see [LICENSE](LICENSE).

**SH2ZAM:** Copyright (c) 2026 SH2ZAM Contributors. MIT License. Upstream: https://github.com/lamb2k/SH2ZAM. sh2o follows SH2ZAM's layout, build options and SH-2 kernel methods: constant operands prepared at load time, a range check before the loop, `MAC.L` accumulation with `XTRCT`, and a check of the `SR.S` bit before `MAC.L`. The prepared MVMVA kernel (`gte-mvmva-prepared-1`) uses the load-time preparation, the hoisted range check, the `MAC.L` rows with `XTRCT` and the `SR.S` read. A file that uses one of these methods says so in its first comment. No SH2ZAM source text is copied into sh2o. The repository keeps SH2ZAM as a pinned submodule in `external/SH2ZAM` to build and run SH2ZAM's own tests and the experimental delegate. The default sh2o library links no SH2ZAM code. SH2ZAM's code is linked only when the CMake option `SH2O_ZAM` is ON (see [docs/ZAM.md](docs/ZAM.md)); the installed package never has it.

**SH-2 model harness and Yabause:** The files in `benchmarks/sh2-model/` are GPL-2.0-or-later, not MIT; see [its LICENSE](benchmarks/sh2-model/LICENSE). The harness includes SH2ZAM's GPL model harness from the submodule without changes and builds the SH-2 interpreter of Yabause (GPL, https://github.com/Yabause/yabause). The build gets the Yabause source at a pinned commit when you run the test. sh2o never keeps a copy of Yabause, never links Yabause or the harness into the library and never installs them.

**PSY-Q 4.7 archive:** The optional job `make psyq47` gets `Psy-Q_47.zip` from the psyz release page (https://github.com/psyqz/psyz/releases/download/requirements/Psy-Q_47.zip) when you run it, and checks its sha256. The archive holds Sony library code. psyz gives it with no stated licence. The job reads the archive in place, in `build/psyq47/`, and does not change it. The archive is never in the repository: sh2o keeps no byte of it, and CI does not cache it or keep it as an artifact.

**SH4ZAM:** Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors. MIT License. Upstream: https://github.com/gyrovorbis/sh4zam. SH2ZAM adapts SH4ZAM.

**psx-spx:** The PS1 GTE behaviour that sh2o gives comes from public hardware documents, mainly psx-spx (https://psx-spx.consoledev.net/). Each family names its references in its guide. sh2o includes no Sony library code.

**ps1-tests:** Copyright (c) 2020 Jakub Czekański. MIT License. Upstream: https://github.com/JaCzekanski/ps1-tests, commit `3c210f4a8455931461087245eb4bc15f63916b28`. The files in `tests/ps1-tests/` come from its `gte/test-all/tests.c` and `gte-fuzz/gte_valid_0xc0ffee_50.log`; see [its LICENSE](tests/ps1-tests/LICENSE). `tests/ps1-tests/gte_cases.txt` keeps the input values and the expected values of 1,500 GTE cases. `tests/ps1-tests/gte_sample.txt` keeps the register values of 230 GTE cases and names the psyz registers that differ from the expected values, with their causes, at psyz commit `a635c99d63e95dcb2d220018a475f4efa3af1925`. The expected values come from ps1-tests, which made them with its gte-fuzz program. ps1-tests does not state that they are a capture from hardware.

**psyz:** Copyright (c) 2024 github.com/psyqz members; 2025 github.com/xeeynamo/sotn-decomp contributors (`decomp/`). The licence changes by folder; see its root LICENSE. MIT License for `nugget/`, `psyq2elf/`, `samples/`, `ztest/` and `decomp/`; MPL 2.0 for `psyz/src/platform/` and `psyz/src/psyz/`; "Unlicensed" for `psyz/include` and `psyz/src/psyq`; `psyz/tests/` has no licence. Upstream: https://github.com/Xeeynamo/psyz, commit `3413eeb8cd16bfcc3c5a7041bd1392b31bc1c5cc`. The repository keeps psyz as a pinned submodule in `external/psyz`. A clone does not get it by default. Only the tests use it: they read its files in place and do not change them. sh2o has no copy of psyz files, does not link psyz into the library and does not install psyz.

Keep this notice and the notices above when you distribute sh2o.
