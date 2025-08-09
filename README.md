# GCN10: Global Curve Number Dataset

GCN10 is a high-resolution, open-source global Curve Number (CN) dataset generator.
It combines ESA WorldCover land cover data and HYSOGs250m soil data to create CN rasters
suitable for hydrologic modeling and runoff estimation.

---

## Repository Structure

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

## Getting Started

Clone the repository:

```bash
git clone https://github.com/mabdazzam/GCN10
cd GCN10
```

Make sure the following directories contain the required input files:
- `landcover/esa_worldcover_2021.vrt`
- `hsg/HYSOGs250m.tif`
- `shps/esa_extent_blocks.shp`
- `lookups/default_lookup_*.csv`

---

## Build & Compilation

### Dependencies

- **MPI**: Required for parallel processing (MS-MPI on Windows).
- **GDAL/OGR**: Required for geospatial raster and vector operations.
- **CMake**: Required for building the project (version 3.12+).
- **C Compiler**: MSVC (Visual Studio 2022).

---

### Linux

```bash
cd src/c/hybrid
make

cd ../mpi
make
```

Ensure development packages for GDAL, MPI (e.g., MPICH/OpenMPI), and OpenMP are installed.

## Windows Installation (MSVC)

### 1. Visual Studio 2022

- Download: https://visualstudio.microsoft.com/vs/
- Install the **"Desktop development with C++"** workload.
- Ensure `cmake`, `cl.exe`, and `ninja` are included in the environment.

---

### 2. Microsoft MPI

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

---

### 3. GDAL via OSGeo4W

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

---

### 4. **Install CMake**
    - Download from [https://cmake.org/download/](https://cmake.org/download/)
    - Or via Chocolatey:
      ```bat
      choco install cmake
```
---

## Compilation Instructions

### Step 1: Open MSVC Environment

Either open **x64 Native Tools Command Prompt for VS 2022**, or run manually:

```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat"
```

---

### Step 2: Create Build Directory

Replace USERNAME with your user.

```cmd
cd C:\USERNAME\gcn10\c
mkdir build
cd build
```

---

### Step 3: Configure with CMake

```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 ^
 -DGDAL_INCLUDE_DIR="C:\OSGeo4W\include" ^
 -DGDAL_LIBRARY="C:\OSGeo4W\lib\gdal_i.lib" ^
 -DMPIEXEC_EXECUTABLE="C:\Program Files\Microsoft MPI\Bin\mpiexec.exe"
```

> Use ^ for line continuation in CMD.

---

### Step 4: Build the Executable

```cmd
cmake --build . --config Release
```

Output will be located at:

```
build\Release\gencn.exe
```

---

## Post-Build Setup

### Step 5: Copy Executable to Source Directory

Your config.txt and input paths are relative to c\, so copy the executable:

```cmd
copy build\Release\gencn.exe ..\
```

Now your executable is located at:

```
C:\USERNAME\gcn10\c\gencn.exe
```

---

## Running the Program

From the c\ directory:

```cmd
gencn.exe -c config.txt
```

### Optional Flags:

- -l block_ids.txt : Specify block IDs to process  
- -o               : Overwrite output files if they exist

### Example:

```cmd
gencn.exe -c config.txt -l block_ids.txt -o
```

### Run in Parallel with MPI:

```cmd
mpiexec -n 4 gencn.exe -c config.txt -o
```

---

## Summary

| Task                 | Command / Action                                             |
|----------------------|--------------------------------------------------------------|
| Install MSVC         | Visual Studio 2022 + Desktop C++ workload                    |  
| Install MPI          | Install SDK + Runtime, add Runtime to `PATH`                 |
| Install GDAL         | Use OSGeo4W: install `gdal`, `gdal-dev`, add `bin` to `PATH` |
| Configure with CMake | `cmake .. -G ... -DGDAL_INCLUDE_DIR=... -DGDAL_LIBRARY=...`  |
| Build                | `cmake --build . --config Release`                           |
| Copy executable      | `copy build\Release\gencn.exe ..\`                           |
| Run                  | `gencn.exe -c config.txt [-l block_ids.txt] [-o]`            |

---

## Troubleshooting

- If gencn.exe crashes, check:
  - Paths in config.txt are valid
  - Input rasters and shapefiles exist
  - You're in the correct working directory

- If mpiexec fails:
  - Ensure MPI Runtime is installed
  - Ensure mpiexec.exe is in your system PATH

---

## Outputs

- 18 Cloud Optimized GeoTIFFs per block (9 drained, 9 undrained)
- Output directories:
  - `cn_rasters_drained/`
  - `cn_rasters_undrained/`
- File naming: `cn_{hc}_{arc}_{block_id}.tif`
