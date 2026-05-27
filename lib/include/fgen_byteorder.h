/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2026 Intel Corporation
 */

#ifndef _FGEN_BYTEORDER_H_
#define _FGEN_BYTEORDER_H_

/**
 * @file fgen_byteorder.h
 * @brief Endian-annotated integer typedefs for portable network code.
 *
 * The following types should be used when handling values according to a
 * specific byte ordering, which may differ from that of the host CPU.
 * Libraries, public APIs, and applications are encouraged to use them for
 * documentation purposes.
 */

#include <stdint.h>
#include <endian.h>

typedef uint16_t fgen_be16_t; /**< 16-bit big-endian value. */
typedef uint32_t fgen_be32_t; /**< 32-bit big-endian value. */
typedef uint64_t fgen_be64_t; /**< 64-bit big-endian value. */
typedef uint16_t fgen_le16_t; /**< 16-bit little-endian value. */
typedef uint32_t fgen_le32_t; /**< 32-bit little-endian value. */
typedef uint64_t fgen_le64_t; /**< 64-bit little-endian value. */

#endif /* _FGEN_BYTEORDER_H_ */
