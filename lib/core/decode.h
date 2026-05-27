/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2026 Intel Corporation
 */

/**
 * @file decode.h
 * @brief Internal decode-context types and accessor macros for decode.c.
 *
 * This header is private to lib/core and is not part of the public API.
 * It defines the @c decode_t working context used while converting raw packet
 * bytes back into a fgen frame-description string.
 */

#ifndef __FGEN_DECODE_H
#define __FGEN_DECODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FGEN_INVALID_PID 0xFFFF /**< Invalid port/protocol ID sentinel */
#define TIMESTAMP_ID     (('t' << 24) || ('s' << 16) || ('c' << 8) || '=') /**< TSC magic marker */

/**
 * @brief Embedded hardware timestamp record written by the TSC() layer.
 *
 * @c tstmp holds the @c TIMESTAMP_ID magic value so that the decoder can
 * detect the presence of the timestamp slot without inspecting protocol
 * headers.  @c tsc_val is filled in at transmit time by the caller.
 */
typedef struct tsc_s {
    uint32_t tstmp;   /**< Magic marker — must equal TIMESTAMP_ID */
    uint64_t tsc_val; /**< Hardware TSC value stamped at transmit time */
} tsc_t;

/**
 * @brief Working context for a single fgen_decode() call.
 *
 * Tracks the raw packet data pointer, the current read position within it
 * (@c data_off), and the dynamically-grown output string buffer (@c buffer)
 * into which the decoded fgen layer tokens are appended.
 *
 * Allocated by fgen_decode_create(); freed by fgen_decode_destroy().
 * The output buffer is reallocated on demand by _append() — callers must not
 * hold pointers into @c buffer across _append() calls.
 */
typedef struct decode_s {
    void *data;        /**< Pointer to raw packet bytes being decoded */
    uint16_t data_len; /**< Total length of the raw packet in bytes */
    uint16_t data_off; /**< Current read offset within @c data */
    char *buffer;      /**< Dynamically allocated output string buffer */
    int buf_len;       /**< Allocated capacity of @c buffer in bytes */
    int used;          /**< Number of bytes currently used in @c buffer */
} decode_t;

/**
 * @brief Return the raw data pointer from a decode context.
 * @param f  Pointer to a @c decode_t.
 */
#define decode_data(f) (f)->data

/**
 * @brief Return the total packet length from a decode context.
 * @param f  Pointer to a @c decode_t.
 */
#define decode_len(f) (f)->data_len

/**
 * @brief Return (as an lvalue) the current read offset within the packet.
 * @param f  Pointer to a @c decode_t.
 */
#define decode_offset(f) (f)->data_off

/**
 * @brief Cast-and-offset the raw data pointer to an arbitrary type.
 * @param f  Pointer to a @c decode_t.
 * @param t  Target pointer type (e.g. @c struct fgen_ipv4_hdr *).
 * @param o  Byte offset from the start of the packet.
 */
#define decode_mtod_offset(f, t, o) ((t)((char *)decode_data(f) + (o)))

/**
 * @brief Cast the raw data pointer to an arbitrary type at offset 0.
 * @param f  Pointer to a @c decode_t.
 * @param t  Target pointer type.
 */
#define decode_mtod(f, t) decode_mtod_offset(f, t, 0)

#ifdef __cplusplus
}
#endif

#endif /* __FGEN_DECODE_H */
