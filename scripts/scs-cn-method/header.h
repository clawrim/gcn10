
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
Raster *read_raster(const char *filename);
void calculate_runoff(const Raster *rainfall, const Raster *curve_number, Raster *runoff);
void write_raster(const char *filename, const Raster *rast, const char *ref_filename);

#endif
