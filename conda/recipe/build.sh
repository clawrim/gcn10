#!/usr/bin/env sh
set -eu

: "${CMAKE_GENERATOR:=Ninja}"

cmake -S "${SRC_DIR}/src" -B build -G "${CMAKE_GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_PREFIX_PATH="${PREFIX}" \
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
  ${CMAKE_ARGS}

cmake --build build --parallel "${CPU_COUNT}"
cmake --install build

test -x "${PREFIX}/bin/gcn10"
