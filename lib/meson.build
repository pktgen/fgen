# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

dirs = ['osal', 'log', 'mmap', 'utils', 'fgen']

headers = []
subdir('include')
install_headers(headers, subdir: meson.project_name().to_lower())

foreach d:dirs
    sources = []
    headers = []

    libname = d
    if libname != 'fgen'
        enabled_libs += libname
    endif
    subdir(d)

    install_headers(headers, subdir: meson.project_name().to_lower())
endforeach
