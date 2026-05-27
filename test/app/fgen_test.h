/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief FGEN frame generator test application entry point.
 */

#ifndef _FGEN_TEST_H_
#define _FGEN_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Entry point for the FGEN frame generator test.
 *
 * @param argc
 *   Argument count passed from the command line.
 * @param argv
 *   Argument vector passed from the command line.
 * @return
 *   0 on success, negative on failure.
 */
int fgen_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* _FGEN_TEST_H_ */
