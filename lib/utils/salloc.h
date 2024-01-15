/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

#ifndef _FGEN_SALLOC_H_
#define _FGEN_SALLOC_H_

/**
 * @file
 *
 * Simple API to allocate from a block of memory. This API does not provide any
 * protection against memory corruption or memory leaks. The API does not
 * provide any methods to free memory allocated by this API.
 * 
 * The API allocates memory from a block of memory and returns the offset of the
 * memory allocated from the block. The memory block can dynamically be resized as more
 * memory is needed, which means the memory block address can be changed.
 */

#include <stdio.h>

#include <fgen_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_SALLOC_BLOCK_SIZE 1024

typedef uint64_t offset_t; /*< Offset into the salloc memory block */

typedef struct salloc_s {
    void *ptr; /**< Pointer to the memory block for allocations */
    uint64_t size; /**< Current total size of the memory block */
    uint64_t used; /**< Amount of space currently used in memory block */
} salloc_t;

/**
 * @brief Calculates the amount of unused memory in a salloc_t structure
 *
 * @param salloc A pointer to the salloc_t structure
 * @return uint64_t The amount of unused memory
 */
static inline uint64_t
salloc_unused(const salloc_t *salloc)
{
    return salloc->size - salloc->used;
}

/**
 * @brief Calculates the pointer to a memory location within a salloc_t structure
 *
 * @param salloc A pointer to the salloc_t structure
 * @param offset The offset into the memory block
 * @return void* A pointer to the memory location within the memory block.
 */
static inline void *
salloc_ptr(const salloc_t *salloc, offset_t offset)
{
    return (void *)((char *)salloc->ptr + offset);
}

/**
 * @brief Creates a new salloc_t structure with the specified size of memory
 *
 * @param size The size of the memory block to allocate
 * @return salloc_t* A pointer to the newly created salloc_t structure
 */
FGEN_API salloc_t *salloc_create(size_t size);

/**
 * @brief Frees the memory allocated by salloc and its memory block
 *
 * @param salloc A pointer to the salloc_t structure
 */
FGEN_API void salloc_destroy(salloc_t *salloc);

/**
 * @brief Allocates memory from a salloc_t structure memory block.
 *
 * @param salloc A pointer to the salloc_t structure
 * @param size The size of the memory block to allocate
 * @param offset A pointer to an offset_t variable that will be set to the offset
 *   into the memory block where the memory was allocated
 * @return int 0 on success, negative value on failure
 */
FGEN_API int salloc(salloc_t *salloc, size_t size, offset_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* _FGEN_SALLOC_H_ */
