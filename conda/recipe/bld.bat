set "PATH=%LIBRARY_BIN%;%PATH%"

cmake -S "%SRC_DIR%\src" -B build -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
  -DCMAKE_PREFIX_PATH="%LIBRARY_PREFIX%;%PREFIX%" ^
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON ^
  -DCMAKE_C_COMPILER="%CC%" ^
  %CMAKE_ARGS%
if errorlevel 1 exit /b 1

cmake --build build --parallel %CPU_COUNT%
if errorlevel 1 exit /b 1

cmake --install build
if errorlevel 1 exit /b 1

if not exist "%LIBRARY_BIN%\gcn10.exe" exit /b 1

endlocal
