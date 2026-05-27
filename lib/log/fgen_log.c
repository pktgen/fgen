/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief FGEN logging implementation.
 *
 * Implements the log level control, formatted log output, stack dump,
 * and panic / exit helpers declared in fgen_log.h.
 */

#include <stdio.h>           // for fflush, vprintf, NULL, stdout
#include <execinfo.h>        // for backtrace, backtrace_symbols
#include <stdarg.h>          // for va_list, va_end, va_start
#include <stdlib.h>          // for abort, exit, free
#include <fgen_strings.h>
#include <fgen_log.h>
#include <fgen_stdio.h>

/** @brief The max size of internal formatting buffers */
#define MAX_LOG_BUF_SIZE 1024

static uint32_t fgen_loglevel = FGEN_LOG_INFO; /**< Current global log level */

/**
 * @brief Set the global log level.
 *
 * @param level
 *   Desired log level, clamped to [FGEN_LOG_EMERG, FGEN_LOG_DEBUG].
 */
void
fgen_log_set_level(uint32_t level)
{
    if (level < FGEN_LOG_EMERG)
        fgen_loglevel = FGEN_LOG_EMERG;
    else if (level > FGEN_LOG_DEBUG)
        fgen_loglevel = FGEN_LOG_DEBUG;
    else
        fgen_loglevel = level;
}

/**
 * @brief Set the global log level by name string.
 *
 * @param log_level
 *   Case-insensitive level name, e.g. "DEBUG", "INFO", "ERR".
 * @return
 *   0 on success, 1 if the string is NULL or not recognised.
 */
int
fgen_log_set_level_str(char *log_level)
{
    if (!log_level)
        goto out;

#define _(n, uc, lc)                                             \
    if (!strcmp((const char *)fgen_strtoupper(log_level), #uc)) { \
        int _lvl = FGEN_LOG_##uc;                                 \
        fgen_log_set_level(_lvl);                                 \
        return 0;                                                \
    }
    foreach_fgen_log_level;
#undef _

out:
    return 1;
}

/**
 * @brief Return the current global log level.
 *
 * @return
 *   The current log level value.
 */
uint32_t
fgen_log_get_level(void)
{
    return fgen_loglevel;
}

/**
 * @brief Generate a log message using a va_list.
 *
 * @param level
 *   Log level; messages above the global level are suppressed.
 * @param func
 *   Name of the calling function (used in the output prefix).
 * @param line
 *   Source line number (used in the output prefix).
 * @param format
 *   printf-style format string.
 * @param ap
 *   Variable argument list corresponding to @p format.
 * @return
 *   Number of characters written on success, negative value on error.
 */
int
fgen_vlog(uint32_t level, const char *func, int line, const char *format, va_list ap)
{
    char buff[MAX_LOG_BUF_SIZE + 1];
    int n = 0;

    if (level > fgen_loglevel)
        return 0;

    memset(buff, 0, MAX_LOG_BUF_SIZE + 1);
    if (level <= FGEN_LOG_ERR)
        n = fgen_snprintf(buff, MAX_LOG_BUF_SIZE, "([red]%-24s[]:[green]%4d[]) %s", func, line,
                         format);
    else
        n = fgen_snprintf(buff, MAX_LOG_BUF_SIZE, "([yellow]%-24s[]:[green]%4d[]) %s", func, line,
                         format);
    if (n <= 0)
        return n;

    buff[n] = '\0';
    /* GCC allows the non-literal "buff" argument whereas clang does not */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif /* __clang__ */
    return vprintf(buff, ap);
#ifdef __clang__
#pragma clang diagnostic pop
#endif /* __clang__ */
}

/**
 * @brief Generate a log message.
 *
 * @param level
 *   Log level; messages above the global level are suppressed.
 * @param func
 *   Name of the calling function (used in the output prefix).
 * @param line
 *   Source line number (used in the output prefix).
 * @param format
 *   printf-style format string followed by variable arguments.
 * @return
 *   Number of characters written on success, negative value on error.
 */
int
fgen_log(uint32_t level, const char *func, int line, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = fgen_vlog(level, func, line, format, ap);
    va_end(ap);

    return ret;
}

/**
 * @brief Print a message unconditionally, bypassing the log level filter.
 *
 * @param format
 *   printf-style format string followed by variable arguments.
 * @return
 *   Number of characters written on success, negative value on error.
 */
int
fgen_print(const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vprintf(format, ap);
    va_end(ap);

    return ret;
}

/** @brief Maximum number of stack frames captured by fgen_dump_stack(). */
#define BACKTRACE_SIZE 256

/**
 * @brief Dump the call stack of the calling thread to stdout.
 */
void
fgen_dump_stack(void)
{
    void *func[BACKTRACE_SIZE];
    char **symb = NULL;
    int size;

    size = backtrace(func, BACKTRACE_SIZE);
    symb = backtrace_symbols(func, size);

    if (symb == NULL)
        return;

    fgen_printf("[yellow]Stack Frames[]\n");
    while (size > 0) {
        fgen_printf("  [cyan]%d[]: [green]%s[]\n", size, symb[size - 1]);
        size--;
    }
    fflush(stdout);

    free(symb);
}

/**
 * @brief Internal panic handler — do not call directly, use fgen_panic().
 *
 * Logs the formatted message at CRIT level, dumps the stack, then calls
 * abort() to generate a core dump.
 *
 * @param funcname
 *   Name of the function that triggered the panic.
 * @param line
 *   Source line number of the panic site.
 * @param format
 *   printf-style format string followed by variable arguments.
 */
void
__fgen_panic(const char *funcname, int line, const char *format, ...)
{
    va_list ap;

    fgen_printf("[yellow]*** [red]PANIC[]:\n");
    va_start(ap, format);
    fgen_vlog(FGEN_LOG_CRIT, funcname, line, format, ap);
    va_end(ap);

    fgen_dump_stack();
    abort();
}

/**
 * @brief Internal exit handler — do not call directly, use fgen_exit().
 *
 * Logs the formatted message at CRIT level then calls exit(-1).
 * No stack trace or core dump is produced.
 *
 * @param func
 *   Name of the function that triggered the exit.
 * @param line
 *   Source line number of the exit site.
 * @param format
 *   printf-style format string followed by variable arguments.
 */
void
__fgen_exit(const char *func, int line, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    fgen_vlog(FGEN_LOG_CRIT, func, line, format, ap);
    va_end(ap);

    exit(-1);
}
