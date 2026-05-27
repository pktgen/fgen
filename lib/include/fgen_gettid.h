/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2026 Intel Corporation
 */

#ifndef _FGEN_GETTID_H_
#define _FGEN_GETTID_H_

/**
 * @file fgen_gettid.h
 * @brief Portable @c gettid() wrapper for glibc versions older than 2.30.
 *
 * A native @c gettid() function was introduced in glibc 2.30.  To support
 * earlier versions this header provides an equivalent syscall wrapper.
 */

#ifdef __GLIBC_PREREQ
#if !__GLIBC_PREREQ(2, 30)

#include <sys/syscall.h>

/**
 * @brief Return the calling thread's kernel thread ID.
 *
 * This macro is defined only when the system glibc is older than 2.30 and
 * does not expose a native @c gettid() function.
 *
 * @return  Thread ID of the calling thread as a @c pid_t.
 */
#define gettid() syscall(SYS_gettid)

#endif /* !__GLIBC_PREREQ(2, 30) */
#endif /* __GLIBC_PREREQ */
#endif /* _FGEN_GETTID_H_ */
