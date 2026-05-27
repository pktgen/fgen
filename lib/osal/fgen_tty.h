/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation.
 */

#ifndef __FGEN_TTY_H_
#define __FGEN_TTY_H_

/**
 * @file
 * @brief FGEN TTY input and output support routines.
 *
 * Manages terminal setup, window-size tracking, and raw I/O for the
 * configured stdin/stdout file descriptors (or a substitute socket pair).
 * All output routines and the CLI layer use these functions for terminal I/O.
 */

// IWYU pragma: no_include <bits/termios-struct.h>

#include <termios.h>
#include <signal.h>            // for sigaction
#include <stdarg.h>            // for va_list
#include <stdint.h>            // for uint16_t
#include <stdio.h>             // for FILE
#include <string.h>            // for strlen
#include <unistd.h>            // for read, write
#include <fgen_atomic.h>        // for FGEN_ATOMIC
#include <fgen_common.h>
#include <fgen_system.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default TTY I/O buffer size in bytes. */
#define TTY_BUFF_SIZE    1024
/** @brief Maximum TTY command string size in bytes. */
#define TTY_MAX_CMD_SIZE 16

/**
 * @brief Union representing the terminal window dimensions.
 *
 * Allows the row and column counts to be read or written atomically as a
 * single 32-bit integer (@p val) or individually as @p nrows / @p ncols.
 */
typedef union {
    int val;               /**< Combined row/column value for atomic access. */
    struct {
        uint16_t nrows; /**< Max number of rows. */
        uint16_t ncols; /**< Max number of columns. */
    };
} tty_wsize_t;

/** @brief Structure to hold information about the tty. */
typedef struct {
    int flags;                       /**< Flags for tty setup and window change */
    int fd_out;                      /**< File descriptor for output data */
    int fd_in;                       /**< File descriptor for input data */
    struct sigaction saved_action;   /**< Saved sigaction data */
    FGEN_ATOMIC(uint_fast16_t) winsz; /**< Atomic value to detect a TIOCGWINSZ change */
    tty_wsize_t wsize;               /**< Window size values */
    struct termios oldterm;          /**< Old terminal setup information */
    FGEN_ATOMIC(int_least32_t) pause; /**< Pause the update of the screen. */
} fgen_tty_t;

/**
 * @brief Bit-flag constants for the @p fgen_tty_t::flags field.
 */
enum {
    TTY_IS_INITED  = (1 << 0), /**< TTY subsystem has been initialised. */
    TTY_IS_A_TTY   = (1 << 1), /**< Input fd is a real terminal device. */
    TTY_COLOR_ON   = (1 << 2), /**< ANSI color output is enabled. */
    TTY_WS_CHANGED = (1 << 8), /**< Window size has changed (SIGWINCH). */
};

extern fgen_tty_t *this_tty;

/**
 * setup a tty or socket for user input. Default is stdin/stdout
 *
 * @param fd_in
 *   Set the tty input file descriptor, if -1 use STDIN_FILENO
 * @param fd_out
 *   Set the tty output file descriptor, if -i use STDOUT_FILENO
 * @return
 *   -1 on error or 0 on success
 */
FGEN_API int tty_setup(int fd_in, int fd_out);

/**
 * Destroy or cleanup the tty interface and handlers
 */
FGEN_API void tty_destroy(void);

/**
 * Enable color printing of messages.
 */
FGEN_API void tty_enable_color(void);

/**
 * Disable color printing of messages.
 */
FGEN_API void tty_disable_color(void);

/**
 * Test to see if color output is enabled or disabled.
 *
 * @return
 *   false if color has been disabled or true if enabled.
 */
FGEN_API int tty_is_color_on(void);

/**
 * Has the TTY been inited?
 *
 * @return
 *   true if the tty has been inited or false if not.
 */
FGEN_API int tty_is_inited(void);

/**
 * Set the window size changed flag.
 */
FGEN_API void tty_set_wchanged(void);

/**
 * Clear the window size changed flag.
 */
FGEN_API void tty_clear_wchanged(void);

/**
 * Test if the window size has changed.
 *
 * Flag is cleared when read.
 *
 * @return
 *   false if the window has not changed or true if it has changed.
 */
FGEN_API int tty_did_wchange(void);

/**
 * Get the current window size
 *
 * @return
 *   Pointer to structure or NULL if error.
 */
