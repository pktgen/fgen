# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Intel Corporation

sources = files('fgen.c', 'encode.c', 'decode.c')
headers = files('fgen.h')

deps = [include, log, osal, mmap, utils]
libfgen = library(libname, sources, install: true, dependencies: deps)
fgen = declare_dependency(link_with: libfgen, include_directories: include_directories('.'))

fgen_libs += fgen
