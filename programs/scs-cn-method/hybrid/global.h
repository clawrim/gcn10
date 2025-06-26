#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <gdal.h>
#include <omp.h>

// Raster structure
typedef struct {
    int nrows, ncols;
    double no_data_value;
    double geotransform[6]; // Store geotransform in Raster
    double *data;
} Raster;

// Raster I/O functions
Raster *allocate_raster(int nrows, int ncols, double no_data_value);
void free_raster(Raster *rast);
Raster *read_raster(const char *filename);
void write_raster(const char *filename, Raster *rast, const char *ref_file);
GDALDatasetH open_rainfall_dataset(const char *filename);

// MPI-specific raster functions
Raster *read_raster_mpi(const char *filename, int rank);
void write_raster_mpi(const char *filename, Raster *rast, const char *ref_file, int rank, int block_index);
void distribute_and_process_blocks(const char *cn_list_file, const char *rainfall_list_file, const char *output_dir, int num_threads);

// Runoff computation
void calculate_runoff(const Raster *rainfall, const Raster *curve_number, Raster *runoff);

#endif


