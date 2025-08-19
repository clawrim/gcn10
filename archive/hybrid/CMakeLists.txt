cmake_minimum_required(VERSION 3.12)
project(gencn_hybrid C)

# set C standard to C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# find MPI (e.g., OpenMPI or MS-MPI)
find_package(MPI REQUIRED)
if(MPI_FOUND)
    include_directories(${MPI_C_INCLUDE_DIRS})
endif()

# find OpenMP
find_package(OpenMP REQUIRED)

# find GDAL
find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIRS})
endif()

# define sources
set(SOURCES
    main.c
    config.c
    raster.c
    cn.c
    log.c
)

# add executable
add_executable(gencn_hybrid ${SOURCES})

# link libraries
target_link_libraries(gencn_hybrid PRIVATE
    ${MPI_C_LIBRARIES}
    ${GDAL_LIBRARIES}
    OpenMP::OpenMP_C
)

# add math library for non-MSVC
if(NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_link_libraries(gencn_hybrid PRIVATE m)
endif()

# set compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_compile_options(gencn_hybrid PRIVATE /W4 /O2 /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(gencn_hybrid PRIVATE -Wall -Werror -O3 -march=native)
endif()

# set .exe suffix on Windows
if(WIN32)
    set_target_properties(gencn_hybrid PROPERTIES SUFFIX ".exe")
endif()

# install target
install(TARGETS gencn_hybrid DESTINATION bin)

# Enable testing
# enable_testing()
