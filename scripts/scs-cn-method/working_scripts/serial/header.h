#ifndef HEADER_H
#define HEADER_H

#include <gdal.h>

typedef void (*cell_callback)(int row, int col, double value, void *user_data);

void get_raster_metadata(const char *filename, int *rows, int *cols, double *no_data_value);
void read_raster_cell_by_cell(const char *filename, cell_callback callback, void *user_data);
void write_raster_cell_by_cell(const char *filename, int rows, int cols, double no_data_value,
                               const char *ref_filename, cell_callback callback, void *user_data);

#endif

