/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

#include <stdio.h>         // for fprintf, snprintf, fflush, FILE, NULL, stdout
#include <string.h>        // for strcpy
#include <stdlib.h>        // for free, malloc, calloc, realloc

#include <fgen_common.h>
#include <fgen_log.h>
#include "salloc.h"

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

void
salloc_destroy(salloc_t *salloc)
{
    if (salloc) {
        free(salloc->ptr);
        free(salloc);
    }
}

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
