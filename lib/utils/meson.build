# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2023 Intel Corporation

sources = files(
    'crc32.c',
#    'crc32_sse42.c',   # later
    'hexdump.c',
	)
headers = files(
    'crc32.h',
    'hexdump.h',
    )

deps += [include, log, osal]

libutils = library(libname, sources, install: true, dependencies: deps)
utils = declare_dependency(link_with: libutils, include_directories: include_directories('.'))

fgen_libs += utils
