# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

dirs = ['include', 'osal', 'log', 'mmap', 'utils', 'fgen']

foreach d:dirs
    sources = []
    headers = []

    libname = d
    subdir(d)

    install_headers(headers, subdir: meson.project_name().to_lower())
endforeach
