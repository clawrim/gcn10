cmake_minimum_required(VERSION 3.12)

project(gcn10 LANGUAGES C)

# language/flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# prefer config packages (conda-forge, vcpkg, etc.)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# MPI
find_package(MPI REQUIRED COMPONENTS C)
# Use the imported target if provided; fallback to a compatible INTERFACE target
if(NOT TARGET MPI::MPI_C)
  add_library(MPI::MPI_C INTERFACE IMPORTED)
  if(MPI_C_LIBRARIES)
    target_link_libraries(MPI::MPI_C INTERFACE ${MPI_C_LIBRARIES})
  endif()
  if(MPI_C_INCLUDE_DIRS)
    target_include_directories(MPI::MPI_C INTERFACE ${MPI_C_INCLUDE_DIRS})
  elseif(MPI_C_INCLUDE_PATH)
    target_include_directories(MPI::MPI_C INTERFACE ${MPI_C_INCLUDE_PATH})
  endif()
endif()

# GDAL
# try CONFIG first (conda/vcpkg), then module fallback; always expose GDAL::GDAL
find_package(GDAL CONFIG QUIET)
if(NOT GDAL_FOUND)
  find_package(GDAL REQUIRED) # module mode
  if(NOT TARGET GDAL::GDAL)
    add_library(GDAL::GDAL UNKNOWN IMPORTED)
    if(GDAL_LIBRARY)
      set_target_properties(GDAL::GDAL PROPERTIES IMPORTED_LOCATION "${GDAL_LIBRARY}")
    endif()
    if(GDAL_INCLUDE_DIRS)
      target_include_directories(GDAL::GDAL INTERFACE "${GDAL_INCLUDE_DIRS}")
    elseif(GDAL_INCLUDE_DIR)
      target_include_directories(GDAL::GDAL INTERFACE "${GDAL_INCLUDE_DIR}")
    endif()
  endif()
endif()

# sources
set(SOURCES
  main.c
  config.c
  raster.c
  cn.c
  log.c
)

add_executable(gcn10 ${SOURCES})

# link order matters on windows; targets handle it for us
target_link_libraries(gcn10 PRIVATE MPI::MPI_C GDAL::GDAL)

# warnings/opts; math on non-MSVC
if(MSVC)
  target_compile_definitions(gcn10 PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  target_compile_options(gcn10 PRIVATE /W4 /O2)
else()
  target_compile_options(gcn10 PRIVATE -Wall -O3)
  target_link_libraries(gcn10 PRIVATE m)
endif()

# install
install(TARGETS gcn10 DESTINATION bin)

# testing
include(CTest) # enables 'enable_testing()' and CTest targets
