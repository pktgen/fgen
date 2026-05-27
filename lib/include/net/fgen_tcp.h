/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2026 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_TCP_H_
#define _FGEN_TCP_H_

/**
 * @file
 * @brief TCP (Transmission Control Protocol) header and flag definitions.
 */

#include <stdint.h>

#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TCP (Transmission Control Protocol) header.
 */
struct fgen_tcp_hdr {
    fgen_be16_t src_port; /**< TCP source lport. */
    fgen_be16_t dst_port; /**< TCP destination lport. */
    fgen_be32_t sent_seq; /**< TX data sequence number. */
    fgen_be32_t recv_ack; /**< RX data acknowledgment sequence number. */
    uint8_t data_off;    /**< Data offset. */
    uint8_t tcp_flags;   /**< TCP flags */
    fgen_be16_t rx_win;   /**< RX flow control window. */
    fgen_be16_t cksum;    /**< TCP checksum. */
    fgen_be16_t tcp_urp;  /**< TCP urgent pointer, if any. */
} __attribute__((__packed__));

/** @brief TCP flag: Congestion Window Reduced. */
#define TCP_CWR_FLAG 0x80
/** @brief TCP flag: ECN-Echo. */
#define TCP_ECE_FLAG 0x40
/** @brief TCP flag: Urgent Pointer field significant. */
#define TCP_URG_FLAG 0x20
/** @brief TCP flag: Acknowledgment field significant. */
#define TCP_ACK_FLAG 0x10
/** @brief TCP flag: Push Function. */
#define TCP_PSH_FLAG 0x08
/** @brief TCP flag: Reset the connection. */
#define TCP_RST_FLAG 0x04
/** @brief TCP flag: Synchronize sequence numbers. */
#define TCP_SYN_FLAG 0x02
/** @brief TCP flag: No more data from sender (FIN). */
#define TCP_FIN_FLAG 0x01

#ifdef __cplusplus
}
#endif

#endif /* FGEN_TCP_H_ */
