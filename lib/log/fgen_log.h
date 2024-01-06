/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

#ifndef _FGEN_LOG_H_
#define _FGEN_LOG_H_

/**
 * @file
 *
 * FGEN Logs API
 *
 * This file provides a log API to FGEN applications.
 */

#include <stdio.h>             // for NULL
#include <stdarg.h>            // for va_list
#include <stdint.h>            // for uint32_t
#include <fgen_common.h>        // for FGEN_API

#ifdef __cplusplus
extern "C" {
#endif

#define foreach_fgen_log_level \
    _(1, EMERG, emerg)         \
    _(2, ALERT, alert)         \
    _(3, CRIT, crit)           \
    _(4, ERR, err)             \
    _(5, WARNING, warn)        \
    _(6, NOTICE, notice)       \
    _(7, INFO, info)           \
    _(8, DEBUG, debug)         \
    _(9, LAST, last)

/* Can't use 0, as it gives compiler warnings */
/*
 * The log levels are defined as follows
 *  FGEN_LOG_EMERG   System is unusable.
 *  FGEN_LOG_ALERT   Action must be taken immediately.
 *  FGEN_LOG_CRIT    Critical conditions.
 *  FGEN_LOG_ERR     Error conditions.
 *  FGEN_LOG_WARNING Warning conditions.
 *  FGEN_LOG_NOTICE  Normal but significant condition.
 *  FGEN_LOG_INFO    Informational.
 *  FGEN_LOG_DEBUG   Debug-level messages.
 *  FGEN_LOG_LAST
 */
enum {
#define _(n, uc, lc) FGEN_LOG_##uc = n,
    foreach_fgen_log_level
#undef _
};

/**
 * Set the log level for a given type.
 *
 * @param level
 *   The level to be set.
 */
FGEN_API void fgen_log_set_level(uint32_t level);

/**
 * Set the log level by string.
 *
 * @param log_level
 *   The level to be set.
 *
 * @return
 *  0 for success, 1 on error.
 */
FGEN_API int fgen_log_set_level_str(char *log_level);

/**
 * Get the log level.
 *
 * @return
 *   The current log level.
 */
FGEN_API uint32_t fgen_log_get_level(void);

/**
 * Generates a log message.
 *
 * The message will be sent to stdout.
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global fgen_loglevel variable.
 *
 * The preferred alternative is the FGEN_LOG() macro because it adds the
 * level in the logged string and sets the function name and line number
 * automatically.
 *
 * @param level
 *   Log level. A value between FGEN_LOG_EMERG (1) and FGEN_LOG_DEBUG (8).
 * @param func
 *   Function name.
 * @param line
 *   Line Number.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - The number of characters printed on success.
 *   - A negative value on error.
 */
FGEN_API int fgen_log(uint32_t level, const char *func, int line, const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
    __attribute__((cold))
#endif
#endif
    __attribute__((format(printf, 4, 5)));

/**
 * Generates a log message regardless of log level.
 *
 * The message will be sent to stdout.
 *
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - The number of characters printed on success.
 *   - A negative value on error.
 */
FGEN_API int fgen_print(const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
    __attribute__((cold))
#endif
#endif
    __attribute__((format(printf, 1, 2)));

/**
 * Generates a log message.
 *
 * The message will be sent to stdout.
 *
 * The level argument determines if the log should be displayed or
 * not, depending on the global fgen_loglevel variable.
 *
 * The preferred alternative is the FGEN_LOG() macro because it adds the
 * level in the logged string and sets the function name and line number
 * automatically.
 *
 * @param level
 *   Log level. A value between FGEN_LOG_EMERG (1) and FGEN_LOG_DEBUG (8).
 * @param func
 *   Function name.
 * @param line
 *   Line Number.
 * @param format
 *   The format string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @param ap
 *   The va_list of the variable arguments required by the format.
 * @return
 *   - The number of characters printed on success.
 *   - A negative value on error.
 */
FGEN_API int fgen_vlog(uint32_t level, const char *func, int line, const char *format, va_list ap)
    __attribute__((format(printf, 4, 0)));

/**
 * Generates a log message.
 *
 * The FGEN_LOG() macro is a helper that prefixes the string with the log level,
 * function name, line number, and calls fgen_log().
 *
 * @param f
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param ...
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - The number of characters printed on success.
 *   - A negative value on error.
 */
#define FGEN_LOG(f, ...) fgen_log(FGEN_LOG_##f, __func__, __LINE__, #f ": " __VA_ARGS__)

/**
 * Generates a log message regardless of log level.
 *
 * @param f
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @param args
 *   Variable arguments depend on Application.
 * @return
 *   - The number of characters printed on success.
 *   - A negative value on error.
 */
#define FGEN_PRINT(f, args...) fgen_print(f, ##args)

/**
 * Macros for the normal cases with FGEN_LOG() levels
 */
#define FGEN_EMERG(...)  FGEN_LOG(EMERG, __VA_ARGS__)
#define FGEN_ALERT(...)  FGEN_LOG(ALERT, __VA_ARGS__)
#define FGEN_CRIT(...)   FGEN_LOG(CRIT, __VA_ARGS__)
#define FGEN_ERR(...)    FGEN_LOG(ERR, __VA_ARGS__)
#define FGEN_WARN(...)   FGEN_LOG(WARNING, __VA_ARGS__)
#define FGEN_NOTICE(...) FGEN_LOG(NOTICE, __VA_ARGS__)
#define FGEN_INFO(...)   FGEN_LOG(INFO, __VA_ARGS__)
#define FGEN_DEBUG(...)  FGEN_LOG(DEBUG, __VA_ARGS__)

/**
 * Generate an Error log message and return value
 *
 * Same as FGEN_LOG(ERR,...) define, but returns -1 to enable this style of coding.
 *   if (val == error) {
 *       FGEN_ERR("Error: Failed\n");
 *       return -1;
 *   }
 * Returning _val  to the calling function.
 */
#define FGEN_ERR_RET_VAL(_val, ...) \
    do {                           \
        FGEN_ERR(__VA_ARGS__);      \
        return _val;               \
    } while ((0))

/**
 * Generate an Error log message and return
 *
 * Same as FGEN_LOG(ERR,...) define, but returns to enable this style of coding.
 *   if (val == error) {
 *       FGEN_ERR("Error: Failed\n");
 *       return;
 *   }
 * Returning to the calling function.
 */
#define FGEN_RET(...) FGEN_ERR_RET_VAL(, __VA_ARGS__)

/**
 * Generate an Error log message and return -1
 *
 * Same as FGEN_LOG(ERR,...) define, but returns -1 to enable this style of coding.
 *   if (val == error) {
 *       FGEN_ERR("Error: Failed\n");
 *       return -1;
 *   }
 * Returning a -1 to the calling function.
 */
#define FGEN_ERR_RET(...) FGEN_ERR_RET_VAL(-1, __VA_ARGS__)

/**
 * Generate an Error log message and return NULL
 *
 * Same as FGEN_LOG(ERR,...) define, but returns NULL to enable this style of coding.
 *   if (val == error) {
 *       FGEN_ERR("Error: Failed\n");
 *       return NULL;
 *   }
 * Returning a NULL to the calling function.
 */
#define FGEN_NULL_RET(...) FGEN_ERR_RET_VAL(NULL, __VA_ARGS__)

/**
 * Generate a Error log message and goto label
 *
 * Same as FGEN_LOG(ERR,...) define, but goes to a label to enable this style of coding.
 *   if (error condition) {
 *       FGEN_ERR("Error: Failed\n");
 *       goto lbl;
 *   }
 */
#define FGEN_ERR_GOTO(lbl, ...) \
    do {                       \
        FGEN_ERR(__VA_ARGS__);  \
        goto lbl;              \
    } while ((0))

/**
 * Dump the stack of the calling core to the console.
 */
FGEN_API void fgen_dump_stack(void);

/**
 * Provide notification of a critical non-recoverable error and terminate
 * execution abnormally.
 *
 * Display the format string and its expanded arguments (printf-like).
 *
 * In a linux environment, this function dumps the stack and calls
 * abort() resulting in a core dump if enabled.
 *
 * The function never returns.
 *
 * @param format
 *   The format string, followed by the variable list of arguments.
 * @param args
 *    Variable arguments depend on Application.
 */
#define fgen_panic(format, args...) __fgen_panic(__func__, __LINE__, format "\n", ##args)

/**
 * Provide notification of a critical non-recoverable error and terminate
 * execution abnormally by calling exit(-1).
 *
 * Display the format string and its expanded arguments (printf-like).
 *
 * The function never returns.
 *
 * @param format
 *   The format string, followed by the variable list of arguments.
 * @param args
 *   Variable arguments depend on Application.
 */
#define fgen_exit(format, args...) __fgen_exit(__func__, __LINE__, format "\n", ##args)

#if FGEN_ENABLE_ASSERT
/**
 * Assert whether an expression is true when FGEN_ENABLE_ASSERT is non-zero
 *
 * Calls FGEN_VERIFY() to verify the expression. This macro does nothing
 * if FGEN_ENABLE_ASSERT is zero.
 *
 * @param exp
 *   The expression to test
 */
#define FGEN_ASSERT(exp) FGEN_VERIFY(exp)
#else
#define FGEN_ASSERT(exp) \
    do {                \
    } while (0)
#endif

/**
 * Verify an expression is true
 *
 * Calls fgen_panic() when the expression is false.
 *
 * @param exp
 *   The expression to test
 */
#define FGEN_VERIFY(exp)                              \
    do {                                             \
        if (unlikely(!(exp)))                        \
            fgen_panic("assert \"%s\" failed", #exp); \
    } while (0)

/**
 * Provide notification of a critical non-recoverable error and stop.
 *
 * This function should not be called directly. Refer to fgen_panic() macro
 * documentation.
 */
FGEN_API void __fgen_panic(const char *funcname, int line, const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
    __attribute__((cold))
#endif
#endif
    __attribute__((noreturn)) __attribute__((format(printf, 3, 4)));

/**
 * Provide notification of a critical non-recoverable error and stop.
 *
 * This function should not be called directly. Refer to fgen_exit() macro
 * documentation.
 */
FGEN_API void __fgen_exit(const char *func, int line, const char *format, ...)
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
    __attribute__((cold))
#endif
#endif
    __attribute__((noreturn)) __attribute__((format(printf, 3, 4)));

#ifdef __cplusplus
}
#endif

#endif /* _FGEN_LOG_H_ */
