# Contributing to sh2o

Keep the MIT notice in [LICENSE](LICENSE), the credits in [NOTICE.md](NOTICE.md) and the first comment of each file. Do not change files in `external/`. Send changes to SH2ZAM's own code to [SH2ZAM](https://github.com/lamb2k/SH2ZAM).

To add a kernel or a family, also read [CONTRIBUTING-KERNELS.md](CONTRIBUTING-KERNELS.md).

## Contracts

Each public function states these items in its header, as sentences:

- Range: the inputs for which the result is defined.
- Rounding: where the function rounds or truncates, and in which direction.
- Saturation: which values saturate or wrap, and to which limits.
- FLAG bits: the GTE FLAG bits that the function sets, and when.
- Aliasing: which outputs can share memory with which inputs.
- Failure: the result for each input outside the range.
- Hardware assumptions: the CPU, register and memory state that the function needs.

## Exactness

A result must be byte-exact against the PS1 reference. The tolerance is zero. Test each result byte and each FLAG bit. See [docs/EXACTNESS.md](docs/EXACTNESS.md).

Keep a portable C path beside each SH-2 kernel. The two paths must give the same bytes.

Add a test for each correctness bug. Tests must also run in Release builds, so do not use `assert`.

## Before you send a change

Run these targets. All must pass:

```sh
make check                 # style, SH2ZAM pin, oracle headers
make test CC=gcc
make test CC=clang
make sanitize              # ASan and UBSan
make strict                # warnings stop the build
make portable              # SH2O_FORCE_PORTABLE=ON
make cross                 # SH-2 cross build in the pinned Yaul image (needs Docker)
```

`make ci` runs all of them. The GitHub workflows in `.github/workflows/` run the same targets.

## Speed claims

Each speed claim states its source. Only a console measurement is a hardware claim. Label an emulator figure as "emulator", and give the emulator and its version. A desktop benchmark or an instruction count is not a console speed.

## Rules for library code

- Do not call the C library. Do not use floating point. Do not allocate memory.
- Do not use the SH-2 divider unless the function name and contract say so.
- Do not keep hidden global state. The caller owns all state.
- Do not do hidden cache or DMA work.
- Do not add approximate modes. A result is exact or the function does not exist.

## Style

[docs/STYLE.md](docs/STYLE.md) gives the code and comment style. `make check` runs `scripts/style_check.py`, which checks it.
