/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020-2026 Intel Corporation
 */

#ifndef _MMAP_PRIVATE_H_
#define _MMAP_PRIVATE_H_

#include <stddef.h>        // for size_t
#include <stdint.h>        // for uint64_t, uint8_t

#include <fgen_mmap.h>

/**
 * @file
 * @brief Internal mmap allocator data structures.
 *
 * Defines the private mmap_data descriptor used by fgen_mmap.c.
 * Not part of the public API — do not include directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal descriptor for a single mmap allocation.
 *
 * Tracks all bookkeeping needed to manage and eventually free an
 * anonymous memory-mapped region.
 */
struct mmap_data {
    uint32_t bufcnt; /**< Number of buffers requested by the caller */
    uint32_t bufsz;  /**< Size in bytes of each buffer */
    size_t sz;       /**< Actual mapped size in bytes (page-aligned) */
    void *addr;      /**< Base virtual address of the mapped region */
    mmap_type_t typ; /**< Hugepage type actually used for this allocation */
    unsigned align;  /**< Alignment / page size used for the mapping */
};

#ifdef __cplusplus
}
#endif

#endif /* _MMAP_PRIVATE_H_ */
