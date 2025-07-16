#ifndef HEADER_H
#define HEADER_H

#include <gdal.h>

typedef struct {
    double *data;
    int nrows, ncols;
    double no_data_value;
} Raster;

Raster *allocate_raster(int nrows, int ncols, double no_data_value);
void free_raster(Raster *rast);
void get_raster_metadata(const char *filename, int *rows, int *cols, double *no_data_value);
int process_runoff_timestep(const char *rainfall_file, const char *curve_number_vrt, const char *output_file, int tile_size);
Raster *read_rainfall_tile(const char *filename, int tile_row, int tile_col, int tile_size);
Raster *read_curve_number_tile(const char *vrt, int tile_row, int tile_col, int tile_size);
void calculate_runoff(const Raster *rainfall, const Raster *curve_number, Raster *runoff);
void write_runoff_tile(const char *filename, const Raster *runoff, int tile_row, int tile_col, int tile_size);
Raster *read_curve_number_tile(const char *vrt, int tile_row, int tile_col, int tile_size);

#define NUM_TIMESTEPS 87600 // Example: 3-hourly for 10 years

#endif

