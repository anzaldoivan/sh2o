/* Selection of the SH-2 path and the attribute macros of the public headers.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_CDEFS_H
#define SH2O_CDEFS_H

/* The SH-2 path needs the GCC SH target. SH2O_FORCE_PORTABLE keeps the C path
 * on the SH-2 too, so that a test can compare the two paths on one machine.
 * The CMake target sh2o gives SH2O_FORCE_PORTABLE and SH2O_REQUIRE_SH2 to its
 * consumers too (PUBLIC), so the inline kernels in a consumer take the path
 * of the library. */
#if defined(__SH2__) && !defined(SH2O_FORCE_PORTABLE)
#define SH2OI_NATIVE 1
#else
#define SH2OI_NATIVE 0
#endif

#if defined(SH2O_REQUIRE_SH2) && !SH2OI_NATIVE
#error "SH2O_REQUIRE_SH2 is set, but the compiler does not target the SH-2."
#endif
/* Marks a function of a header as static inline. The compiler may choose not to inline it. */
#define SH2OI_INLINE static inline
/* Marks a function of a header as static inline. GCC always inlines it; other compilers may not. */
#if defined(__GNUC__)
#define SH2OI_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define SH2OI_FORCE_INLINE static inline
#endif
/* Puts a table in its own section .rodata.<name>, for --gc-sections and a linker script; empty under PIC/PIE, where
 * pointer tables need relocations and so a writable section. */
#if defined(__GNUC__) && defined(__ELF__) && !defined(__PIC__)
#define SH2OI_RODATA(name) __attribute__((section(".rodata." #name)))
#else
#define SH2OI_RODATA(name)
#endif

/* Kernel entry and exit hooks (e.g. saving MACH/MACL around a kernel an interrupt handler shares). No-op on the
 * host; an image may define them before including sh2o. */
#ifndef SH2O_K_ENTER
#define SH2O_K_ENTER() ((void)0)
#endif
/* Kernel exit hook: the pair of SH2O_K_ENTER. No-op on the host. */
#ifndef SH2O_K_LEAVE
#define SH2O_K_LEAVE() ((void)0)
#endif

#endif
