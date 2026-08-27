#!/bin/sh
set -eu

toolchain_label="$1"
build_dir="/workspace/build-${toolchain_label}"

cmake -S /workspace -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build_dir}" -- -j2
cd "${build_dir}"
ctest --output-on-failure
