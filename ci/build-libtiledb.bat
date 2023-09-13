@echo on

cmake -S TileDB -B build-libtiledb ^
  -D CMAKE_INSTALL_PREFIX="%CONDA_PREFIX%\Library" ^
  -D CMAKE_BUILD_TYPE=Release ^
  -D TILEDB_WERROR=OFF ^
  -D TILEDB_AZURE=ON ^
  -D TILEDB_GCS=ON ^
  -D TILEDB_S3=ON ^
  -D TILEDB_HDFS=OFF ^
  -D COMPILER_SUPPORTS_AVX2=OFF ^
  -D TILEDB_SERIALIZATION=ON ^
  -D libxml2_DIR="%CONDA_PREFIX%\Library" ^
  -D CMAKE_PREFIX_PATH="%CONDA_PREFIX%\Library"
if errorlevel 1 exit 1

cmake --build build-libtiledb -j2 --config Release
if errorlevel 1 exit 1

cmake --build build-libtiledb --config Release --target install-tiledb
if errorlevel 1 exit 1
