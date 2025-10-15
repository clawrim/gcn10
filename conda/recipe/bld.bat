@echo on
setlocal EnableDelayedExpansion

REM conda-forge toolchain exposes cl.exe, Ninja, msmpi, and gdal
cmake -S %SRC_DIR% -B build -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX%" ^
  -DGCN10_PORTABLE=ON ^
  %CMAKE_ARGS%

cmake --build build --config Release --parallel
cmake --install build

