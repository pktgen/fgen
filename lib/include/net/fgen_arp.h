/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation
 */

#ifndef _FGEN_ARP_H_
#define _FGEN_ARP_H_

/**
 * @file
 *
 * ARP-related defines
 */

// IWYU pragma: no_forward_declare fgen_mempool

#include <stdint.h>               // for uint16_t, uint32_t, uint8_t
#include <net/fgen_ether.h>       // for ether_addr
#include <net/ethernet.h>         // for ether_addr

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ARP header IPv4 payload.
 */
struct fgen_arp_ipv4 {
    struct ether_addr arp_sha; /**< sender hardware address */
    uint32_t arp_sip;          /**< sender IP address */
    struct ether_addr arp_tha; /**< target hardware address */
    uint32_t arp_tip;          /**< target IP address */
} __fgen_packed __fgen_aligned(2);

/**
 * ARP header.
 */
struct fgen_arp_hdr {
    uint16_t arp_hardware;  /* format of hardware address */
#define FGEN_ARP_HRD_ETHER 1 /* ARP Ethernet address format */

    uint16_t arp_protocol;      /* format of protocol address */
    uint8_t arp_hlen;           /* length of hardware address */
    uint8_t arp_plen;           /* length of protocol address */
    uint16_t arp_opcode;        /* ARP opcode (command) */
#define FGEN_ARP_OP_REQUEST    1 /* request to resolve address */
#define FGEN_ARP_OP_REPLY      2 /* response to previous request */
#define FGEN_ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define FGEN_ARP_OP_REVREPLY   4 /* response giving protocol address */
#define FGEN_ARP_OP_INVREQUEST 8 /* request to identify peer */
#define FGEN_ARP_OP_INVREPLY   9 /* response identifying peer */

    struct fgen_arp_ipv4 arp_data;
} __fgen_packed __fgen_aligned(2);

#ifdef __cplusplus
}
#endif

#endif /* _FGEN_ARP_H_ */
