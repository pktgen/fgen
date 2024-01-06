/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation
 */

#ifndef _FGEN_GRE_H_
#define _FGEN_GRE_H_

/**
 * @file
 */

#include <stdint.h>
#include <endian.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GRE Header
 */
__extension__ struct fgen_gre_hdr {
#if BYTE_ORDER == LITTLE_ENDIAN
    uint16_t res2 : 4; /**< Reserved */
    uint16_t s : 1;    /**< Sequence Number Present bit */
    uint16_t k : 1;    /**< Key Present bit */
    uint16_t res1 : 1; /**< Reserved */
    uint16_t c : 1;    /**< Checksum Present bit */
    uint16_t ver : 3;  /**< Version Number */
    uint16_t res3 : 5; /**< Reserved */
#elif BYTE_ORDER == BIG_ENDIAN
    uint16_t c : 1;    /**< Checksum Present bit */
    uint16_t res1 : 1; /**< Reserved */
    uint16_t k : 1;    /**< Key Present bit */
    uint16_t s : 1;    /**< Sequence Number Present bit */
    uint16_t res2 : 4; /**< Reserved */
    uint16_t res3 : 5; /**< Reserved */
    uint16_t ver : 3;  /**< Version Number */
#endif
    uint16_t proto; /**< Protocol Type */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* FGEN_GRE_H_ */
