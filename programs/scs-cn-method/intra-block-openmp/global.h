#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <gdal.h>
#include <omp.h>
#include <pthread.h>

// raster structure
typedef struct {
    int nrows, ncols;
    double no_data_value;
    double *data;
} Raster;

// raster i/o functions (raster.c)
Raster *allocate_raster(int nrows, int ncols, double no_data_value);
void free_raster(Raster *rast);
Raster *read_raster(const char *filename);
Raster *read_raster_parallel(const char *filename, int num_threads);
void write_raster(const char *filename, float *data, int nrows, int ncols, double no_data_value, const char *ref_file);

// runoff computation (runoff.c)
void calculate_runoff(Raster *rainfall, const Raster *curve_number, Raster *runoff);

#endif
