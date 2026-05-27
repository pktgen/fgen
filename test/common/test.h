/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2026 Intel Corporation
 */

/**
 * @file
 * @brief Common test framework helpers: assertion macros, test-case descriptors,
 *        and the unit-test suite runner interface.
 */

#ifndef _TEST_H_
#define _TEST_H_

#include <stddef.h>        // for NULL, size_t
#include <sys/queue.h>
#include <bsd/sys/queue.h>        // for TAILQ_ENTRY, TAILQ_HEAD
#include <stdio.h>                // for FILE, size_t
#include <stdlib.h>               // for NULL, EXIT_SUCCESS

#include <fgen_common.h>           // for FGEN_INIT, FGEN_STR
#include <fgen_log.h>
#include <hexdump.h>

#define TEST_SUCCESS EXIT_SUCCESS /**< Return value indicating a test passed. */
#define TEST_FAILED  -1          /**< Return value indicating a test failed. */
#define TEST_SKIPPED 77          /**< Return value indicating a test was skipped. */

/* Before including test.h file you can define
 * TEST_TRACE_FAILURE(_file, _line, _func) macro to better trace/debug test
 * failures. Mostly useful in test development phase. */
#ifndef TEST_TRACE_FAILURE
/** @brief Optional hook called on test failure; define before including test.h to customise. */
#define TEST_TRACE_FAILURE(_file, _line, _func)
#endif

//#include "fgen_test.h"        // for FGEN_TEST_ASSERT, FGEN_TEST_ASSERT_EQUAL

#define TEST_ASSERT FGEN_TEST_ASSERT

#define TEST_ASSERT_EQUAL FGEN_TEST_ASSERT_EQUAL

/**
 * @brief Assert that two byte buffers contain identical content.
 *
 * @param a
 *   Pointer to the first buffer.
 * @param b
 *   Pointer to the second buffer.
 * @param len
 *   Number of bytes to compare.
 * @param msg
 *   printf-style failure message.
 * @param ...
 *   Optional arguments for @p msg.
 */
/* Compare two buffers (length in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, len, msg, ...)                                 \
    do {                                                                                   \
        if (memcmp(a, b, len)) {                                                           \
            fgen_printf("[yellow]TestCase %s() line %d [red]failed[]: " msg "\n", __func__, \
                       __LINE__, ##__VA_ARGS__);                                           \
            TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);                              \
            return TEST_FAILED;                                                            \
        }                                                                                  \
    } while (0)

/**
 * @brief Assert that two byte buffers are equal starting from a byte offset.
 *
 * @param a
 *   Pointer to the first buffer.
 * @param b
 *   Pointer to the second buffer.
 * @param len
 *   Number of bytes to compare after the offset.
 * @param off
 *   Byte offset into both buffers at which the comparison starts.
 * @param msg
 *   printf-style failure message.
 * @param ...
 *   Optional arguments for @p msg.
 */
/* Compare two buffers with offset (length and offset in bytes) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_OFFSET(a, b, len, off, msg, ...)     \
    do {                                                                   \
        const uint8_t *_a_with_off = (const uint8_t *)a + off;             \
        const uint8_t *_b_with_off = (const uint8_t *)b + off;             \
        TEST_ASSERT_BUFFERS_ARE_EQUAL(_a_with_off, _b_with_off, len, msg); \
    } while (0)

/**
 * @brief Assert that two bit buffers contain identical content.
 *
 * Compares @p len bits; the final partial byte (if any) is masked before comparison.
 *
 * @param a
 *   Pointer to the first buffer.
 * @param b
 *   Pointer to the second buffer.
 * @param len
 *   Number of bits to compare.
 * @param msg
 *   printf-style failure message.
 * @param ...
 *   Optional arguments for @p msg.
 */
