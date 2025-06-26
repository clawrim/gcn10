#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <gdal.h>
#include <omp.h>

// Raster structure
typedef struct {
    int nrows, ncols;
    double no_data_value;
    double *data;
} Raster;

// Raster I/O functions
Raster *allocate_raster(int nrows, int ncols, double no_data_value);
void free_raster(Raster *rast);
Raster *read_raster(const char *filename);
void write_raster(const char *filename, Raster *rast, const char *ref_file);
GDALDatasetH open_rainfall_dataset(const char *filename);
double get_rainfall_value(GDALDatasetH rainfall_ds, int col, int row, double *cn_geotransform);

// Runoff computation
void calculate_runoff(GDALDatasetH rainfall_ds, const Raster *curve_number, Raster *runoff, double *cn_geotransform);

#endif


