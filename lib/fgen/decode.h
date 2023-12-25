/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#ifndef __FGEN_DECODE_H
#define __FGEN_DECODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FGEN_INVALID_PID 0xFFFF /**< Invalid PID */
#define TIMESTAMP_ID     (('t' << 24) || ('s' << 16) || ('c' << 8) || '=')

typedef struct tsc_s {
    uint32_t tstmp;
    uint64_t tsc_val;
} tsc_t;

typedef struct decode_s {
    void *data;        /**< Frame data */
    uint16_t data_len; /**< Length of the data buffer */
    uint16_t data_off; /**< Current offset into data frame */
    uint16_t pid;      /**< Port ID 0 - N or 0xFFFF if not defined */
    uint16_t rsvd;     /* Reserved */
    char *buffer;      /**< Output buffers pointer */
    int buf_len;       /**< length of buffer data */
    int used;          /**< Amount of used data in buffer */

} decode_t;

/**
 * Return the data pointer to the packet data.
 *
 * @param f
 *   The decode_t structure pointer.
 */
#define decode_data(f) (f)->data

/**
 * Return the packet data length.
 *
 * @param f
 *   The decode_t structure pointer.
 */
#define decode_len(f) (f)->data_len

/**
 * Return the packet data length.
 *
 * @param f
 *   The decode_t structure pointer.
 */
#define decode_offset(f) (f)->data_off

/**
 * Return the packet data pointer at the given offset.
 *
 * @param f
 *   The decode_t structure pointer.
 * @param t
 *   The pointer type cast.
 * @param o
 *   The offset into the packet.
 */
#define decode_mtod_offset(f, t, o) ((t)((char *)decode_data(f) + (o)))

/**
 * Return the first byte address of the packet data.
 *
 * @param f
 *   The frame_t structure pointer.
 * @param t
 *   The pointer type cast.
 */
#define decode_mtod(f, t) decode_mtod_offset(f, t, 0)

#ifdef __cplusplus
}
#endif

#endif /* __FGEN_DECODE_H */
