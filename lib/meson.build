# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

dirs = ['include', 'osal', 'log', 'mmap', 'utils', 'core']

foreach d:dirs
    sources = []
    headers = []

    if d != 'include'
        libname = 'fgen_' + d
        enabled_libs += libname
    else
        libname = d
    endif
    subdir(d)

    install_headers(headers, subdir: meson.project_name().to_lower())
endforeach
