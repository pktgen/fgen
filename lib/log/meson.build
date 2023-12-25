# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

sources = files('fgen_log.c')
headers = files('fgen_log.h')

deps = [include, osal]

liblog = library(libname, sources, install: true, dependencies: deps)
log = declare_dependency(link_with: liblog, include_directories: include_directories('.'))

fgen_libs += log
