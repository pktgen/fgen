/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2024 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_GTP_H_
#define _FGEN_GTP_H_

/**
 * @file
 *
 * GTP-related defines
 */

#include <stdint.h>
#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simplified GTP protocol header.
 * Contains 8-bit header info, 8-bit message type,
 * 16-bit payload length after mandatory header, 32-bit TEID.
 * No optional fields and next extension header.
 */
struct fgen_gtp_hdr {
    uint8_t gtp_hdr_info; /**< GTP header info */
    uint8_t msg_type;     /**< GTP message type */
    uint16_t plen;        /**< Total payload length */
    uint32_t teid;        /**< Tunnel endpoint ID */
} __attribute__((__packed__));

/** GTP header length */
#define FGEN_ETHER_GTP_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_gtp_hdr))
/* GTP next protocol type */
#define FGEN_GTP_TYPE_IPV4 0x40 /**< GTP next protocol type IPv4 */
#define FGEN_GTP_TYPE_IPV6 0x60 /**< GTP next protocol type IPv6 */
/* GTP destination lport number */
#define FGEN_GTPC_UDP_PORT 2123 /**< GTP-C UDP destination port */
#define FGEN_GTPU_UDP_PORT 2152 /**< GTP-U UDP destination port */

#ifdef __cplusplus
}
#endif

#endif /* FGEN_GTP_H_ */
