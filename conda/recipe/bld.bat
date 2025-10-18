@echo off
setlocal

rem prefer conda host prefix tools/dlls
set "PATH=%LIBRARY_BIN%;%PATH%"

cmake -S "%SRC_DIR%\src" -B build -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
  -DCMAKE_PREFIX_PATH="%LIBRARY_PREFIX%" ^
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON ^
  -DCMAKE_C_COMPILER="%CC%" ^
  -DCMAKE_CXX_COMPILER="%CXX%" ^
  %CMAKE_ARGS% || exit /b 1

cmake --build build --parallel %CPU_COUNT% || exit /b 1
cmake --install build || exit /b 1

endlocal
