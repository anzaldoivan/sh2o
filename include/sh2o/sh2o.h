/* sh2o: PS1 GTE and libgte replacement layer for PS1 ports to the Saturn, not a general maths library (for that, SH2ZAM).
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */
#ifndef SH2O_H
#define SH2O_H
#include <stdbool.h>
#include <stdint.h>
#include "sh2o/fixed.h"
#include "sh2o/gte.h"
#include "sh2o/k.h"
#include "sh2o/list.h"
#include "sh2o/psyq.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Version of the sh2o API, as major.minor.patch. */
#define SH2O_VERSION "0.1.0"

/** \brief Gives the name of the code path that the library was built with.
 *
 *  The result is "sh2" when the library uses the SH-2 inline assembly, and
 *  "portable" when it uses the C path. Both paths give the same results. The
 *  string is static and the caller does not free it. */
const char *sh2o_backend(void);

#ifdef __cplusplus
}
#endif
#endif
