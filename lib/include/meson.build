# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2025 Intel Corporation

sources = []
headers = files(
    'fgen_atomic.h',
    'fgen_buf.h',
    'fgen_byteorder.h',
    'fgen_common.h',
    'fgen_cycles.h',
    'fgen_gettid.h',
    'fgen_strings.h',
    'fgen_version.h',
    )
net_hdrs = files(
    'net/fgen_arp.h',
    'net/fgen_ether.h',
    'net/fgen_gre.h',
    'net/fgen_gtp.h',
    'net/fgen_icmp.h',
    'net/fgen_ip.h',
    'net/fgen_mpls.h',
    'net/fgen_sctp.h',
    'net/fgen_tcp.h',
    'net/fgen_udp.h',
    'net/fgen_vxlan.h',
    )

include = declare_dependency(include_directories: include_directories('.'))

install_headers(headers, subdir: meson.project_name().to_lower())
install_headers(net_hdrs, subdir: meson.project_name().to_lower() + '/net')
