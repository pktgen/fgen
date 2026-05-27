/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief FGEN test helper: start/end/summary lifecycle and assertion macros.
 *
 * Provides tst_info_t, tst_start(), tst_end(), tst_summary(), and a set of
 * TST_ASSERT_* macros used throughout the test suite.
 */

#ifndef _TESTING_INFO_H_
#define _TESTING_INFO_H_

#include <stdbool.h>        // for true, bool, false
#include <stddef.h>         // for NULL
#include <stdint.h>         // for uint32_t

#ifdef __cplusplus
extern "C" {
#endif

#include <fgen_log.h>

/** An exit code of 77 indicates a test is skipped */
#define EXIT_SKIPPED 77

/**
 * @brief Result codes passed to tst_end() to record pass/fail/skip status.
 */
enum {
    TST_FAILED = 0, /**< used for tst_end() when a test fails */
    TST_PASSED,     /**< used for tst_end() when a test passes */
    TST_SKIPPED,    /**< used for tst_end() when a test is skipped */
};

/**
 * @brief Per-test state allocated by tst_start() and freed by tst_end().
 */
typedef struct {
    char *name; /**< Name of the test (duplicated with strdup). */
    int lid;    /**< Lcore ID on which the test is running. */
    int sid;    /**< NUMA socket ID on which the test is running. */
} tst_info_t;

/**
 * Get an appropriate exit code used by a test application
 *
 * @return
 *   EXIT_FAILURE if any test fails
 *   EXIT_SKIPPED if any test is skipped, but none fail
 *   EXIT_SUCCESS if all tests pass
 */
int tst_exit_code(void);

/**
 * Print a summary of test results
 *
 * @return
 *   number of test failures
 */
uint32_t tst_summary(void);

/**
 * Start a test
 *
 * Also print out the socket and lcore ids plus the test's name. This function calls
 * abort() if memory cannot be allocated. Use the tst_end() function to free memory
 * allocated by this function.
 *
 * @param name
 *   The name of the test
 * @return
 *   pointer to tst_info structure
 */
tst_info_t *tst_start(const char *name);

/**
 * End a test
 *
 * This function records the pass/fail/skip status and frees the tst_info structure.
 *
 * @param tst
 *   pointer to tst_info structure
 * @param result
 *   TST_PASSED if the test passed
 *   TST_FAILED if the test failed
 *   TST_SKIPPED if the test was skipped
 */
void tst_end(tst_info_t *tst, int result);

/**
 * @brief Print a coloured PASS log line prefixed with "PASS - TEST:".
 *
 * @param fmt
 *   printf-like format string.
 * @param ...
 *   Variable-length arguments matching @p fmt.
 */
void tst_ok(const char *fmt, ...) __attribute__((__format__(__printf__, 1, 0)));

/**
 * @brief Print a coloured FAIL log line prefixed with "FAIL - TEST:".
 *
 * @param fmt
 *   printf-like format string.
 * @param ...
 *   Variable-length arguments matching @p fmt.
 */
void tst_error(const char *fmt, ...) __attribute__((__format__(__printf__, 1, 0)));

/**
 * @brief Print a coloured INFO log line prefixed with "INFO - TEST:".
 *
 * @param fmt
 *   printf-like format string.
 * @param ...
 *   Variable-length arguments matching @p fmt.
 */
void tst_info(const char *fmt, ...) __attribute__((__format__(__printf__, 1, 0)));

/**
 * @brief Print a coloured SKIP log line prefixed with "SKIP - TEST:".
 *
 * @param fmt
 *   printf-like format string.
 * @param ...
 *   Variable-length arguments matching @p fmt.
 */
void tst_skip(const char *fmt, ...) __attribute__((__format__(__printf__, 1, 0)));

/**
 * @brief Assert @p cond; print @p msg and return void on failure.
 *
 * @param cond   Condition that must be true.
 * @param msg    printf-style failure message.
 * @param ...    Optional arguments for @p msg.
 */
#define TST_ASSERT_RETURN(cond, msg, ...)                                                   \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            fgen_printf("[yellow]Test assert %s line %d [red]failed[]: " msg "\n", __func__, \
                       __LINE__, ##__VA_ARGS__);                                            \
            return;                                                                         \
        }                                                                                   \
    } while (0)

/**
 * @brief Assert @p cond; print @p msg and goto @p lbl on failure.
 *
 * @param cond   Condition that must be true.
 * @param msg    printf-style failure message.
 * @param lbl    Label to jump to on assertion failure.
 * @param ...    Optional arguments for @p msg.
 */
