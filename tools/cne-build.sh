#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2023 Intel Corporation

# A simple script to help build FGEN using meson/ninja tools.
# The script also creates an installed directory called usr/local.
# The install directory will contain all of the includes and libraries
# for external applications to build and link with FGEN.
#
# using 'cne-build.sh help' or 'cne-build.sh -h' or 'cne-build.sh --help' to see help information.
#

currdir=$(pwd)
script_dir=$(cd "${BASH_SOURCE[0]%/*}" && pwd -P)
sdk_dir="${FGEN_SDK_DIR:-${script_dir%/*}}"
target_dir="${FGEN_TARGET_DIR:-usr/local}"
build_dir="${FGEN_BUILD_DIR:-${currdir}/builddir}"
install_path="${FGEN_DEST_DIR:-${currdir}}"

export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-/usr/lib64/pkgconfig}"

buildtype="release"
static=""
coverity=""
configure=""

if [[ "${build_dir}" = /* ]]; then
    # absolute path to build dir. Don't prepend workdir.
    build_path=$build_dir
else
    build_path=${currdir}/$build_dir
fi
if [[ ! "${install_path}" = /* ]]; then
    # relative path for install path detected
    # prepend with currdir
    install_path=${currdir}/${install_path}
fi
if [[ "${target_dir}" = .* ]]; then
    echo "target_dir starts with . or .. if different install prefix required then use FGEN_DEST_DIR instead";
    exit 1;
fi
if [[ "${target_dir}" = /* ]]; then
    echo "target_dir absolute path detected removing leading '/'"
    export target_dir=${target_dir##/}
fi
target_path=${install_path%/}/${target_dir%/}

echo "Build environment variables and paths:"
echo "  FGEN_SDK_DIR     : $sdk_dir"
echo "  FGEN_TARGET_DIR  : $target_dir"
echo "  FGEN_BUILD_DIR   : $build_dir"
echo "  FGEN_DEST_DIR    : $install_path"
echo "  PKG_CONFIG_PATH : $PKG_CONFIG_PATH"
echo "  build_path      : $build_path"
echo "  target_path     : $target_path"
echo ""

function run_meson() {
    btype="-Dbuildtype=$buildtype"
    meson $configure $static $coverity $btype --prefix="/$target_dir" "$build_path" "$sdk_dir"
}

function ninja_build() {
    echo ">>> Ninja build in $build_path buildtype= $buildtype"

    if [[ -d $build_path ]] || [[ -f $build_path/build.ninja ]]; then
        # add reconfigure command if meson dir already exists
        configure="configure"
        # sdk_dir must be empty if we're reconfiguring
        sdk_dir=""
    fi
    run_meson

    if ! ninja -C "$build_path"; then
        return 1
    fi

    return 0
}

function ninja_build_docs() {
    echo ">>> Ninja build documents in $build_path"

    if [[ ! -d $build_path ]] || [[ ! -f $build_path/build.ninja ]]; then
        run_meson
    fi

    if ! ninja -C "$build_path" doc; then
        return 1;
    fi
    return 0
}

function build_rust_apps() {
    echo ">>> Build rust applications"
    # Check if Cargo is installed.
    if ! command -v cargo &> /dev/null; then
        echo "Cargo not found.Install Cargo"
        return 1
    fi
    # Build rust applications
    if [[ -d ${currdir}/lang/rs ]]; then
        cd "${currdir}/lang/rs" || return 1
        cargo_build_rust_app
    fi
}

function cargo_build_rust_app() {
    if [ "$buildtype" == "release" ]; then
        if ! cargo build --release; then
            echo "Cargo builld rust app failed"
            return 1
        fi
    else
        if ! cargo build; then
            echo "Cargo builld rust app failed"
            return 1
        fi
    fi

    return 0
}

function clean_rust_apps() {
    echo ">>> Clean rust applications"
    # Check if Cargo is installed.
    if ! command -v cargo &> /dev/null; then
        echo "Cargo not found.Install Cargo"
        return 1
    fi
    # Clean rust applications.
    if [[ -d ${currdir}/lang/rs ]]; then
        cd "${currdir}/lang/rs" || return 1
        cargo_clean_rust_app
    fi
}

function cargo_clean_rust_app() {
    if ! cargo clean -v; then
        return 1;
    fi
    return 0
}


ninja_install() {
    echo ">>> Ninja install to $target_path"

    if [[ $verbose = true ]]; then
        if ! DESTDIR=$install_path ninja -C "$build_path" install; then
            echo "*** Install failed!!"
            return 1
        fi
    else
        if ! DESTDIR=$install_path ninja -C "$build_path" install > /dev/null; then
            echo "*** Install failed!!"
            return 1
        fi
    fi

    return 0
}

ninja_uninstall() {
    echo ">>> Ninja uninstall to $target_path"

    if [[ $verbose = true ]]; then
        if ! DESTDIR=$install_path ninja -C "$build_path" uninstall; then
            echo "*** Uninstall failed!!"
            return 1;
        fi
    else
        if ! DESTDIR=$install_path ninja -C "$build_path" uninstall > /dev/null; then
            echo "*** Uninstall failed!!"
            return 1;
        fi
    fi

    return 0
}

usage() {
    echo " Usage: Build FGEN using Meson/Ninja tools"
    echo "  ** Must be in the top level directory for FGEN"
    echo "     This tool is in tools/cne-build.sh, but use 'make' which calls this script"
    echo "     Use 'make' to build FGEN as it allows for multiple targets i.e. 'make clean debug'"
    echo ""
    echo "     FGEN_SDK_DIR    - FGEN source directory path (default: current working directory)"
    echo "     FGEN_TARGET_DIR - Target directory for installed files (default: usr/local)"
    echo "     FGEN_BUILD_DIR  - Build directory name (default: builddir)"
    echo "     FGEN_DEST_DIR   - Destination directory (default: current working directory)"
    echo ""
    echo "  cne-build.sh     - create the 'build_dir' directory if not present and compile FGEN"
    echo "                     If the 'build_dir' directory exists it will use ninja to build FGEN"
    echo "                     without running meson unless one of the meson.build files were changed"
    echo "    -v             - Enable verbose output"
    echo "    build          - build FGEN using the 'build_dir' directory"
    echo "    static         - build FGEN static using the 'build_dir' directory, 'make static build'"
    echo "    debug          - turn off optimization, may need to do 'clean' then 'debug' the first time"
    echo "    debugopt       - turn optimization on with -O2, may need to do 'clean' then 'debugopt'"
    echo "                     the first time"
    echo "    clean          - remove the 'build_dir' directory then exit"
    echo "    install        - install the includes/libraries into 'target_dir' directory"
    echo "    uninstall      - uninstall the includes/libraries from 'target_dir' directory"
    echo "    coverity       - (internal) build using coverity tool"
    echo "    docs           - create the document files"
    echo "    rust-app       - Build Rust application"
    echo "    rust-app-clean - Clean Rust application"
    exit
}

verbose=false

for cmd in "$@"
do
    case "$cmd" in
    'help' | '-h' | '--help')
        usage
        ;;

    '-v' | '--verbose')
        verbose=true
        ;;

    'static')
        echo ">>> Static  build in $build_path"
        static="-Ddefault_library=static"
        ;;

    'build')
        echo ">>> Release build in $build_path"
        ninja_build
        ;;

    'coverity')
        echo ">>> Build for Coverity in $build_path"
        coverity="-Dcoverity=true"
        ninja_build
        ;;

    'debug')
        echo ">>> Debug build in $build_path"
        buildtype="debug"
        ninja_build
        ;;

    'debugopt')
        echo ">>> Debug Optimized build in $build_path"
        buildtype="debugoptimized"
        ninja_build
        ;;

    'clean')
    echo "*** Removing $build_path directory"
        rm -fr "$build_path"
        ;;

    'uninstall')
        echo "*** Uninstalling $target_path directory"
        ninja_uninstall
        exit
        ;;

    'install')
        echo ">>> Install the includes/libraries into $target_path directory"
        ninja_install
        ;;

    'docs')
        echo ">>> Create the documents in $build_path directory"
        ninja_build_docs
        ;;

    'rust-app')
        echo ">>> Build Rust application. This should be run after building and installing FGEN"
        build_rust_apps
        ;;

    'rust-app-clean')
        echo ">>> Clean Rust application"
        clean_rust_apps
        ;;

    *)
        if [[ $# -gt 0 ]]; then
            usage
        else
            echo ">>> Build and install FGEN"
            ninja_build && ninja_install
        fi
        ;;
    esac
done
