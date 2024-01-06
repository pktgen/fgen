/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2024 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_UDP_H_
#define _FGEN_UDP_H_

/**
 * @file
 *
 * UDP-related defines
 */

#include <stdint.h>

#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UDP Header
 */
struct fgen_udp_hdr {
    fgen_be16_t src_port;    /**< UDP source port. */
    fgen_be16_t dst_port;    /**< UDP destination port. */
    fgen_be16_t dgram_len;   /**< UDP datagram length */
    fgen_be16_t dgram_cksum; /**< UDP datagram checksum */
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif /* FGEN_UDP_H_ */
