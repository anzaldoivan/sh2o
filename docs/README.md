# sh2o guides

sh2o is a replacement layer for the PS1 GTE and `libgte` in PS1 game ports to the Saturn, not a general-purpose maths library; for maths that does not need PS1-exact results, use SH2ZAM (see [sh2o or SH2ZAM](../README.md#sh2o-or-sh2zam)).

The overview, the build commands and the API list are in the [root README](../README.md).

| Guide | Contents |
|---|---|
| [STYLE.md](STYLE.md) | The code and comment style, and its check. |
| [EXACTNESS.md](EXACTNESS.md) | Byte-exact oracle tests: vector files, oracle headers, mutants. |
| [GTE.md](GTE.md) | The GTE register model, the commands and the MVMVA list. |
| [PSYQ.md](PSYQ.md) | The PSY-Q `libgte` calls. |
| [COUNT.md](COUNT.md) | The call counters: the option `SH2O_COUNT`, what they count, a census. Off by default. |
| [ZAM.md](ZAM.md) | The experimental SH2ZAM delegate: the option, its proof and its tests. Off by default. |
| [KERNELS.md](KERNELS.md) | The SH-2 kernels and their portable paths. |
| [SH2ZAM_MAP.md](SH2ZAM_MAP.md) | For each operation, the SH2ZAM guide, kernel and model suite that are related to it. |
| [VALIDATION.md](VALIDATION.md) | Dated records of what was tested, and what was not. |
| [Contributing kernels](../CONTRIBUTING-KERNELS.md) | How to add a kernel or a family. |
| [Benchmarks](../benchmarks/README.md) | Speed measurements and their sources. |
| [Examples](../examples/README.md) | Examples that you can run. |
