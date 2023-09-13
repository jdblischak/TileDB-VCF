#!/bin/sh
set -exo pipefail

export CMAKE_OSX_DEPLOYMENT_TARGET=10.14

cmake -S TileDB -B build-libtiledb \
  -D CMAKE_INSTALL_PREFIX="${CONDA_PREFIX}" \
  -D CMAKE_BUILD_TYPE=Release \
  -D TILEDB_WERROR=OFF \
  -D TILEDB_TESTS=OFF \
  -D TILEDB_INSTALL_LIBDIR=lib \
  -D TILEDB_HDFS=ON \
  -D SANITIZER=OFF \
  -D COMPILER_SUPPORTS_AVX2:BOOL=FALSE \
  -D TILEDB_AZURE=ON \
  -D TILEDB_GCS=ON \
  -D TILEDB_S3=ON \
  -D TILEDB_SERIALIZATION=ON \
  -D TILEDB_LOG_OUTPUT_ON_FAILURE=ON \
  -D CMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET}

cmake --build build-libtiledb -j2 --config Release

cmake --build build-libtiledb --config Release --target install-tiledb
