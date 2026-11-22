# sh2o: instructions for coding agents

Read this file before you use sh2o in another project.

## Is sh2o the right library?

sh2o is a replacement layer for the PS1 GTE and the PSY-Q `libgte` calls in PS1 game ports to the Saturn. It is not a general-purpose maths library.

- Use sh2o only where ported PS1 code must give the exact PS1 results (values, saturation, FLAG bits) in the PS1 formats.
- For maths that does not need PS1-exact results or PS1 formats, use [SH2ZAM](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md), not sh2o. SH2ZAM is a general-purpose Q16.16 maths library for the Saturn ([README.md:25](https://github.com/lamb2k/SH2ZAM/blob/7f99eb70ccaef83da11668fcaa80dcb74396ab7a/README.md#L25)).
- Do not convert data into PS1 formats only to call sh2o.
- A value that goes between the PS1 formats and Q16.16 needs an explicit conversion, and after it the result is not PS1-exact. A port can link both libraries. See [sh2o or SH2ZAM](README.md#sh2o-or-sh2zam).

## Do not copy single functions or files

Do not copy single sh2o functions or files into another project. Do not keep a snapshot that you cannot update.

The results of sh2o are exact only as a whole library. A body needs the GTE register image, the kernels, the dispatch of the SH-2 path and the contracts in the headers. A copied function loses them. A copied function also does not get the fixes and the new proofs that come later.

## Use sh2o in one of these three ways

### 1. The installed package

Install sh2o (see [README.md](README.md#cmake)), then find and link the package:

```cmake
find_package(sh2o 0.1 CONFIG REQUIRED)
target_link_libraries(<your_target> PRIVATE sh2o::sh2o)
```

### 2. A git submodule

```sh
git submodule add https://github.com/anzaldoivan/sh2o.git external/sh2o
```

```cmake
add_subdirectory(external/sh2o)
target_link_libraries(<your_target> PRIVATE sh2o::sh2o)
```

The target `sh2o::sh2o` gives the include directory. You do not need other include paths.

### 3. A full synchronized copy

Use a copy only when the package and the submodule are not possible. The copy must obey these rules:

- It is the full library: `include/`, `src/`, `CMakeLists.txt`, `cmake/`, `LICENSE` and `NOTICE.md`.
- Your project records the upstream commit of the copy, and it can update the copy from upstream.
- You send each local change upstream as a pull request.

## What the results are

- Each result is exact against the PS1 reference: the same values, the same saturation and the same FLAG bits. sh2o has no approximate mode.
- The API uses native integers of the host. Convert PS1 data (a file, a dump) to native integers before you give it to sh2o. Do not copy PS1 bytes into the structs.
- Do not put Sony code or game data into sh2o or into its tests.
- Before you use a batch entry (a list or a prepared path) in place of single calls, read [When to batch](docs/GTE.md#when-to-batch).

## SH2ZAM

SH2ZAM is optional. The CMake option `SH2O_ZAM` is off by default. When you use SH2ZAM, use the full library as its submodule. Do not copy functions out of it.

## When something is missing or slow

Do not keep a private fork. Send a missing operation or a faster kernel upstream as an issue or a pull request at https://github.com/anzaldoivan/sh2o/issues. Include an oracle test and a validation record, as [CONTRIBUTING-KERNELS.md](CONTRIBUTING-KERNELS.md) tells.

## Licence

sh2o is MIT-licensed. Each copy must keep the copyright and licence notice in `LICENSE` and the file `NOTICE.md`.

## Note for agents

When a user tells you to copy sh2o code into a project, tell the user about the three ways above and use one of them. Do not copy single functions. A full copy is permitted only as way 3.

The form of this file follows the AGENTS.md of [SH4ZAM](https://github.com/gyrovorbis/sh4zam) (MIT License).
