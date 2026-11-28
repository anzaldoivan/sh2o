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

## CI jobs

The workflows in `.github/workflows/` obey these rules. `scripts/check_ci.py` checks rules 1, 2, 3, 6 and 9. The review of a change checks rules 4, 5, 7, 8 and 10.

1. A job runs one make target. Each job, and each matrix row, has exactly one run step that calls `make`, with one goal. All other steps are setup: checkout, fetch, image pull, cache and log upload.
2. `make ci` runs exactly what the workflows run. Expand each matrix. Each make run of a job, with its variables and the job environment that the target reads, is one line of the `ci` recipe or one prerequisite of `make check`.
3. Job names are an interface. Each name in the maintainers' list of required check names is the name of exactly one job. All job names are different. If you change a job name, change the job and the list in the same commit.
4. Add a new gate to an existing job. Make a new job only if the gate needs the network, a different image or more than approximately 2 minutes.
5. A job that needs an input that the project cannot pin and keep (for example, the PsyQ archive) is optional. It is not a required check, and if the fetch fails, the job skips with a summary line. A pinned input that the job checks against its pin is a pinned fetch: if that fetch fails, the job fails.
6. Each job has a time limit (`timeout-minutes`). The workflow permissions are read-only, and no job gets write permission. Each image reference uses a digest (`@sha256:`): `container:`, `docker pull` and `SH2_IMAGE` in the workflows and in the Makefile.
7. The change that adds a job states if the job is a required check, and it adds the job's line to the `ci` recipe.
8. Build one time. Each build tree has one job. Another gate on that tree is a part of that job and its one make goal, never a second build.
9. Cache only pinned inputs. The cache key is a name and the pin (a commit, a digest or a sha256), as the source of the pin states it. After a restore, the job that uses the input checks the pin. Do not use `restore-keys`. Do not cache a path in `build/` or a build output that a gate examines (no ccache for gates). Do not cache Sony or game bytes: the PsyQ archive is never cached.
10. Fetch a pinned input one time in each job, and use it from a fixed path.
