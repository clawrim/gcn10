cmake_minimum_required(VERSION 3.12)
project(GCN10)

# set C standard to C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# find MPI (prefer MS-MPI on Windows)
find_package(MPI REQUIRED)
if(MPI_FOUND)
    include_directories(${MPI_C_INCLUDE_DIRS})
endif()

# find GDAL
find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    include_directories(${GDAL_INCLUDE_DIRS})
endif()

# define source files
set(SOURCES
    main.c
    config.c
    raster.c
    cn.c
    log.c
)

# define executable
add_executable(gcn10 ${SOURCES})

# link libraries (add math library for non-MSVC compilers)
target_link_libraries(gcn10 PRIVATE ${MPI_C_LIBRARIES} ${GDAL_LIBRARIES})
if(NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_link_libraries(gcn10 PRIVATE m)
endif()

# set compiler flags for MSVC and others
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_compile_options(gcn10 PRIVATE /W4 /O2 /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(gcn10 PRIVATE -Wall -Werror -O3 -march=native)
endif()

# set output executable extension for Windows
if(WIN32)
    set_target_properties(gcn10 PROPERTIES SUFFIX ".exe")
endif()

# install target
install(TARGETS gcn10 DESTINATION bin)

# enable testing
enable_testing()
# add tests here (e.g., ctest commands for sample inputs)
