# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023-2024 Intel Corporation

project('fgen', 'C', 'cpp',
    # Get version number from file.
    # Fallback to "more" for Windows compatibility.
    version: run_command(find_program('cat', 'more'),
        files('VERSION'), check: true).stdout().strip(),
    license: 'BSD',
    default_options: [
        'buildtype=release',
        'default_library=shared',
        'warning_level=3',
        'werror=true'
    ],
    meson_version: '>= 1.2.0'
)

use_static_libs = get_option('default_library') == 'static'

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

fgen_source_root = meson.current_source_dir()
fgen_build_root = meson.current_build_dir()
enable_asserts = get_option('enable_asserts')
fgen_verbose = get_option('verbose')
fgen_libs = []
extra_ldflags = []
enabled_libs = []

fgen_conf = configuration_data()

machine_args = []

##################################################################
# set the major version, which might be used by pmds and libraries
# depending on the configuration options
pver = meson.project_version().split('.')
major_version = '@0@.@1@'.format(pver.get(0), pver.get(1))

# extract all version information into the build configuration
fgen_conf.set('FGEN_VER_YEAR', pver.get(0).to_int())
fgen_conf.set('FGEN_VER_MONTH', pver.get(1).to_int())
if pver.get(2).contains('-rc')
    rc_ver = pver.get(2).split('-rc')
    fgen_conf.set('FGEN_VER_MINOR', rc_ver.get(0).to_int())
    fgen_conf.set_quoted('FGEN_VER_SUFFIX', '-rc')
    fgen_conf.set('FGEN_VER_RELEASE', rc_ver.get(1).to_int())
else
    fgen_conf.set('FGEN_VER_MINOR', pver.get(2).to_int())
    fgen_conf.set_quoted('FGEN_VER_SUFFIX', '')
# for actual, non-rc releases, set the release value to 99 to ensure releases
# have higher version numbers than their respective release candidates
    fgen_conf.set('FGEN_VER_RELEASE', 99)
endif

fgen_conf.set10('FGEN_ENABLE_ASSERT', enable_asserts)

machine = get_option('machine')

# machine type 'default' is special, it defaults to the per arch agreed common
# minimal baseline needed for FGEN.
# That might not be the most optimized, but the most portable version while
# still being able to support the CPU features required for FGEN.
# This can be bumped up by the FGEN project, but it can never be an
# invariant like 'native'
if machine == 'default'
    if host_machine.cpu_family().startswith('x86')
        # matches the old pre-meson build systems default
        machine = 'corei7'
    endif
endif

fgen_conf.set('FGEN_MACHINE', machine)
fgen_conf.set('FGEN_CACHE_LINE_SIZE', 64)

machine_args += '-march=' + machine

toolchain = cc.get_id()
fgen_conf.set_quoted('FGEN_TOOLCHAIN', toolchain)
fgen_conf.set('FGEN_TOOLCHAIN_' + toolchain.to_upper(), 1)

fgen_conf.set('FGEN_ARCH_64', cc.sizeof('void *') == 8)

add_project_link_arguments('-Wl,--no-as-needed', language: 'c')
if get_option('buildtype') == 'release'
    add_project_arguments('-g', language: 'c')
endif

# use pthreads
add_project_link_arguments('-pthread', language: 'c')
extra_ldflags += '-pthread'

add_project_link_arguments('-lm', language: 'c')
extra_ldflags += '-lm'

# on Alpine, execinfo and backtrace are in a separate library
if not cc.has_header('execinfo.h')
    error('Missing execinfo.h. Try installing "libexecinfo-dev".')
endif

if dependency('libexecinfo', required : false, static : use_static_libs).found()
    add_project_link_arguments('-lexecinfo', language: 'c')
    extra_ldflags += '-lexecinfo'
endif

# if link_lib is empty, do not add it to project properties
add_project_link_arguments('-ldl', language: 'c')
extra_ldflags += '-ldl'

# check for libraries used in multiple places in FGEN
has_libnuma = 0
numa_dep = dependency('numa', required: false, static: use_static_libs)
if numa_dep.found() and cc.has_header('numaif.h')
    fgen_conf.set10('FGEN_HAS_LIBNUMA', true)
    has_libnuma = 1
    add_project_link_arguments('-lnuma', language: 'c')
    extra_ldflags += '-lnuma'
endif

# check for libbsd
libbsd = dependency('libbsd', required: true, static: use_static_libs)
if libbsd.found()
    add_project_link_arguments('-lbsd', language: 'c')
    fgen_conf.set('FGEN_USE_LIBBSD', 1)
endif

