/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief Memory hex-dump utility implementation.
 *
 * Implements fgen_hexdump() and fgen_memdump() as declared in hexdump.h.
 */

#include <stdio.h>         // for fprintf, snprintf, fflush, FILE, NULL, stdout
#include <string.h>        // for strcpy

#include <fgen_common.h>
#include <fgen_stdio.h>
#include "hexdump.h"

/** @brief Maximum length of a formatted line in the hexdump output buffer. */
#define LINE_LEN 128

/**
 * @brief Dump memory in a traditional offset + hex + ASCII layout.
 *
 * @param f
 *   Destination FILE pointer; if NULL stdout is used.
 * @param title
 *   Optional header string printed before the dump; may be NULL.
 * @param buf
 *   Pointer to the first byte of the region to dump.
 * @param len
 *   Number of bytes to dump.
 */
void
fgen_hexdump(FILE *f, const char *title, const void *buf, unsigned int len)
{
    unsigned int i, out, ofs;
    const unsigned char *data = buf;
    char line[LINE_LEN]; /* Space needed 8+16*3+3+16 == 75 */

    if (f == NULL)
        f = stdout;

    fgen_fprintf(f, "%s at [%p], len=%u\n", title ? "" : "  Dump data", data, len);
    ofs = 0;
    while (ofs < len) {
        /* Format the line in the buffer */
        out = snprintf(line, LINE_LEN, "%08X:", ofs);
        for (i = 0; i < 16; i++) {
            if (ofs + i < len)
                snprintf(line + out, LINE_LEN - out, " %02X", (data[ofs + i] & 0xff));
            else
                strcpy(line + out, "   ");
            out += 3;
        }

        for (; i <= 16; i++)
            out += snprintf(line + out, LINE_LEN - out, " | ");

        for (i = 0; ofs < len && i < 16; i++, ofs++) {
            unsigned char c = data[ofs];

            if (c < ' ' || c > '~')
                c = '.';
            out += snprintf(line + out, LINE_LEN - out, "%c", c);
        }
        fgen_fprintf(f, "%s\n", line);
    }

    fflush(f);
}

/**
 * @brief Dump memory as a compact colon-separated hex byte sequence.
 *
 * @param f
 *   Destination FILE pointer; if NULL stdout is used.
 * @param title
 *   Optional label printed before the bytes; may be NULL.
 * @param buf
 *   Pointer to the first byte of the region to dump.
 * @param len
 *   Number of bytes to dump.
 */
void
fgen_memdump(FILE *f, const char *title, const void *buf, unsigned int len)
{
    unsigned int i, out;
    const unsigned char *data = buf;
    char line[LINE_LEN];

    if (f == NULL)
        f = stdout;

    if (title)
        fgen_fprintf(f, "%s: ", title);

    line[0] = '\0';
    for (i = 0, out = 0; i < len; i++) {
        /* Make sure we do not overrun the line buffer length. */
        if (out >= LINE_LEN - 4) {
            fgen_fprintf(f, "%s", line);
            out       = 0;
            line[out] = '\0';
        }
        out += snprintf(line + out, LINE_LEN - out, "%02x%s", (data[i] & 0xff),
                        ((i + 1) < len) ? ":" : "");
    }
    if (out > 0)
        fgen_fprintf(f, "%s", line);
    fgen_fprintf(f, "\n");

    fflush(f);
}
