/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)libkern.h   8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

/**
 * @file
 * @brief CRC-32 and CRC-32C (iSCSI) computation API.
 *
 * Provides a software CRC-32 table-driven implementation derived from
 * BSD libkern and a CRC-32C implementation that uses SSE 4.2 hardware
 * acceleration when available.  All original copyright notices are
 * preserved exactly as required by their respective licences.
 */

#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>        // for uint32_t, uint8_t
#include <sys/cdefs.h>
#include <sys/types.h>
#include <stddef.h>        // for size_t

/** @brief CRC-32 feedback-term lookup table (polynomial 0xEDB88320). */
extern const uint32_t crc32_tab[];

/** @brief Round @p x down to the nearest multiple of @p y. */
#define rounddown(x, y)  (((x) / (y)) * (y))
/** @brief Round @p x down to the nearest power-of-two multiple of @p y (y must be a power of 2). */
#define rounddown2(x, y) ((x) & (~((y)-1)))
/** @brief Round @p x up to the nearest power-of-two multiple of @p y (y must be a power of 2). */
#define roundup2(x, y)   (((x) + ((y)-1)) & (~((y)-1)))

/**
 * @brief Compute a running CRC-32 over a buffer, starting from an existing CRC.
 *
 * @param buf
 *   Pointer to the data buffer.
 * @param size
 *   Number of bytes in @p buf.
 * @param crc
 *   Initial / running CRC value (use ~0U to start a new computation).
 * @return
 *   Updated CRC-32 value after processing @p buf.
 */
static __inline uint32_t
crc32_raw(const void *buf, size_t size, uint32_t crc)
{
    const uint8_t *p = (const uint8_t *)buf;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return (crc);
}

/**
 * @brief Compute the CRC-32 checksum of a buffer.
 *
 * @param buf
 *   Pointer to the data buffer.
 * @param size
 *   Number of bytes in @p buf.
 * @return
 *   CRC-32 checksum of the buffer contents.
 */
static __inline uint32_t
crc32(const void *buf, size_t size)
{
    uint32_t crc;

    crc = crc32_raw(buf, size, ~0U);
    return (crc ^ ~0U);
}

/**
 * @brief Compute a CRC-32C (Castagnoli) checksum, selecting the fastest available method.
 *
 * Uses SSE 4.2 hardware acceleration when the CPU supports it, otherwise falls
 * back to a software slice-by-8 implementation.
 *
 * @param crc32c
 *   Initial CRC value (use 0 to start a new computation).
 * @param buffer
 *   Pointer to the data buffer.
 * @param length
 *   Number of bytes in @p buffer.
 * @return
 *   CRC-32C checksum value.
 */
uint32_t calculate_crc32c(uint32_t crc32c, const unsigned char *buffer, unsigned int length);

/**
 * @brief Compute CRC-32C using SSE 4.2 hardware instructions.
 *
 * @param crc
 *   Initial CRC value.
 * @param buf
 *   Pointer to the data buffer.
 * @param len
 *   Number of bytes to process.
 * @return
 *   CRC-32C checksum value.
 */
uint32_t sse42_crc32c(uint32_t crc, const unsigned char *buf, unsigned len);

#endif /* _CRC32_H_ */