# check for libjson-c
json_c = dependency('json-c', required: true, static: use_static_libs)
if json_c.found()
    add_project_link_arguments('-ljson-c', language: 'c')
    fgen_conf.set('FGEN_USE_JSON_C', 1)
    extra_ldflags += '-ljson-c'
endif

# Check for libpcap for txgen
pcap_dep = dependency('libpcap', required: true, static: use_static_libs)
if pcap_dep.found()
    add_project_link_arguments('-lpcap', language: 'c')
    extra_ldflags += '-lpcap'
endif

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang'
    atomic_dep = dependency('atomic', required: false, method: 'pkg-config', static: use_static_libs)
    if not atomic_dep.found()
        atomic_dep = cc.find_library('atomic', required: true)
    endif
    add_project_link_arguments('-latomic', language: 'c')
    extra_ldflags += '-latomic'
endif

# enable extra warnings and disable any unwanted warnings
warning_flags = [
    # additional warnings in alphabetical order
    '-Wno-pedantic',
    '-Wcast-qual',
    '-Wdeprecated',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wpointer-arith',
    '-Wsign-compare',
    '-Wstrict-prototypes',
    '-Wundef',
    '-Wwrite-strings',

    # globally disabled warnings
    '-Wno-address-of-packed-member',
    '-Wno-packed-not-aligned',
    '-Wno-missing-field-initializers',
]
foreach arg: warning_flags
    if cc.has_argument(arg)
        add_project_arguments(arg, language: 'c')
    endif
    if cpp.has_argument(arg)
        add_project_arguments(arg, language: 'cpp')
    endif
endforeach

compile_time_cpuflags = []

# get binutils version for the workaround of Bug 97
ldver = run_command('ld', '-v', check: true).stdout().strip()
if ldver.contains('2.30') and cc.has_argument('-mno-avx512f')
    machine_args += '-mno-avx512f'
    message('Binutils 2.30 detected, disabling AVX512 support as workaround for bug #97')
endif
if ldver.contains('2.31') and cc.has_argument('-mno-avx512f')
    machine_args += '-mno-avx512f'
    message('Binutils 2.31 detected, disabling AVX512 support as workaround for bug #249')
endif

fgen_conf.set('FGEN_ARCH_X86', 1)
if fgen_conf.get('FGEN_ARCH_64')
    fgen_conf.set('FGEN_ARCH_X86_64', 1)
    fgen_conf.set('FGEN_ARCH', 'x86_64')
else
    fgen_conf.set('FGEN_ARCH_I686', 1)
    fgen_conf.set('FGEN_ARCH', 'i686')
endif

fgen_conf.set('FGEN_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# specify -D_GNU_SOURCE unconditionally
add_project_arguments('-D_GNU_SOURCE', language: 'c')

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.')
build_cfg_file = 'fgen_build_config.h'
add_project_arguments(['-include', build_cfg_file] + machine_args, language: 'c')
add_project_arguments(['-include', build_cfg_file] + machine_args, language: 'cpp')

# write the build config into the fgen include directory
configure_file(output: build_cfg_file,
    configuration: fgen_conf,
    install_dir: join_paths(get_option('includedir'), meson.project_name().to_lower()))

build_cfg = declare_dependency(include_directories: include_directories('.'))

##################################################################

subdir('lib')
subdir('docs')              # Build Docs
subdir('test')              # Build Tests
subdir('examples')          # Examples

libfgen_a = []
libfgen_so = []
foreach lib:enabled_libs
    libfgen_a += 'lib' + lib + '.a '
    libfgen_so += 'lib' + lib + '.so '
endforeach

fgen_a_name = 'libfgen.a'
fgen_so_name = 'libfgen.so'

mklib = find_program('tools/mklib.sh')
build_dir = meson.current_build_dir()

pkg = import('pkgconfig')

message('>>> Create pkg-config file')
pkg.generate(name: meson.project_name(),
    filebase: 'lib' + meson.project_name().to_lower(),
    version: meson.project_version(),
    libraries: ['-Wl,--as-needed'] + fgen_libs,
    libraries_private: ['-lfgen'] + ['-Wl,-Bdynamic'] + extra_ldflags,
    requires: libbsd, # if libbsd is not enabled, then this is blank
    description: '''Frame Generator library (FGEN).
Note that CFLAGS might contain an -march flag higher than typical baseline.
This is required for a number of static inline functions in the public headers.''',
    subdirs: ['fgen'],
    extra_cflags: ['-include', build_cfg_file] + machine_args,
    install_dir: 'lib/pkgconfig'
)
message('<<< Done pkg-config file')
