#!/usr/bin/env sh
set -eu

: "${CMAKE_GENERATOR:=Ninja}"

# build from repo root
cmake -S "${SRC_DIR}/src" -B build -G "${CMAKE_GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_PREFIX_PATH="${PREFIX}" \
  -DGCN10_PORTABLE=ON \
  ${CMAKE_ARGS}

cmake --build build --parallel "${CPU_COUNT}"
cmake --install build
