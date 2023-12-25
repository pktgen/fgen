# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

sources = files(
    'fgen_stdio.c',
    'fgen_system.c',
    'fgen_tty.c',
    'netdev_funcs.c',
    'vt100_out.c',
    )
headers = files(
    'fgen_stdio.h',
    'fgen_system.h',
    'fgen_tty.h',
    'netdev_funcs.h',
    'vt100_out.h',
    )

deps = [include]

libosal = library(libname, sources, install: true, dependencies: deps)
osal = declare_dependency(link_with: libosal, include_directories: include_directories('.'))

fgen_libs += osal
