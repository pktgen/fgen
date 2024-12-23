..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2019-2025 Intel Corporation.

Ring PMD
========

Allows the creation of pseudo ethernet devices by providing PMD APIs to a fixed
size, lockless, multi/single producer, multi/single consumer FIFO (FGEN Ring)
that behaves as if it was a physical NIC. This allows for the running of FGEN
applications without any ethernet devices, instead a pair of FGEN rings can be
used.

Enqueuing and dequeuing items from a FGEN ring using the rings-based PMD may be
slower than using the native rings API. This is because FGEN Ethernet drivers
make use of function pointers to call the appropriate enqueue or dequeue
functions, while the FGEN ring specific functions are direct function calls in
the code and are often inlined by the compiler.
