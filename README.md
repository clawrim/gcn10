# GCN10: Global Curve Number 10m

GCN10 is a high-resolution, open-source global Curve Number (CN) dataset generator.
It combines ESA WorldCover land cover data and HYSOGs250m soil data to create CN rasters
suitable for hydrologic modeling and runoff estimation.

---

## 1. Repository Structure

- `src/`
  - `python/` : Python scripts for serial and parallel CN raster generation.
  - `c/`      : C programs for high-performance CN computation (OpenMP, MPI, hybrid).
  - `gee/`    : Google Earth Engine script for dynamic CN raster generation.
  - `arcs/`   : Quarto reports and reproducible documentation.
- `landcover/` : ESA WorldCover 2021 land cover virtual raster.
- `hsg/`       : HYSOGs250m hydrologic soil group raster.
- `shps/`      : Block shapefile defining spatial extents for processing.
- `lookups/`   : Lookup tables mapping land cover and HSG to CN values.

---

## 2. Getting Started

Clone the repository:

### 2.1. Linux
```bash
cd usr/local/src # or wherever you want to compile
git clone https://github.com/mabdazzam/gcn10

# or if you have SSH enabled
git clone git@github.com:mabdazzam/gcn10.git

cd gcn10
```

### 2.2. Windows
Make sure the following directories contain the required input files:
- `landcover/esa_worldcover_2021.vrt`
- `hsg/HYSOGs250m.tif`
- `shps/esa_extent_blocks.shp`
- `lookups/default_lookup_*.csv`

---

## 3. Build & Compilation

### 3.1. Dependencies

- **MPI**: Required for parallel processing (MS-MPI on Windows).
- **GDAL/OGR**: Required for geospatial raster and vector operations.
- **CMake**: Required for building the project (version 3.12+).
- **C Compiler**: gcc (linux) or MSVC (Visual Studio 2022 on windows).


### 3.2. Linux

```bash
cd mpi/
mkdir build && cd build
cmake ..; make
cd ..
cp build/gcn10 .
```

Ensure development packages for GDAL, MPI (e.g., MPICH/OpenMPI), and OpenMP are installed.

### 3.3. Windows

#### 3.3.1. Install Dependencies
#### (a) Visual Studio 2022

- Download: https://visualstudio.microsoft.com/vs/
- Install the **"Desktop development with C++"** workload.
- Ensure `cmake`, `cl.exe`, and `ninja` are included in the environment.

#### (b) Microsoft MPI

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

#### (c) GDAL via OSGeo4W

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

#### (d) **Install CMake**
 Download from [CMake](https://cmake.org/download/) and install

#### 3.3.2 Compilation

#### Step 1: Open MSVC Environment

Either open **x64 Native Tools Command Prompt for VS 2022**, or run manually:

```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat"
```

#### Step 2: Create Build Directory

Replace USERNAME with your user.

```cmd
cd C:\USERNAME\gcn10\c
mkdir build
cd build
```

#### Step 3: Configure with CMake

```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 ^
 -DGDAL_INCLUDE_DIR="C:\OSGeo4W\include" ^
 -DGDAL_LIBRARY="C:\OSGeo4W\lib\gdal_i.lib" ^
 -DMPIEXEC_EXECUTABLE="C:\Program Files\Microsoft MPI\Bin\mpiexec.exe"
```

> Use ^ for line continuation in CMD.

#### Step 4: Build the Executable

```cmd
cmake --build . --config Release
```

Output will be located at:

```
build\Release\gcn10.exe
```

#### Step 5: Copy Executable to Source Directory

Your config.txt and input paths are relative to c\, so copy the executable:

```cmd
copy build\Release\gcn10.exe ..\
```

Now your executable is located at:

```
C:\USERNAME\gcn10\c\gcn10.exe
```

---

## 4. Running the Program

### 4.1. Linux
```bash
# if gdal/mpi aren’t in default paths
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
export PATH=/usr/local/bin:$PATH

# serial
./gcn10 -c config.txt

# serial with block list + overwrite
./gcn10 -c config.txt -l block_ids.txt -o

# mpi (openmpi or mpich)
mpirun -n 4 ./gcn10 -c config.txt -o
# or (some setups)
mpiexec -n 4 ./gcn10 -c config.txt -o
```

### 4.2. Windows

From the mpi\ directory:
```cmd
:: serial
gcn10.exe -c config.txt

:: serial with block list + overwrite
gcn10.exe -c config.txt -l block_ids.txt -o

:: mpi (ms-mpi)
mpiexec -n 4 gcn10.exe -c config.txt -o
```

---

## 5. Summary

| Task                 | Command / Action                                             |
|----------------------|--------------------------------------------------------------|
| Install MSVC         | Visual Studio 2022 + Desktop C++ workload                    |  
| Install MPI          | Install SDK + Runtime, add Runtime to `PATH`                 |
| Install GDAL         | Use OSGeo4W: install `gdal`, `gdal-dev`, add `bin` to `PATH` |
| Configure with CMake | `cmake .. -G ... -DGDAL_INCLUDE_DIR=... -DGDAL_LIBRARY=...`  |
| Build                | `cmake --build . --config Release`                           |
| Copy executable      | `copy build\Release\gcn10.exe ..\`                           |
| Run                  | `gcn10.exe -c config.txt [-l block_ids.txt] [-o]`            |

---

## 6. Troubleshooting

- If gcn10 crashes, check:
  - Paths in config.txt are valid
  - Input rasters and shapefiles exist
  - You're in the correct working directory

- If mpiexec fails:
  - Ensure MPI Runtime is installed
  - Ensure mpiexec.exe is in your system PATH

- Read the log files in written rankwise in the logs/ subdir for other errors. 

---

## 7. Outputs

- 18 Cloud Optimized GeoTIFFs per block (9 drained, 9 undrained)
- Output directories:
  - `cn_rasters_drained/`
  - `cn_rasters_undrained/`
- File naming: `cn_{hc}_{arc}_{block_id}.tif`
