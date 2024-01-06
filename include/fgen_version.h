/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

/**
 * @file
 *
 * Definitions of version numbers
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
 * Macro to compute a version number usable for comparisons
 */
#define FGEN_VERSION_NUM(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

/**
 * All version numbers in one define to compare with FGEN_VERSION_NUM()
 */
// clang-format off
#define FGEN_VERSION FGEN_VERSION_NUM( \
            FGEN_VER_YEAR, \
            FGEN_VER_MONTH, \
            FGEN_VER_MINOR, \
            FGEN_VER_RELEASE)
// clang-format on

/**
 * Function returning version string
 *
 * @return
 *     string
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
