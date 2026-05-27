/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file fgen_version.h
 * @brief Version number macros and the fgen_version() accessor function.
 */

#ifndef _FGEN_VERSION_H_
#define _FGEN_VERSION_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fgen_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode four version components into a single 32-bit integer.
 *
 * The resulting value is suitable for compile-time comparisons against
 * @c FGEN_VERSION.
 *
 * @param a  Year component.
 * @param b  Month component.
 * @param c  Minor component.
 * @param d  Release/patch component.
 * @return   Packed 32-bit version number: (a<<24 | b<<16 | c<<8 | d).
 */
#define FGEN_VERSION_NUM(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

/**
 * @brief Packed 32-bit version number for this build of FGEN.
 *
 * Compare against a value produced by @c FGEN_VERSION_NUM() to perform
 * compile-time version checks.
 */
// clang-format off
#define FGEN_VERSION FGEN_VERSION_NUM( \
            FGEN_VER_YEAR, \
            FGEN_VER_MONTH, \
            FGEN_VER_MINOR, \
            FGEN_VER_RELEASE)
// clang-format on

/**
 * @brief Return the FGEN version as a human-readable string.
 *
 * The string is built once and cached in a static buffer.  Its format is
 * @c "FGEN YY.MM.minor" when there is no suffix, or
 * @c "FGEN YY.MM.minorSUFFIXrelease" when a suffix is defined.
 *
 * @return  Pointer to a static NUL-terminated version string.
 */
static inline const char *
fgen_version(void)
{
    static char version[32];
    if (version[0] != 0)
        return version;
    // clang-format off
    if (strlen(FGEN_VER_SUFFIX) == 0)
        snprintf(version, sizeof(version), "%s %d.%02d.%d",
            FGEN_VER_PREFIX,
            FGEN_VER_YEAR,
            FGEN_VER_MONTH,
            FGEN_VER_MINOR);
    else
        snprintf(version, sizeof(version), "%s %d.%02d.%d%s%d",
            FGEN_VER_PREFIX,
            FGEN_VER_YEAR,
            FGEN_VER_MONTH,
            FGEN_VER_MINOR,
            FGEN_VER_SUFFIX,
            FGEN_VER_RELEASE);
    // clang-format on
    return version;
}

#ifdef __cplusplus
}
#endif

#endif /* FGEN_VERSION_H */
