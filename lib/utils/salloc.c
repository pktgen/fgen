/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief Bump-pointer allocator (salloc) implementation.
 *
 * Implements the salloc API declared in salloc.h: creation, destruction,
 * and offset-based allocation from a dynamically-grown memory block.
 */

#include <stdio.h>         // for fprintf, snprintf, fflush, FILE, NULL, stdout
#include <string.h>        // for strcpy
#include <stdlib.h>        // for free, malloc, calloc, realloc

#include <fgen_common.h>
#include <fgen_log.h>
#include "salloc.h"

/**
 * @brief Create a new salloc_t allocator with an initial backing block.
 *
 * @param size
 *   Initial size of the backing block in bytes.  If 0, DEFAULT_SALLOC_BLOCK_SIZE
 *   is used.
 * @return
 *   Pointer to the newly created salloc_t, or NULL on allocation failure.
 */
salloc_t *
salloc_create(uint64_t size)
{
    salloc_t *salloc;

    salloc = (salloc_t *)calloc(1, sizeof(salloc_t));
    if (!salloc)
        return NULL;

    if (size == 0)
        size = DEFAULT_SALLOC_BLOCK_SIZE;

    salloc->ptr = (void *)realloc(salloc->ptr, size);
    if (!salloc->ptr) {
        free(salloc);
        return NULL;
    }
    memset(salloc->ptr, 0, size);
    salloc->size = size;

    return salloc;
}

/**
 * @brief Destroy an salloc_t allocator and release all memory.
 *
 * @param salloc
 *   Pointer to the salloc_t to destroy; NULL is silently ignored.
 */
void
salloc_destroy(salloc_t *salloc)
{
    if (salloc) {
        free(salloc->ptr);
        free(salloc);
    }
}

/**
 * @brief Allocate @p size bytes from the salloc block.
 *
 * If insufficient space remains the backing block is grown via realloc().
 * The returned offset is stable only until the next salloc() call that
 * triggers a realloc() (the block address may change).
 *
 * @param salloc
 *   Pointer to the salloc_t allocator; must not be NULL.
 * @param size
 *   Number of bytes to allocate; must be > 0.
 * @param offset
 *   On success, receives the byte offset within the block at which the
 *   allocation starts; must not be NULL.
 * @return
 *   0 on success, -1 on invalid arguments or allocation failure.
 */
int
salloc(salloc_t *salloc, uint64_t size, offset_t *offset)
{
    if (!salloc || !offset)
        return -1;
    if (size == 0)
        return -1;

    if (!salloc->ptr || (size > salloc_unused(salloc))) {
        salloc->ptr = realloc(salloc->ptr, size);
        if (!salloc->ptr)
            FGEN_ERR_RET("salloc for %ld bytes", size);
        salloc->size += size;
    }
    *offset = salloc->used;
    salloc->used += size;

    return 0;
}
