# GCN10: Global Curve Number 10m

<!-- [![status](https://joss.theoj.org/papers/821a5ce21f9dff42b866bad7400f2cc7/status.svg)](https://joss.theoj.org/papers/821a5ce21f9dff42b866bad7400f2cc7) -->

GCN10 is a high-resolution, open-source global Curve Number (CN) dataset generator.
It combines ESA WorldCover land cover data and HYSOGs250m soil data to create CN rasters
suitable for hydrologic modeling and runoff estimation.

<!--ts-->
* [GCN10: Global Curve Number 10m](#gcn10-global-curve-number-10m)
   * [1. Repository Structure](#1-repository-structure)
   * [2. Getting Started](#2-getting-started)
      * [2.1. Linux](#21-linux)
      * [2.2. Windows](#22-windows)
   * [3. Build and Compilation](#3-build-and-compilation)
      * [3.1. Dependencies](#31-dependencies)
      * [3.2. Linux](#32-linux)
      * [3.3. Windows](#33-windows)
         * [3.3.1. Install Dependencies](#331-install-dependencies)
            * [(a) Visual Studio 2022](#a-visual-studio-2022)
            * [(b) Microsoft MPI](#b-microsoft-mpi)
            * [(c) GDAL via OSGeo4W](#c-gdal-via-osgeo4w)
            * [(d) Install CMake](#d-install-cmake)
         * [3.3.2 Compilation](#332-compilation)
            * [Step 1: Open MSVC Environment](#step-1-open-msvc-environment)
            * [Step 2: Create Build Directory](#step-2-create-build-directory)
            * [Step 3: Configure with CMake](#step-3-configure-with-cmake)
            * [Step 4: Build the Executable](#step-4-build-the-executable)
            * [Step 5: Copy Executable to Source Directory](#step-5-copy-executable-to-source-directory)
      * [3.4 HPC](#34-hpc)
         * [3.4.1 Load Dependencies Modules](#341-load-dependencies-modules)
         * [3.4.2 Configure and Build](#342-configure-and-build)
   * [4. Running the Program](#4-running-the-program)
      * [4.1 Testing](#41-testing)
      * [4.2. Linux](#42-linux)
      * [4.3. Windows](#43-windows)
   * [5. Summary](#5-summary)
   * [6. Troubleshooting](#6-troubleshooting)
   * [7. Outputs](#7-outputs)
* [Acknowledgments](#acknowledgments)

<!-- Created by https://github.com/ekalinin/github-markdown-toc -->
<!-- Added by: abd, at: Thu Aug 21 16:14:23 MDT 2025 -->

<!--te-->

## 1. Repository Structure

- `src/`  : C MPI Program for parallelized CN generation.
- `landcover/`  : ESA WorldCover 2021 land cover virtual raster.
- `hsg/`        : HYSOGs250m hydrologic soil group raster.
- `blocks/`     : Block shapefile defining spatial extents for processing.
- `lookups/`    : Lookup tables mapping land cover and HSG to CN values.
- `archive/`
  - `python/` : Python scripts for serial and parallel CN raster generation.
  - `hybrid/` : Hybrid MPI/OpenMP C program for CN computation.
  - `gee/`    : Google Earth Engine script for dynamic CN raster generation.
  - `arcs/`   : Quarto reports and reproducible documentation for Antecedent Runoff Condition.

## 2. Getting Started

Clone the repository:

### 2.1. Linux
```bash
# ~/usr/local/src or wherever you want to compile
[ -d ~/usr/local/src ] || mkdir -p ~/usr/local/src
cd ~/usr/local/src
git clone https://github.com/clawrim/gcn10

# or if you have SSH enabled
git clone git@github.com:clawrim/gcn10.git

cd gcn10/
```

### 2.2. Windows
Make sure the following directories contain the required input files:
- `landcover/esa_worldcover_2021.vrt`
- `hsg/HYSOGs250m.tif`
- `blocks/esa_extent_blocks.shp`
- `lookups/default_lookup_*.csv`

## 3. Build and Compilation

### 3.1. Dependencies

- **MPI**: Required for parallel processing (MS-MPI on Windows).
- **GDAL/OGR**: Required for geospatial raster and vector operations.
- **CMake**: Required for building the project (version 3.12+).
- **C Compiler**: gcc (Linux) or MSVC (Visual Studio 2022 on windows).


### 3.2. Linux

```bash
(
  cd src
  [ -d build ] && rm -rf build
  mkdir build
  cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/usr/local && cmake --build . -v &&
  cmake --install .
)
```

Ensure development packages for GDAL, MPI (e.g., MPICH/OpenMPI), and OpenMP are installed.

### 3.3. Windows

#### 3.3.1. Install Dependencies
##### (a) Visual Studio 2022

- Download: https://visualstudio.microsoft.com/vs/
- Install the **"Desktop development with C++"** workload.

##### (b) Microsoft MPI

Install both the SDK and the Runtime:

- **MPI SDK**
  Download: https://www.microsoft.com/en-us/download/details.aspx?id=100593
  File: `msmpisdk.msi`

- **MPI Runtime**
  Download: https://www.microsoft.com/en-us/download/details.aspx?id=100593  
  File: `msmpisetup.exe`

After installation:

- MPI SDK path:
  `C:\Program Files (x86)\Microsoft SDKs\MPI`
- MPI Runtime path:
  `C:\Program Files\Microsoft MPI\Bin`

**Add to your system `PATH`:**

```
C:\Program Files\Microsoft MPI\Bin
```

##### (c) GDAL via OSGeo4W

- Download the OSGeo4W installer: https://download.osgeo.org/osgeo4w/osgeo4w-setup-x86_64.exe
- Choose **Advanced Install**.
- Select and install the following packages:
  - `gdal` (runtime)
  - `gdal-dev` (headers and `gdal_i.lib`)

After installation:

- GDAL include: `C:\OSGeo4W\include`
- GDAL lib: `C:\OSGeo4W\lib\gdal_i.lib`

**Add to your `PATH`:**

```
C:\OSGeo4W\bin
```

##### (d) Install CMake

Download from [CMake](https://cmake.org/download/) and install

#### 3.3.2 Compilation

##### Step 1: Open MSVC Environment

Either open **x64 Native Tools Command Prompt for VS 2022**, or run manually:

```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat"
```

##### Step 2: Create Build Directory

Replace USERNAME with your user.

```cmd
cd C:\USERNAME\gcn10\src
mkdir build
cd build
```

##### Step 3: Configure with CMake

```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 ^
 -DGDAL_INCLUDE_DIR="C:\OSGeo4W\include" ^
 -DGDAL_LIBRARY="C:\OSGeo4W\lib\gdal_i.lib" ^
 -DMPIEXEC_EXECUTABLE="C:\Program Files\Microsoft MPI\Bin\mpiexec.exe"
```

> Use ^ for line continuation in CMD.

##### Step 4: Build the Executable

```cmd
cmake --build . --config Release
```

Output will be located at:

```
build\Release\gcn10.exe
```

##### Step 5: Copy Executable to Source Directory

Your config.txt and input paths are relative to test\, so copy the executable:

```cmd
copy build\Release\gcn10.exe ..\
```

Now your executable is located at:

```
C:\USERNAME\gcn10\src\gcn10.exe
```

### 3.4 HPC

#### 3.4.1 Load Dependencies Modules

Before compiling, load the required toolchain modules.
Versions may vary by the cluster, check `module spider` or `module avail`
for the correct ones. 

On the New Mexico State [Discovery](https://doi.org/10.1145/3437359.3465610)
cluster, the setup is; 

```bash
module purge
module load spack/2023a
module load gcc/12.2.0-2023a-gcc_8.5.0-e643dqu
module load cmake/3.24.3-2023a-gcc_12.2.0-l6ogg2k
module load openmpi/4.1.4-2023a-gcc_12.2.0-slurm-pmix_v4-vv2mnh6
module load gdal/3.6.2-2023a-gcc_12.2.0-cuda-phafc4i
```
Check that the compilers and the libraries are visible

```bash
which mpicc mpicxx gdal-config cmake
mpicc -show
gdal-config --version
```
#### 3.4.2 Configure and Build

- Assuming the source code is cloned (e.g., `~/usr/local/src/gcn10` by following [Section 2.1](#21-linux))

```bash
(
  cd src
  [ -d build ] && rm -rf build
  mkdir build
  cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/usr/local && cmake --build . -v &&
  cmake --install .
)
```

The executable (`gcn10`) will be installed in `<CMAKE_INSTALL_PREFIX>/bin` (e.g., `~/usr/local/bin` following this Section).

## 4. Running the Program

### 4.1 Testing
For testing the installation of the program on any OS:

```bash
cd test/
chmod a+x run_test.py && ./run_test.py

# or

python3 run_test.py
# this will run the test with 8 processes by default.
```
If you want to change the number of processes;
```bash
./run_test.py n
# where n can be [1-8] e.g;
./run_test.py 4
```
This will create progress log in logs/ subdirectory and output rasters in
`cn_raster_drained` and `cn_raster_undrained` subdirectories in the
`gcn10/src/test/` directory.

> NOTE: Running more that 8 blocks will not have any additional advantages because
blocks.txt contains only 8 blocks

### 4.2. Linux
```bash
# use your CMAKE_INSTALL_PREFIX if it's different from $HOME/usr/local
# e.g., export PATH="<CMAKE_INSTALL_PREFIX>/bin:$PATH"
export PATH="$HOME/usr/local/bin:$PATH"

# serial

# start generating global CN rasters for all blocks
gcn10 -c config.txt
# serial with blocks list and overwrite
gcn10 -c config.txt -l blocks.txt -o

# parallel

# mpi (openmpi or mpich)
mpirun -n 4 gcn10 -c config.txt -o
# or (some setups)
mpiexec -n 4 gcn10 -c config.txt -o
```

### 4.3. Windows

From the `src\test\` directory:
```cmd
:: serial
gcn10.exe -c config.txt

:: serial with block list + overwrite
gcn10.exe -c config.txt -l blocks.txt -o

:: mpi (ms-mpi)
mpiexec -n 4 gcn10.exe -c config.txt -o
```

## 5. Summary

| Task                 | Command / Action                                             |
|----------------------|--------------------------------------------------------------|
| Install MSVC         | Visual Studio 2022 + Desktop C++ workload                    |  
| Install MPI          | Install SDK + Runtime, add Runtime to `PATH`                 |
| Install GDAL         | Use OSGeo4W: install `gdal`, `gdal-dev`, add `bin` to `PATH` |
| Configure with CMake | `cmake .. -G ... -DGDAL_INCLUDE_DIR=... -DGDAL_LIBRARY=...`  |
| Build                | `cmake --build . --config Release`                           |
| Copy executable      | `copy build\Release\gcn10.exe ..\`                           |
| Run                  | `gcn10.exe -c config.txt [-l blocks.txt] [-o]`            |

## 6. Troubleshooting

- If gcn10 crashes, check:
  - Paths in config.txt are valid
  - Input rasters and shapefiles exist
  - You're in the correct working directory

- If mpiexec fails:
  - Ensure MPI Runtime is installed
  - Ensure mpiexec.exe is in your system PATH

- Read the log files in written rankwise in the logs/ subdir for other errors. 

## 7. Outputs

- 18 Cloud Optimized GeoTIFFs per block (9 drained, 9 undrained)
- Output directories:
  - `cn_rasters_drained/`
  - `cn_rasters_undrained/`
- File naming: `cn_{hc}_{arc}_{block_id}.tif`

# Acknowledgments

We acknowledge the [New Mexico Water Resources Research Institute](https://nmwrri.nmsu.edu/) (NM WRRI)
and the New Mexico State Legislature for their support and resources, which
were essential for this project. This work is funded by the NM WRRI and the
New Mexico State Legislature under the grant number NMWRRI-SG-FALL2024.
