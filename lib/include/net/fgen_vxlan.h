/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#ifndef _FGEN_VXLAN_H_
#define _FGEN_VXLAN_H_

/**
 * @file
 *
 * VXLAN-related definitions
 */

#include <stdint.h>

#include <fgen_byteorder.h>
#include <net/fgen_udp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** VXLAN default port. */
#define FGEN_VXLAN_DEFAULT_PORT     4789
#define FGEN_VXLAN_GPE_DEFAULT_PORT 4790

/**
 * VXLAN protocol header.
 * Contains the 8-bit flag, 24-bit VXLAN Network Identifier and
 * Reserved fields (24 bits and 8 bits)
 */
struct fgen_vxlan_hdr {
    fgen_be32_t vx_flags; /**< flag (8) + Reserved (24). */
    fgen_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __fgen_packed;

/** VXLAN tunnel header length. */
#define FGEN_ETHER_VXLAN_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_vxlan_hdr))

/**
 * VXLAN-GPE protocol header (draft-ietf-nvo3-vxlan-gpe-05).
 * Contains the 8-bit flag, 8-bit next-protocol, 24-bit VXLAN Network
 * Identifier and Reserved fields (16 bits and 8 bits).
 */
struct fgen_vxlan_gpe_hdr {
    uint8_t vx_flags;    /**< flag (8). */
    uint8_t reserved[2]; /**< Reserved (16). */
    uint8_t proto;       /**< next-protocol (8). */
    fgen_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __fgen_packed;

/** VXLAN-GPE tunnel header length. */
#define FGEN_ETHER_VXLAN_GPE_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_vxlan_gpe_hdr))

/* VXLAN-GPE next protocol types */
#define FGEN_VXLAN_GPE_TYPE_IPV4 1 /**< IPv4 Protocol. */
#define FGEN_VXLAN_GPE_TYPE_IPV6 2 /**< IPv6 Protocol. */
#define FGEN_VXLAN_GPE_TYPE_ETH  3 /**< Ethernet Protocol. */
#define FGEN_VXLAN_GPE_TYPE_NSH  4 /**< NSH Protocol. */
#define FGEN_VXLAN_GPE_TYPE_MPLS 5 /**< MPLS Protocol. */
#define FGEN_VXLAN_GPE_TYPE_GBP  6 /**< GBP Protocol. */
#define FGEN_VXLAN_GPE_TYPE_VBNG 7 /**< vBNG Protocol. */

#ifdef __cplusplus
}
#endif

#endif /* FGEN_VXLAN_H_ */
