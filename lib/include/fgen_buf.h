/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation
 */

/**
 * @file
 *
 * Simple structure to hold information about the frame data.
 */

#ifndef _FGEN_BUF_H_
#define _FGEN_BUF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fgenbuf_s {
    uint8_t l2_len;
    uint8_t l3_len;
    uint8_t l4_len;
} __fgen_cacheline_aligned;

typedef struct fgenbuf_s fgenbuf_t;

#ifdef __cplusplus
}
#endif

#endif /** _FGEN_BUF_H_ **/
