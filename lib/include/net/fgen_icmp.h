/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 1982, 1986, 1990, 1993
 *      The Regents of the University of California.
 * Copyright (c) 2023-2024 Intel Corporation.
 * All rights reserved.
 */

#ifndef _FGEN_ICMP_H_
#define _FGEN_ICMP_H_

/**
 * @file
 *
 * ICMP-related defines
 */

#include <stdint.h>

#include <fgen_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ICMP Header
 */
struct fgen_icmp_hdr {
    uint8_t icmp_type;      /* ICMP packet type. */
    uint8_t icmp_code;      /* ICMP packet code. */
    fgen_be16_t icmp_cksum;  /* ICMP packet checksum. */
    fgen_be16_t icmp_ident;  /* ICMP packet identifier. */
    fgen_be16_t icmp_seq_nb; /* ICMP packet sequence number. */
} __attribute__((__packed__));

/* ICMP packet types */
#define FGEN_IP_ICMP_ECHO_REPLY   0
#define FGEN_IP_ICMP_ECHO_REQUEST 8

#ifdef __cplusplus
}
#endif

#endif /* FGEN_ICMP_H_ */