#define TST_ASSERT_GOTO(cond, msg, lbl, ...)                                                \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            fgen_printf("[yellow]Test assert %s line %d [red]failed[]: " msg "\n", __func__, \
                       __LINE__, ##__VA_ARGS__);                                            \
            goto lbl;                                                                       \
        }                                                                                   \
    } while (0)

/**
 * @brief Assert @p cond; print @p msg and return -1 on failure.
 *
 * @param cond   Condition that must be true.
 * @param msg    printf-style failure message.
 * @param ...    Optional arguments for @p msg.
 */
#define TST_ASSERT(cond, msg, ...)                                                          \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            fgen_printf("[yellow]Test assert %s line %d [red]failed[]: " msg "\n", __func__, \
                       __LINE__, ##__VA_ARGS__);                                            \
            return -1;                                                                      \
        }                                                                                   \
    } while (0)

/**
 * @brief Assert @p cond; call @p _function(@p _arg) and return -1 on failure.
 *
 * @param cond       Condition that must be true.
 * @param msg        printf-style failure message.
 * @param _function  Cleanup function to call before returning.
 * @param _arg       Argument passed to @p _function.
 * @param ...        Optional arguments for @p msg.
 */
#define TST_ASSERT_AND_CLEANUP(cond, msg, _function, _arg, ...)                             \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            fgen_printf("[yellow]Test assert %s line %d [red]failed[]: " msg "\n", __func__, \
                       __LINE__, ##__VA_ARGS__);                                            \
            _function(_arg);                                                                \
            return -1;                                                                      \
        }                                                                                   \
    } while (0)

/** @brief Assert @p a == @p b; return -1 on failure. */
#define TST_ASSERT_EQUAL(a, b, msg, ...) TST_ASSERT(a == b, msg, ##__VA_ARGS__)

/** @brief Assert @p a != @p b; return -1 on failure. */
#define TST_ASSERT_NOT_EQUAL(a, b, msg, ...) TST_ASSERT(a != b, msg, ##__VA_ARGS__)

/** @brief Assert @p val == 0; return -1 on failure. */
#define TST_ASSERT_SUCCESS(val, msg, ...) TST_ASSERT(val == 0, msg, ##__VA_ARGS__)

/** @brief Assert @p val != 0; return -1 on failure. */
#define TST_ASSERT_FAIL(val, msg, ...) TST_ASSERT(val != 0, msg, ##__VA_ARGS__)

/** @brief Assert @p val == NULL; return -1 on failure. */
#define TST_ASSERT_NULL(val, msg, ...) TST_ASSERT(val == NULL, msg, ##__VA_ARGS__)

/** @brief Assert @p val != NULL; return -1 on failure. */
#define TST_ASSERT_NOT_NULL(val, msg, ...) TST_ASSERT(val != NULL, msg, ##__VA_ARGS__)

/** @brief Assert @p a == @p b; run cleanup and return -1 on failure. */
#define TST_ASSERT_EQUAL_AND_CLEANUP(a, b, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(a == b, msg, _function, _arg, ##__VA_ARGS__)

/** @brief Assert @p a != @p b; run cleanup and return -1 on failure. */
#define TST_ASSERT_NOT_EQUAL_AND_CLEANUP(a, b, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(a != b, msg, _function, _arg, ##__VA_ARGS__)

/** @brief Assert @p val == 0; run cleanup and return -1 on failure. */
#define TST_ASSERT_SUCCESS_AND_CLEANUP(val, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(val == 0, msg, _function, _arg, ##__VA_ARGS__)

/** @brief Assert @p val != 0; run cleanup and return -1 on failure. */
#define TST_ASSERT_FAIL_AND_CLEANUP(val, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(val != 0, msg, _function, _arg, ##__VA_ARGS__)

/** @brief Assert @p val == NULL; run cleanup and return -1 on failure. */
#define TST_ASSERT_NULL_AND_CLEANUP(val, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(val == NULL, msg, _function, _arg, ##__VA_ARGS__)

/** @brief Assert @p val != NULL; run cleanup and return -1 on failure. */
#define TST_ASSERT_NOT_NULL_AND_CLEANUP(val, msg, _function, _arg, ...) \
    TST_ASSERT_AND_CLEANUP(val != NULL, msg, _function, _arg, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* _TESTING_INFO_H_ */
