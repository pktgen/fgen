/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation
 */

#include <errno.h>        // for ENOTSUP

#include <fgen_stdio.h>   // for fgen_printf
#include "test.h"

int last_test_result;

int
unit_test_suite_runner(struct unit_test_suite *suite)
{
    int test_success;
    unsigned int total = 0, executed = 0, skipped = 0;
    unsigned int succeeded = 0, failed = 0, unsupported = 0;
    const char *status;

    if (suite->suite_name) {
        fgen_printf("[green] + ------------------------------------------------------- +[]\n");
        fgen_printf("[green] + Test Suite : [magenta]%s[]\n", suite->suite_name);
    }

    if (suite->setup) {
        test_success = suite->setup();
        if (test_success != 0) {
            /*
             * setup did not pass, so count all enabled tests and
             * mark them as failed/skipped
             */
            while (suite->unit_test_cases[total].testcase) {
                if (!suite->unit_test_cases[total].enabled || test_success == TEST_SKIPPED)
                    skipped++;
                else
                    failed++;
                total++;
            }
            goto suite_summary;
        }
    }

    fgen_printf("[green] + ------------------------------------------------------- +[]\n");

    while (suite->unit_test_cases[total].testcase) {
        if (!suite->unit_test_cases[total].enabled) {
            skipped++;
            total++;
            continue;
        } else {
            executed++;
        }

        /* run test case setup */
        if (suite->unit_test_cases[total].setup)
            test_success = suite->unit_test_cases[total].setup();
        else
            test_success = TEST_SUCCESS;

        if (test_success == TEST_SUCCESS) {
            /* run the test case */
            test_success = suite->unit_test_cases[total].testcase();
            if (test_success == TEST_SUCCESS)
                succeeded++;
            else if (test_success == TEST_SKIPPED)
                skipped++;
            else if (test_success == -ENOTSUP)
                unsupported++;
            else
                failed++;
        } else if (test_success == -ENOTSUP) {
            unsupported++;
        } else {
            failed++;
        }

        /* run the test case teardown */
        if (suite->unit_test_cases[total].teardown)
            suite->unit_test_cases[total].teardown();

        if (test_success == TEST_SUCCESS)
            status = "succeeded";
        else if (test_success == TEST_SKIPPED)
            status = "skipped";
        else if (test_success == -ENOTSUP)
            status = "unsupported";
        else
            status = "failed";

        fgen_printf("[green] + TestCase [[magenta]%2d[]] : %s [red]%s[]\n", total,
                   suite->unit_test_cases[total].name, status);

        total++;
    }

    /* Run test suite teardown */
    if (suite->teardown)
        suite->teardown();

    goto suite_summary;

suite_summary:
    fgen_printf("[green] + ------------------------------------------------------- +[]\n");
    fgen_printf("[green] + Test Suite Summary\n");
    fgen_printf("[green] + Tests Total :       [magenta]%2d[]\n", total);
    fgen_printf("[green] + Tests Skipped :     [magenta]%2d[]\n", skipped);
    fgen_printf("[green] + Tests Executed :    [magenta]%2d[]\n", executed);
    fgen_printf("[green] + Tests Unsupported:  [magenta]%2d[]\n", unsupported);
    fgen_printf("[green] + Tests Passed :      [magenta]%2d[]\n", succeeded);
    fgen_printf("[green] + Tests Failed :      [magenta]%2d[]\n", failed);
    fgen_printf("[green] + ------------------------------------------------------- +[]\n");

    last_test_result = failed;

    if (failed)
        return TEST_FAILED;
    if (total == skipped)
        return TEST_SKIPPED;
    return TEST_SUCCESS;
}
