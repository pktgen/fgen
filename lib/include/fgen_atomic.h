/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation.
 */

#ifndef _FGEN_ATOMIC_H_
#define _FGEN_ATOMIC_H_

/**
 * @file
 */

#ifndef __cplusplus
#include <stdatomic.h>
#define FGEN_ATOMIC(X)       atomic_##X
#define FGEN_MEMORY_ORDER(X) memory_order_##X
#else
#include <atomic>
#define FGEN_ATOMIC(X)       std::atomic<X>
#define FGEN_MEMORY_ORDER(X) std::memory_order_##X
#endif

#endif /* _FGEN_ATOMIC_H_ */
