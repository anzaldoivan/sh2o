# CMake toolchain file for the Saturn SH-2 (big-endian, bare metal).
# Copyright (c) 2026 sh2o Contributors. MIT License.
# Use a bare-metal SH-2 GCC such as Yaul's sh2eb-elf-gcc. A Linux SH-4 GCC is
# not a replacement: it can refuse -m2.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh2)
set(SH2O_SH2_PREFIX sh2eb-elf- CACHE STRING "Prefix of the SH-2 tools, with the trailing dash")
set(CMAKE_C_COMPILER ${SH2O_SH2_PREFIX}gcc)
set(CMAKE_AR ${SH2O_SH2_PREFIX}ar)
set(CMAKE_RANLIB ${SH2O_SH2_PREFIX}ranlib)
set(CMAKE_C_FLAGS_INIT "-m2 -mb -O2 -ffreestanding")
# There is no target C library, so CMake checks the compiler with a library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