/* Compare two buffers (length in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(a, b, len, msg, ...)                                 \
    do {                                                                                       \
        uint8_t _last_byte_a, _last_byte_b;                                                    \
        uint8_t _last_byte_mask, _last_byte_bits;                                              \
        TEST_ASSERT_BUFFERS_ARE_EQUAL(a, b, (len >> 3), msg);                                  \
        if (len % 8) {                                                                         \
            _last_byte_bits = len % 8;                                                         \
            _last_byte_mask = ~((1 << (8 - _last_byte_bits)) - 1);                             \
            _last_byte_a    = ((const uint8_t *)a)[len >> 3];                                  \
            _last_byte_b    = ((const uint8_t *)b)[len >> 3];                                  \
            _last_byte_a &= _last_byte_mask;                                                   \
            _last_byte_b &= _last_byte_mask;                                                   \
            if (_last_byte_a != _last_byte_b) {                                                \
                fgen_printf("[yellow]TestCase %s() line %d [red]failed[]: " msg "\n", __func__, \
                           __LINE__, ##__VA_ARGS__);                                           \
                TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);                              \
                return TEST_FAILED;                                                            \
            }                                                                                  \
        }                                                                                      \
    } while (0)

/**
 * @brief Assert that two bit buffers are equal starting from a bit offset.
 *
 * @param a
 *   Pointer to the first buffer.
 * @param b
 *   Pointer to the second buffer.
 * @param len
 *   Number of bits to compare after the offset.
 * @param off
 *   Bit offset into both buffers at which the comparison starts.
 * @param msg
 *   printf-style failure message.
 * @param ...
 *   Optional arguments for @p msg.
 */
