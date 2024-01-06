/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2024 Intel Corporation.
 * All rights reserved.
 */

/**
 * @file
 *
 * SCTP-related defines
 */

#ifndef _FGEN_SCTP_H_
#define _FGEN_SCTP_H_

#include <stdint.h>

#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SCTP Header
 */
struct fgen_sctp_hdr {
    fgen_be16_t src_port; /**< Source port. */
    fgen_be16_t dst_port; /**< Destin port. */
    fgen_be32_t tag;      /**< Validation tag. */
    fgen_be32_t cksum;    /**< Checksum. */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* FGEN_SCTP_H_ */
