/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation.
 */

#ifndef _FGEN_CYCLES_H_
#define _FGEN_CYCLES_H_

/**
 * @file
 */

#include <fgen_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

/**
 * Read and return the timestamp counter value
 *
 * @return
 *   Returns the 64bit time sample counter value.
 */
static inline uint64_t
fgen_rdtsc(void)
{
    union {
        uint64_t tsc_64;
        struct {
            uint32_t lo_32;
            uint32_t hi_32;
        };
    } tsc;

    // clang-format off
    asm volatile("rdtsc" :
             "=a" (tsc.lo_32),
             "=d" (tsc.hi_32));
    // clang-format on
    return tsc.tsc_64;
}

#ifdef __cplusplus
}
#endif

#endif /* _FGEN_CYCLES_H_ */