/* Compare two buffers with offset (length and offset in bits) */
#define TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(a, b, len, off, msg, ...)                         \
    do {                                                                                           \
        uint8_t _first_byte_a, _first_byte_b;                                                      \
        uint8_t _first_byte_mask, _first_byte_bits;                                                \
        uint32_t _len_without_first_byte = (off % 8) ? len - (8 - (off % 8)) : len;                \
        uint32_t _off_in_bytes           = (off % 8) ? (off >> 3) + 1 : (off >> 3);                \
        const uint8_t *_a_with_off       = (const uint8_t *)a + _off_in_bytes;                     \
        const uint8_t *_b_with_off       = (const uint8_t *)b + _off_in_bytes;                     \
        TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(_a_with_off, _b_with_off, _len_without_first_byte, msg); \
        if (off % 8) {                                                                             \
            _first_byte_bits = 8 - (off % 8);                                                      \
            _first_byte_mask = (1 << _first_byte_bits) - 1;                                        \
            _first_byte_a    = *(_a_with_off - 1);                                                 \
            _first_byte_b    = *(_b_with_off - 1);                                                 \
            _first_byte_a &= _first_byte_mask;                                                     \
            _first_byte_b &= _first_byte_mask;                                                     \
            if (_first_byte_a != _first_byte_b) {                                                  \
                fgen_printf("[yellow]TestCase %s() line %d [red]failed[]: " msg "\n", __func__,     \
                           __LINE__, ##__VA_ARGS__);                                               \
                TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);                                  \
                return TEST_FAILED;                                                                \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define TEST_ASSERT_NOT_EQUAL FGEN_TEST_ASSERT_NOT_EQUAL

#define TEST_ASSERT_SUCCESS FGEN_TEST_ASSERT_SUCCESS

#define TEST_ASSERT_FAIL FGEN_TEST_ASSERT_FAIL

#define TEST_ASSERT_NULL FGEN_TEST_ASSERT_NULL

#define TEST_ASSERT_NOT_NULL FGEN_TEST_ASSERT_NOT_NULL

/**
 * @brief Descriptor for a single unit test case.
 */
struct unit_test_case {
    int (*setup)(void);    /**< Optional per-test setup function; NULL if not needed. */
    void (*teardown)(void);/**< Optional per-test teardown function; NULL if not needed. */
    int (*testcase)(void); /**< The test function; returns TEST_SUCCESS/FAILED/SKIPPED. */
    const char *name;      /**< Human-readable name of the test case. */
    unsigned enabled;      /**< Non-zero if this test should be executed. */
};

/** @brief Declare an enabled test case with no setup/teardown. */
#define TEST_CASE(fn)          \
    {                          \
        NULL, NULL, fn, #fn, 1 \
    }

/** @brief Declare an enabled test case with an explicit display name. */
#define TEST_CASE_NAMED(name, fn) \
    {                             \
        NULL, NULL, fn, name, 1   \
    }

/** @brief Declare an enabled test case with explicit setup and teardown functions. */
#define TEST_CASE_ST(setup, teardown, testcase) \
    {                                           \
        setup, teardown, testcase, #testcase, 1 \
    }

/** @brief Declare a disabled (skipped) test case with no setup/teardown. */
#define TEST_CASE_DISABLED(fn) \
    {                          \
        NULL, NULL, fn, #fn, 0 \
    }

/** @brief Declare a disabled (skipped) test case with explicit setup and teardown functions. */
#define TEST_CASE_ST_DISABLED(setup, teardown, testcase) \
    {                                                    \
        setup, teardown, testcase, #testcase, 0          \
    }

/** @brief Sentinel entry that marks the end of a unit_test_case array. */
#define TEST_CASES_END()          \
    {                             \
        NULL, NULL, NULL, NULL, 0 \
    }

/**
 * @brief Hex-dump @p buf to @p file with an optional title.
 *
 * Thin wrapper around fgen_hexdump() for use in test code.
 *
 * @param file
 *   Output FILE stream (e.g. stdout or stderr).
 * @param title
 *   Optional label printed before the hex dump; may be NULL.
 * @param buf
 *   Pointer to the data to dump.
 * @param len
 *   Number of bytes to dump.
 */
static inline void
debug_hexdump(FILE *file, const char *title, const void *buf, size_t len)
{
    fgen_hexdump(file, title, buf, len);
}

/**
 * @brief Descriptor for a collection of unit test cases forming a named suite.
 */
struct unit_test_suite {
    const char *suite_name;              /**< Human-readable name of the test suite. */
    int (*setup)(void);                  /**< Optional suite-level setup function; NULL if not needed. */
    void (*teardown)(void);              /**< Optional suite-level teardown function; NULL if not needed. */
    struct unit_test_case unit_test_cases[]; /**< Flexible array of test cases, terminated by TEST_CASES_END(). */
};

/**
 * @brief Execute all test cases in a unit_test_suite and print a summary.
 *
 * @param suite
 *   Pointer to the test suite descriptor to run.
 * @return
 *   TEST_SUCCESS if all enabled tests passed, TEST_SKIPPED if all were skipped,
 *   or TEST_FAILED if any test failed.
 */
int unit_test_suite_runner(struct unit_test_suite *suite);

/** @brief Result code of the most recently completed test suite run. */
extern int last_test_result;

/** @brief Environment variable used to detect recursive test invocations. */
#define RECURSIVE_ENV_VAR "FGEN_TEST_RECURSIVE"

/** @brief Program name string, set during initialisation. */
extern const char *prgname;

/** @brief Signature of a test command callback. */
typedef int(test_callback)(void);

TAILQ_HEAD(test_commands_list, test_command);

/**
 * @brief Entry in the global list of registered test commands.
 */
struct test_command {
    TAILQ_ENTRY(test_command) next; /**< Linked-list linkage. */
    const char *command;            /**< Command string used to select this test. */
    test_callback *callback;        /**< Function invoked when the command is selected. */
};

/**
 * @brief Register a test_command entry in the global command list.
 *
 * @param t
 *   Pointer to the statically allocated test_command to add.
 */
void add_test_command(struct test_command *t);

/**
 * @brief Register a test function and associate it with a command string.
 *
 * Declares a static test_command structure and an initialiser constructor
 * that inserts it into the global test command list at startup.
 *
 * @param cmd
 *   Token used as both the command string and part of the generated symbol names.
 * @param func
 *   test_callback function to invoke when @p cmd is selected.
 */
/* Register a test function with its command string */
#define REGISTER_TEST_COMMAND(cmd, func)             \
    static struct test_command test_struct_##cmd = { \
        .command  = FGEN_STR(cmd),                    \
        .callback = func,                            \
    };                                               \
    FGEN_INIT(test_register_##cmd)                    \
    {                                                \
        add_test_command(&test_struct_##cmd);        \
    }

#endif
