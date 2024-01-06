/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation.
 */

#ifndef __FGEN_STDIO_H_
#define __FGEN_STDIO_H_

/**
 * @file
 * FGEN cursor and color support for VT100 using ANSI color escape codes.
 */

// IWYU pragma: no_include <bits/termios-struct.h>

#include <termios.h>
#include <stdarg.h>        // for va_list
#include <stdint.h>        // for int16_t
#include <stdio.h>         // for FILE
#include <string.h>        // for strlen
#include <unistd.h>        // for write

#include <fgen_atomic.h>        // for atomic_exchange, atomic_int_least32_t, atomic...
#include <fgen_common.h>
#include <fgen_system.h>
#include <vt100_out.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A printf like routine to output to tty file descriptor.
 *
 * @param fmt
 *   The formatting string for a printf like API
 */
FGEN_API int fgen_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * cursor position the text at the given location for r and c.
 *
 * @param r
 *   The row to place the text.
 * @param c
 *   The column to place the text.
 * @param fmt
 *   The fgen_printf() like format string.
 */
FGEN_API int fgen_printf_pos(int16_t r, int16_t c, const char *fmt, ...)
    __attribute__((format(printf, 3, 0)));

/**
 * Routine similar to fprintf() to output text to a file descriptor.
 *
 * @param f
 *   The file descriptor to output the text to.
 * @param fmt
 *   The format string to output.
 * @return
 *   The number of bytes written
 */
FGEN_API int fgen_fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 0)));

/**
 * Format a string with color formatting and return the number of bytes
 *
 * @param buff
 *    The buffer to place the formatted string with color
 * @param len
 *    The max length of the *buff* array
 * @param fmt
 *    The formatting string to use with any arguments
 * @return
 *    The number of bytes written into the *buff* array
 */
FGEN_API int fgen_snprintf(char *buff, int len, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Output a string at a given row/column using printf like formatting.
 * Centering the text on the console display, using the *ncols* value
 *
 * @param r
 *    The row to start the print out of the string.
 * @param ncols
 *    Number of columns on the line. Used to center the text on the line.
 * @param fmt
 *    The formatting string to use to print the text data.
 */
FGEN_API int fgen_cprintf(int16_t r, int16_t ncols, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * vsnprintf() like function using va_list pointer with color tags.
 *
 * @param buff
 *   The output buffer to place the text
 * @param len
 *   The length of the buff array
 * @param fmt
 *   The format string with color tags
 * @param ap
 *   The va_list pointer to be used byt vsnprintf()
 * @return
 *   The number of bytes in the buff array or -1 on error
 */
FGEN_API int fgen_vsnprintf(char *buff, int len, const char *fmt, va_list ap)
    __attribute__((format(printf, 3, 0)));

/**
 * vprintf() like function using va_list pointer with color tags.
 *
 * @param fmt
 *   The format string with color tags
 * @param ap
 *   The va_list pointer to be used
 * @return
 *  number of bytes written to output or -1 on error
 */
FGEN_API int fgen_vprintf(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));

#ifdef __cplusplus
}
#endif

#endif /* __FGEN_STDIO_H_ */
