/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

#include <stdio.h>           // for fflush, vprintf, NULL, stdout
#include <execinfo.h>        // for backtrace, backtrace_symbols
#include <stdarg.h>          // for va_list, va_end, va_start
#include <stdlib.h>          // for abort, exit, free
#include <fgen_strings.h>
#include <fgen_log.h>
#include <fgen_stdio.h>

#define MAX_LOG_BUF_SIZE 1024 /** The max size of internal buffers */

static uint32_t fgen_loglevel = FGEN_LOG_INFO;

/* Set global log level */
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

/* Get global log level */
uint32_t
fgen_log_get_level(void)
{
    return fgen_loglevel;
}

/*
 * Generates a log message.
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

/*
 * Generates a log message.
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

/*
 * Generates a log message regardless of log level.
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

#define BACKTRACE_SIZE 256

/* dump the stack of the calling core */
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

/* call abort(), it will generate a coredump if enabled */
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

/*
 * Like fgen_panic this terminates the application. However, no traceback is
 * provided and no core-dump is generated.
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
