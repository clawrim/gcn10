# SCS Runoff Calculator

## Description
The **SCS Runoff Calculator** computes runoff depth using the Soil Conservation Service (SCS) Curve Number (CN) method. This tool processes geospatial raster data to calculate runoff depth for a given rainfall raster (`rainfall.tif`) and curve number raster (`curve_number.tif`). The output is saved as a raster file (`output_runoff.tif`) representing the runoff depth.

### How It Works
1. **Input Rasters**:
   - `rainfall.tif`: A GeoTIFF file containing rainfall depth values (in millimeters).
   - `curve_number.tif`: A GeoTIFF file containing Curve Number values (dimensionless).

2. **SCS Equation**:
   The SCS runoff equation is applied pixel-by-pixel:
   \[
   Q = \begin{cases} 
   0, & \text{if } P \leq 0.2S \\
   \frac{(P - 0.2S)^2}{P + 0.8S}, & \text{if } P > 0.2S
   \end{cases}
   \]
   Where:
   - \( Q \): Runoff depth (mm)
   - \( P \): Rainfall depth (mm)
   - \( S = \frac{25400}{CN} - 254 \): Maximum potential retention (mm)

3. **Output Raster**:
   - `output_runoff.tif`: A GeoTIFF file containing runoff depth values calculated for each pixel.

## Compilation

This project is written in **C** and relies on the **GDAL library** for raster data handling. Ensure GDAL is installed on your system before compiling.

### Compile the Program
Use the included `Makefile` to compile the program:
```bash
make
```

To remove the compiled files:
```bash
make clean
```

### Usage
```bash
./scs_runoff <rainfall.tif> <curve_number.tif> <output_runoff.tif>
```
Example using the files provided in the test directory
```bash
./scs_runoff test/rainfall.tif test/cn_p_iii_2280.tif test/runoff.tif
```
