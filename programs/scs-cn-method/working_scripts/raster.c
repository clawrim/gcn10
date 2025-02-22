#include "header.h"
#include <omp.h>
#include <stdlib.h>
#include <stdio.h>

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "Error: Unable to allocate memory for raster structure\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = malloc(nrows * ncols * sizeof(double));
    if (!rast->data) {
        fprintf(stderr, "Error: Unable to allocate memory for raster data\n");
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

Raster *read_rainfall_tile(const char *filename, int tile_row, int tile_col, int tile_size) {
    int rows, cols;
    double no_data_value;
    get_raster_metadata(filename, &rows, &cols, &no_data_value);

    int rows_to_read = (tile_row + tile_size > rows) ? rows - tile_row : tile_size;
    int cols_to_read = (tile_col + tile_size > cols) ? cols - tile_col : tile_size;

    Raster *tile = allocate_raster(rows_to_read, cols_to_read, no_data_value);
    if (!tile) return NULL;

    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
        free_raster(tile);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Read, tile_col, tile_row, cols_to_read, rows_to_read, tile->data, cols_to_read, rows_to_read, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Error: Unable to read tile from %s\n", filename);
        free_raster(tile);
    }

    GDALClose(dataset);
    return tile;
}

void get_raster_metadata(const char *filename, int *rows, int *cols, double *no_data_value) {
    GDALAllRegister();

    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    *rows = GDALGetRasterYSize(dataset);
    *cols = GDALGetRasterXSize(dataset);
    *no_data_value = GDALGetRasterNoDataValue(band, NULL);

    GDALClose(dataset);
}

Raster *read_curve_number_tile(const char *vrt, int tile_row, int tile_col, int tile_size) {
    int rows, cols;
    double no_data_value;

    get_raster_metadata(vrt, &rows, &cols, &no_data_value);

    int rows_to_read = (tile_row + tile_size > rows) ? rows - tile_row : tile_size;
    int cols_to_read = (tile_col + tile_size > cols) ? cols - tile_col : tile_size;

    Raster *tile = allocate_raster(rows_to_read, cols_to_read, no_data_value);
    if (!tile) return NULL;

    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(vrt, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open curve number VRT %s\n", vrt);
        free_raster(tile);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Read, tile_col, tile_row, cols_to_read, rows_to_read, tile->data, cols_to_read, rows_to_read, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Error: Unable to read tile from %s\n", vrt);
        free_raster(tile);
    }

    GDALClose(dataset);
    return tile;
}

void calculate_runoff(const Raster *rainfall, const Raster *curve_number, Raster *runoff) {
#pragma omp parallel for
    for (int row = 0; row < runoff->nrows; row++) {
        for (int col = 0; col < runoff->ncols; col++) {
            int idx = row * runoff->ncols + col;
            double R = rainfall->data[idx];
            double CN = curve_number->data[idx];
	    printf("%f %f\n", R, CN);

            if (R < 0 || CN < 0) {
                runoff->data[idx] = runoff->no_data_value;
            } else {
                double S = (25400.0 / CN) - 254.0;
                runoff->data[idx] = (R > 0.2 * S) ? ((R - 0.2 * S) * (R - 0.2 * S)) / (R + 0.8 * S) : 0.0;
            }
        }
    }
}

int process_runoff_timestep(const char *rainfall_file, const char *curve_number_vrt, const char *output_file, int tile_size) {
    GDALAllRegister();

    GDALDatasetH rainfall_ds = GDALOpen(rainfall_file, GA_ReadOnly);
    if (!rainfall_ds) {
        fprintf(stderr, "Error: Unable to open rainfall raster %s\n", rainfall_file);
        return -1;
    }

    int nrows = GDALGetRasterYSize(rainfall_ds);
    int ncols = GDALGetRasterXSize(rainfall_ds);

    GDALDriverH driver = GDALGetDriverByName("GTiff");
    GDALDatasetH runoff_ds = GDALCreate(driver, output_file, ncols, nrows, 1, GDT_Float64, NULL);

    double geotransform[6];
    GDALGetGeoTransform(rainfall_ds, geotransform);
    GDALSetGeoTransform(runoff_ds, geotransform);
    GDALSetProjection(runoff_ds, GDALGetProjectionRef(rainfall_ds));

    GDALRasterBandH runoff_band = GDALGetRasterBand(runoff_ds, 1);

    for (int row = 0; row < nrows; row += tile_size) {
        for (int col = 0; col < ncols; col += tile_size) {
            Raster *rainfall_tile = read_rainfall_tile(rainfall_file, row, col, tile_size);
            Raster *curve_number_tile = read_curve_number_tile(curve_number_vrt, row, col, tile_size);

            Raster *runoff_tile = allocate_raster(rainfall_tile->nrows, rainfall_tile->ncols, rainfall_tile->no_data_value);
            calculate_runoff(rainfall_tile, curve_number_tile, runoff_tile);

            if (GDALRasterIO(runoff_band, GF_Write, col, row, runoff_tile->ncols, runoff_tile->nrows, runoff_tile->data, runoff_tile->ncols, runoff_tile->nrows, GDT_Float64, 0, 0) != CE_None) {
                fprintf(stderr, "Error: Unable to write tile to output raster\n");
            }

            free_raster(rainfall_tile);
            free_raster(curve_number_tile);
            free_raster(runoff_tile);
        }
    }

    GDALClose(rainfall_ds);
    GDALClose(runoff_ds);
    return 0;
}
