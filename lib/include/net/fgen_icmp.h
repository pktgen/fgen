/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2026 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_ICMP_H_
#define _FGEN_ICMP_H_

/**
 * @file
 * @brief ICMP (Internet Control Message Protocol) header and related defines.
 */

#include <stdint.h>

#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ICMP header.
 */
struct fgen_icmp_hdr {
    uint8_t icmp_type;       /**< ICMP packet type. */
    uint8_t icmp_code;       /**< ICMP packet code. */
    fgen_be16_t icmp_cksum;  /**< ICMP packet checksum. */
    fgen_be16_t icmp_ident;  /**< ICMP packet identifier. */
    fgen_be16_t icmp_seq_nb; /**< ICMP packet sequence number. */
} __attribute__((__packed__));

/** @brief ICMP echo reply message type. */
#define FGEN_IP_ICMP_ECHO_REPLY   0
/** @brief ICMP echo request message type. */
#define FGEN_IP_ICMP_ECHO_REQUEST 8

#ifdef __cplusplus
}
#endif

#endif /* FGEN_ICMP_H_ */
