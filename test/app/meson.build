# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

sources = files('fgen_test.c')

deps += [include, log, osal, mmap, tst_common, fgen]
cflags = []

executable('fgen_test',
    sources,
    c_args: cflags,
    install: true,
    dependencies: deps)