FGEN_API tty_wsize_t *tty_window_size(void);

/**
 * Return the number of rows in the screen.
 *
 * @return
 *   The current number of terminal rows.
 */
FGEN_API int tty_num_rows(void);

/**
 * Return the number of columns in the screen.
 *
 * @return
 *   The current number of terminal columns.
 */
FGEN_API int tty_num_columns(void);

/**
 * Poll the TTY input fd and return number of bytes read
 *
 * @param buf
 *   Buffer to put the input characters
 * @param len
 *   The length of the buf array
 * @param timeout
 *   The number of milli-seconds to wait or -1 for no timeout
 * @return
 *   0 - on timeout
 *   -1 - on Error
 *   Number of characters read from input.
 */
FGEN_API int tty_poll(char *buf, int len, int timeout);

/**
 * Write the data from buf to the tty file descriptor.
 *
 * @param buf
 *    Pointer to data buffer to output the file descriptor.
 * @param len
 *    Number of bytes to write to file descriptor.
 * @return
 *    The number of bytes written to the tty file descriptor
 */
FGEN_API int tty_write(const char *buf, int len);

/**
 * Write the data from buf to the tty file descriptor number, bypass this_tty->fd_out
 *
 * @param fd
 *    The file descriptor index value
 * @param buf
 *    Pointer to data buffer to output the file descriptor.
 * @param len
 *    Number of bytes to write to file descriptor.
 * @return
 *    The number of bytes written to the tty file descriptor
 */
FGEN_API int tty_dwrite(int fd, const char *buf, int len);

/**
 * Write the data from buf to the tty file descriptor, bypass this_tty->fd_out
 *
 * @param f
 *    The file descriptor pointer
 * @param buf
 *    Pointer to data buffer to output the file descriptor.
 * @param len
 *    Number of bytes to write to file descriptor.
 * @return
 *    The number of bytes written to the tty file descriptor
 */
FGEN_API int tty_fwrite(FILE *f, const char *buf, int len);

/**
 * Read the data from the tty file descriptor into the buffer.
 *
 * @param buf
 *    Pointer to data buffer to place the input from file descriptor.
 * @param len
 *    Number of bytes to read from file descriptor.
 * @return
 *    The number of bytes read from the tty file descriptor
 */
FGEN_API int tty_read(char *buf, int len);

/**
 * Read the data from the tty file descriptor into the buffer.
 *
 * @param fd
 *    The file descriptor index value
 * @param buf
 *    Pointer to data buffer to place the input from file descriptor.
 * @param len
 *    Number of bytes to read from file descriptor.
 * @return
 *    The number of bytes read from the tty file descriptor
 */
FGEN_API int tty_dread(int fd, char *buf, int len);

/**
 * Read the data from the tty file descriptor into the buffer.
 *
 * @param f
 *    The file descriptor pointer
 * @param buf
 *    Pointer to data buffer to place the input from file descriptor.
 * @param len
 *    Number of bytes to read from file descriptor.
 * @return
 *    The number of bytes read from the tty file descriptor
 */
FGEN_API int tty_fread(FILE *f, char *buf, int len);

/**
 * A printf like routine to output to tty file descriptor.
 *
 * @param fmt
 *   The formatting string for a printf like API
 * @return
 *   The number of bytes written to the tty file descriptor.
 */
FGEN_API int tty_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * A fprintf like routine to output to tty file descriptor.
 *
 * @param f
 *    The file descriptor pointer
 * @param fmt
 *   The formatting string for a printf like API
 * @return
 *   The number of bytes written, or -1 if @p f is NULL.
 */
FGEN_API int tty_fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 0)));

/**
 * A vprintf like routine to output to tty file descriptor
 *
 * @param fmt
 *   The fgen_printf() like format string.
 * @param ap
 *   The va_list pointer
 * @return
 *   The number of bytes written to the file descriptor.
 */
FGEN_API int tty_vprintf(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));

/**
 * A vfprintf like routine to output to tty file descriptor
 *
 * @param f
 *    The file descriptor pointer
 * @param fmt
 *   The fgen_printf() like format string.
 * @param ap
 *   The va_list pointer
 * @return
 *   The number of bytes written to the file descriptor.
 */
FGEN_API int tty_vfprintf(FILE *f, const char *fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));

#ifdef __cplusplus
}
#endif

#endif /* __FGEN_TTY_H_ */
