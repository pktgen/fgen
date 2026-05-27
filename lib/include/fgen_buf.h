/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2026 Intel Corporation
 */

/**
 * @file fgen_buf.h
 * @brief Simple structure to hold layer-length information about a frame buffer.
 */

#ifndef _FGEN_BUF_H_
#define _FGEN_BUF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame buffer metadata describing the byte lengths of each protocol layer.
 */
struct fgenbuf_s {
    uint8_t l2_len; /**< Length of the Layer-2 (Ethernet) header in bytes. */
    uint8_t l3_len; /**< Length of the Layer-3 (IP) header in bytes. */
    uint8_t l4_len; /**< Length of the Layer-4 (TCP/UDP) header in bytes. */
} __fgen_cacheline_aligned;

/** @brief Convenience typedef for struct fgenbuf_s. */
typedef struct fgenbuf_s fgenbuf_t;

#ifdef __cplusplus
}
#endif

#endif /** _FGEN_BUF_H_ **/
