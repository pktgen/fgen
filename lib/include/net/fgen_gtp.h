/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2026 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_GTP_H_
#define _FGEN_GTP_H_

/**
 * @file
 * @brief GTP (GPRS Tunnelling Protocol) header definitions and related constants.
 */

#include <stdint.h>
#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simplified GTP protocol header.
 *
 * Contains 8-bit header info, 8-bit message type,
 * 16-bit payload length after mandatory header, and 32-bit TEID.
 * No optional fields or next extension header are included.
 */
struct fgen_gtp_hdr {
    uint8_t gtp_hdr_info; /**< GTP header info */
    uint8_t msg_type;     /**< GTP message type */
    uint16_t plen;        /**< Total payload length */
    uint32_t teid;        /**< Tunnel endpoint ID */
} __attribute__((__packed__));

/** @brief Combined UDP + GTP header length in bytes. */
#define FGEN_ETHER_GTP_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_gtp_hdr))

/** @brief GTP next protocol type: IPv4. */
#define FGEN_GTP_TYPE_IPV4 0x40
/** @brief GTP next protocol type: IPv6. */
#define FGEN_GTP_TYPE_IPV6 0x60

/** @brief GTP-C (control plane) UDP destination port. */
#define FGEN_GTPC_UDP_PORT 2123
/** @brief GTP-U (user plane) UDP destination port. */
#define FGEN_GTPU_UDP_PORT 2152

#ifdef __cplusplus
}
#endif

#endif /* FGEN_GTP_H_ */
