# Benchmarks

This folder will hold speed measurements of the sh2o kernels. It has no console or emulator measurement yet. It has MODELLED figures.

[`sh2-model`](sh2-model/README.md) gives MODELLED cycle counts from an SH-2 interpreter (`make sh2-model`). These are emulator figures, not measurements.

Each result must give its source:

- **Console:** measured on a Saturn. Give the console model and the method.
- **Emulator:** measured in an emulator. Give the emulator and its version. This is not a hardware speed.
- **Host:** measured on a desktop computer. This is not a Saturn speed.
- **Modelled:** counted in an SH-2 instruction model (`make sh2-model`). The model has flat RAM. It has no caches, no wait states and no bus contention, and it uses a stand-in for the divider unit. This is not a Saturn speed.

## Modelled: `fixed`

Source: `make sh2-model`, the SH-2 interpreter of Yabause `7e38821d`, SH2ZAM `7f99eb70`, sh2eb-elf-gcc 13.2.0 at `-O2`. The figures are for one call, from the function entry to its return. All 783 vectors are exact before a figure is given.

| Program | Function | Cycles min | Cycles max | Cycles mean | Instructions mean | Vectors |
|---|---|---|---|---|---|---|
| native | `sh2o_mul_q12` | 13 | 13 | 13.00 | 11.00 | 617 |
| native | `sh2o_lim_ir` | 12 | 15 | 13.37 | 10.46 | 166 |
| portable | `sh2o_mul_q12` | 20 | 27 | 23.14 | 20.69 | 617 |
| portable | `sh2o_lim_ir` | 12 | 15 | 13.37 | 10.46 | 166 |

`sh2o_lim_ir` has one C body for the two programs, so its figures are the same.

An instruction count is not a speed.

## Modelled: `k`

The figures are in [`k-model/README.md`](k-model/README.md).

## Modelled: `gte`

MVMVA on its kernel path (21 oracle vectors, `SR.S` = 0): mean cycles of one `sh2o_gte_mvmva` call 400.29 (native) and 476.29 (portable); of the kernel `sh2o_k_mvb3` through the bridge's wrapper 133.00 and 210.00. Overhead: 267.29 (66.8%) and 266.29 (55.9%). The figures are in [`gte-model/README.md`](gte-model/README.md).

## Modelled: `psyq`

The figures are in [`psyq-model/README.md`](psyq-model/README.md).

## Modelled: `list`

The MVMVA list (`sh2o_gte_mvmva_list`) and the single `sh2o_gte_mvmva`, over the 88 oracle vectors. All vectors are exact before a figure is given. Mean cycles of one list item: 519.30 (native) and 559.02 (portable). Mean cycles of one single: 482.11 (native) and 522.35 (portable). The list item figures include the call of the list and its loop. On the MVMVA kernel path (173 items): whole 396.08 and 472.08, kernel 133.00 and 210.00, overhead 263.08 (66.4%) and 262.08 (55.5%). The figures are in [`list-model/README.md`](list-model/README.md).

## Modelled: `zam`

The experimental SH2ZAM delegate, only with `make sh2-model SH2O_ZAM=ON`. The delegate stays off by default. In this model, the delegate takes more cycles than the GTE core for each command. Mean cycles of one `sh2o_gte_command` call in the native program, delegate against core: MVMVA 1517.92 and 522.38, RTPS 1750.04 and 753.53, RTPT 5051.23 and 2065.73. The figures are in [`zam-model/README.md`](zam-model/README.md). [docs/ZAM.md](../docs/ZAM.md) describes the delegate.

## Modelled: `dpct32`

The figures are in [`dpct32-model/README.md`](dpct32-model/README.md).

## Modelled: `prepared`

MVMVA with a prepared shadow (`sh2o_gte_mvmva_prepared`) against `sh2o_gte_mvmva`, with the load of the shadow, on the 21 kernel-path vectors. The figures are in [`prepared-model/README.md`](prepared-model/README.md).

## Modelled: `listprep`

The prepared MVMVA list (`sh2o_gte_mvmva_list_prepared`) against `sh2o_gte_mvmva_list`, its load included, at n = 1..8 and over the list vectors. The figures are in [`listprep-model/README.md`](listprep-model/README.md).

## Tools: the GTE budget and the bench verdict

[`scripts/gte_budget.py`](../scripts/gte_budget.py) calculates a per-call SH-2 cycle budget for each GTE command and function. It reads two files:

- **Config (TOML):** `frame_ms`, `share`, `share_reason`, `clock_mhz`, and a `[weight, basis]` pair for each name in `[commands]` and `[functions]`.
- **Census (JSON):** a `gte` object with `frames` and, for each command and function, its largest count in one frame (`max`) and that frame (`max_frame`).

`k = share x frame_ms x clock / sum(max x weight)`. The budget of a name is `k x weight`. Each name in the census must have a weight and each weight must have a census name, or the script stops with an error. A weight must be a positive integer.

```sh
python3 scripts/gte_budget.py --config <budget.toml> --census <report.json> [--json <out.json>] [--markdown]
```

The script needs Python 3.11 or later (`tomllib`). The CTest `gte_budget` checks it on synthetic inputs.

[`sh2o_bench.h`](sh2o_bench.h) and [`sh2o_bench.c`](sh2o_bench.c) compare a candidate function with its C reference. They are not in the library and they are not installed. A program compiles them in.

- The program supplies the clock as a `sh2o_bench_tick` function, for example the SH-2 free-running timer. The files have no timer code.
- `sh2o_bench_run` calls the reference and the candidate alternately for a number of rounds. It keeps the lowest tick count of each.
- `sh2o_bench_classify` gives `SH2O_BENCH_BETTER`, `SH2O_BENCH_NOISE` or `SH2O_BENCH_WORSE`. The noise band is `ref x noise_pct / 100`. Set `noise_pct` before the run. A candidate is better only when it is below the reference by more than the band.

The CTest `bench` (`tests/test_bench.c`) checks the two band edges and the minima of `sh2o_bench_run`.
