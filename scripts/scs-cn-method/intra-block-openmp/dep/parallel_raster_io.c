
#include "header.h"
#include <omp.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    if (nrows <= 0 || ncols <= 0) {
        fprintf(stderr, "Invalid dimensions: %d x %d\n", nrows, ncols);
        return NULL;
    }
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "Alloc error\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = malloc(nrows * ncols * sizeof(double));
    if (!rast->data) {
        fprintf(stderr, "Data alloc error\n");
        free(rast);
        return NULL;
    }
    return rast;
}

void free_raster(Raster *rast) {
    if (rast) {
        free(rast->data);
        free(rast);
    }
}

bool file_exists(const char *filename) {
    return stat(filename, &(struct stat){}) == 0;
}

Raster *read_rainfall_tile(const char *filename, int tile_row, int tile_col, int tile_size, int num_threads) {
    int rows, cols;
    double no_data;
    get_raster_metadata(filename, &rows, &cols, &no_data);

    int read_rows = (tile_row + tile_size > rows) ? rows - tile_row : tile_size;
    int read_cols = (tile_col + tile_size > cols) ? cols - tile_col : tile_size;

    Raster *tile = allocate_raster(read_rows, read_cols, no_data);
    if (!tile) return NULL;

    GDALAllRegister();
    GDALDatasetH ds = GDALOpen(filename, GA_ReadOnly);
    if (!ds) {
        fprintf(stderr, "Open error: %s\n", filename);
        free_raster(tile);
        return NULL;
    }

    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    if (!band) {
        fprintf(stderr, "Band error: %s\n", filename);
        GDALClose(ds);
        free_raster(tile);
        return NULL;
    }

    #pragma omp parallel for num_threads(num_threads) collapse(2)
    for (int i = 0; i < read_rows; i++) {
        for (int j = 0; j < read_cols; j++) {
            if (GDALRasterIO(band, GF_Read, tile_col + j, tile_row + i, 1, 1, 
                             &tile->data[i * read_cols + j], 1, 1, GDT_Float64, 0, 0) != CE_None) {
                fprintf(stderr, "Read error: [%d, %d]\n", tile_row + i, tile_col + j);
            }
        }
    }

    GDALClose(ds);
    return tile;
}

void write_raster_tile(const char *filename, Raster *tile, int tile_row, int tile_col, int tile_size, int num_threads) {
    if (file_exists(filename)) {
        fprintf(stderr, "File exists: %s\n", filename);
        return;
    }

    if (tile_row < 0 || tile_col < 0 || tile_row >= tile->nrows || tile_col >= tile->ncols) {
        fprintf(stderr, "Out of bounds: [%d, %d]\n", tile_row, tile_col);
        return;
    }

    GDALAllRegister();
    GDALDriverH driver = GDALGetDriverByName("GTiff");
    if (!driver) {
        fprintf(stderr, "Driver error\n");
        return;
    }

    GDALDatasetH ds = GDALCreate(driver, filename, tile->ncols, tile->nrows, 1, GDT_Float64, NULL);
    if (!ds) {
        fprintf(stderr, "Create error: %s\n", filename);
        return;
    }

    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    if (!band) {
        fprintf(stderr, "Band error: %s\n", filename);
        GDALClose(ds);
        return;
    }

    #pragma omp parallel for num_threads(num_threads) collapse(2)
    for (int i = 0; i < tile->nrows; i++) {
        for (int j = 0; j < tile->ncols; j++) {
            if (GDALRasterIO(band, GF_Write, tile_col + j, tile_row + i, 1, 1, 
                             &tile->data[i * tile->ncols + j], 1, 1, GDT_Float64, 0, 0) != CE_None) {
                fprintf(stderr, "Write error: [%d, %d]\n", tile_row + i, tile_col + j);
            }
        }
    }

    GDALClose(ds);
}

void get_raster_metadata(const char *filename, int *rows, int *cols, double *no_data) {
    GDALAllRegister();
    GDALDatasetH ds = GDALOpen(filename, GA_ReadOnly);
    if (!ds) {
        fprintf(stderr, "Metadata error: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    *rows = GDALGetRasterYSize(ds);
    *cols = GDALGetRasterXSize(ds);
    *no_data = GDALGetRasterNoDataValue(GDALGetRasterBand(ds, 1), NULL);
    GDALClose(ds);
}
