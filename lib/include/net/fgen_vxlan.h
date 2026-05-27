/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2026 Intel Corporation
 */

#ifndef _FGEN_VXLAN_H_
#define _FGEN_VXLAN_H_

/**
 * @file
 * @brief VXLAN and VXLAN-GPE header definitions and related constants.
 */

#include <stdint.h>

#include <fgen_byteorder.h>
#include <net/fgen_udp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default VXLAN UDP destination port (RFC 7348). */
#define FGEN_VXLAN_DEFAULT_PORT     4789
/** @brief Default VXLAN-GPE UDP destination port. */
#define FGEN_VXLAN_GPE_DEFAULT_PORT 4790

/**
 * @brief VXLAN protocol header.
 *
 * Contains the 8-bit flag, 24-bit VXLAN Network Identifier, and
 * reserved fields (24 bits and 8 bits).
 */
struct fgen_vxlan_hdr {
    fgen_be32_t vx_flags; /**< flag (8) + Reserved (24). */
    fgen_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __fgen_packed;

/** @brief Combined UDP + VXLAN tunnel header length in bytes. */
#define FGEN_ETHER_VXLAN_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_vxlan_hdr))

/**
 * @brief VXLAN-GPE protocol header (draft-ietf-nvo3-vxlan-gpe-05).
 *
 * Contains the 8-bit flag, 8-bit next-protocol, 24-bit VXLAN Network
 * Identifier, and reserved fields (16 bits and 8 bits).
 */
struct fgen_vxlan_gpe_hdr {
    uint8_t vx_flags;    /**< flag (8). */
    uint8_t reserved[2]; /**< Reserved (16). */
    uint8_t proto;       /**< next-protocol (8). */
    fgen_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __fgen_packed;

/** @brief Combined UDP + VXLAN-GPE tunnel header length in bytes. */
#define FGEN_ETHER_VXLAN_GPE_HLEN (sizeof(struct fgen_udp_hdr) + sizeof(struct fgen_vxlan_gpe_hdr))

/** @brief VXLAN-GPE next protocol type: IPv4. */
#define FGEN_VXLAN_GPE_TYPE_IPV4 1
/** @brief VXLAN-GPE next protocol type: IPv6. */
#define FGEN_VXLAN_GPE_TYPE_IPV6 2
/** @brief VXLAN-GPE next protocol type: Ethernet. */
#define FGEN_VXLAN_GPE_TYPE_ETH  3
/** @brief VXLAN-GPE next protocol type: NSH (Network Service Header). */
#define FGEN_VXLAN_GPE_TYPE_NSH  4
/** @brief VXLAN-GPE next protocol type: MPLS. */
#define FGEN_VXLAN_GPE_TYPE_MPLS 5
/** @brief VXLAN-GPE next protocol type: GBP (Group Based Policy). */
#define FGEN_VXLAN_GPE_TYPE_GBP  6
/** @brief VXLAN-GPE next protocol type: vBNG (virtual Broadband Network Gateway). */
#define FGEN_VXLAN_GPE_TYPE_VBNG 7

#ifdef __cplusplus
}
#endif

#endif /* FGEN_VXLAN_H_ */
