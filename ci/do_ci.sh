#!/bin/bash

set -e

[ -z "${SRC_DIR}" ] && export SRC_DIR="`pwd`"
[ -z "${BUILD_DIR}" ] && export BUILD_DIR=/build
mkdir -p "${BUILD_DIR}"

if [[ "$1" == "cmake.debug" ]]; then
  cd "${BUILD_DIR}"
  cmake -DCMAKE_BUILD_TYPE=Debug  \
        "${SRC_DIR}"
  make
  make test
  exit 0
elif [[ "$1" == "cmake.no_grpc" ]]; then
  cd "${BUILD_DIR}"
  cmake -DCMAKE_BUILD_TYPE=Debug  \
        -DWITH_GRPC=OFF \
        "${SRC_DIR}"
  make
  make test
  exit 0
elif [[ "$1" == "cmake.asan" ]]; then
  cd "${BUILD_DIR}"
  cmake -DCMAKE_BUILD_TYPE=Debug  \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -fsanitize=address"  \
        -DCMAKE_SHARED_LINKER_FLAGS="-fno-omit-frame-pointer -fsanitize=address" \
        -DCMAKE_EXE_LINKER_FLAGS="-fno-omit-frame-pointer -fsanitize=address" \
        "${SRC_DIR}"
  make
  make test
  exit 0
elif [[ "$1" == "cmake.tsan" ]]; then
  cd "${BUILD_DIR}"
# Testing with dynamic load seems to have some issues with TSAN so turn off
# dynamic loading in this test for now.
  cmake -DCMAKE_BUILD_TYPE=Debug  \
        -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -fsanitize=thread"  \
        -DCMAKE_SHARED_LINKER_FLAGS="-fno-omit-frame-pointer -fsanitize=thread" \
        -DCMAKE_EXE_LINKER_FLAGS="-fno-omit-frame-pointer -fsanitize=thread" \
        -DWITH_DYNAMIC_LOAD=OFF \
        "${SRC_DIR}"
  make VERBOSE=1
  make test
  exit 0
elif [[ "$1" == "plugin" ]]; then
  cd "${BUILD_DIR}"
  "${SRC_DIR}"/ci//build_plugin.sh
  exit 0
elif [[ "$1" == "bazel.build" ]]; then
  bazel build //...
  exit 0
fi
