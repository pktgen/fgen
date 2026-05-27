/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2026 Intel Corporation.
 */

#ifndef _FGEN_ATOMIC_H_
#define _FGEN_ATOMIC_H_

/**
 * @file fgen_atomic.h
 * @brief Atomic type and memory-order portability macros for C and C++.
 */

#ifndef __cplusplus
#include <stdatomic.h>
/**
 * @brief Expands to the C11 atomic type for @p X (e.g. @c atomic_int).
 *
 * @param X  The base type name (e.g. @c int, @c uint32_t).
 */
#define FGEN_ATOMIC(X)       atomic_##X
/**
 * @brief Expands to the C11 memory-order constant for @p X
 *        (e.g. @c memory_order_relaxed).
 *
 * @param X  The memory-order suffix (e.g. @c relaxed, @c acquire, @c release).
 */
#define FGEN_MEMORY_ORDER(X) memory_order_##X
#else
#include <atomic>
/**
 * @brief Expands to the C++ atomic type for @p X (e.g. @c std::atomic<int>).
 *
 * @param X  The base type name (e.g. @c int, @c uint32_t).
 */
#define FGEN_ATOMIC(X)       std::atomic<X>
/**
 * @brief Expands to the C++ memory-order constant for @p X
 *        (e.g. @c std::memory_order_relaxed).
 *
 * @param X  The memory-order suffix (e.g. @c relaxed, @c acquire, @c release).
 */
#define FGEN_MEMORY_ORDER(X) std::memory_order_##X
#endif

#endif /* _FGEN_ATOMIC_H_ */
