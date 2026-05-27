# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2026 Intel Corporation

if dpdk.found()
	sources = files('pktperf.c', 'parse.c', 'port.c', 'stats.c', 'utils.c')

	cflags = ['-DALLOW_EXPERIMENTAL_API'] # used for DPDK APIs

	deps = [dpdk, include, fgen, osal, mmap, utils]

	deps += [dependency('threads')]
	deps += [dependency('numa', required: true)]
	deps += [dependency('pcap', required: true)]
	deps += [cc.find_library('dl', required: false)]
	deps += [cc.find_library('m', required: false)]

	pktperf = executable('pktperf',
			sources,
			c_args: cflags,
			install: true,
			dependencies: deps)
else
    message('No DPDK found not building pktperf application')
endif
