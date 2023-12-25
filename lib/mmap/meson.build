# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

sources = files('fgen_mmap.c')
headers = files('fgen_mmap.h')

deps = [include, log]

libmmap = library(libname, sources, install: true, dependencies: deps)
mmap = declare_dependency(link_with: libmmap, include_directories: include_directories('.'))

fgen_libs += mmap
