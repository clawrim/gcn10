#include "header.h"
#include <omp.h>
#include <stdlib.h>
#include <stdio.h>

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

void read_raster_cell_by_cell(const char *filename, cell_callback callback, void *user_data) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    int rows = GDALGetRasterYSize(dataset);
    int cols = GDALGetRasterXSize(dataset);
    double value;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            if (GDALRasterIO(band, GF_Read, col, row, 1, 1, &value, 1, 1, GDT_Float64, 0, 0) != CE_None) {
                fprintf(stderr, "Error reading cell [%d, %d] from %s\n", row, col, filename);
                exit(EXIT_FAILURE);
            }
            callback(row, col, value, user_data);
        }
    }

    GDALClose(dataset);
}

void write_raster_cell_by_cell(const char *filename, int rows, int cols, double no_data_value,
                               const char *ref_filename, cell_callback callback, void *user_data) {
    GDALAllRegister();
    GDALDatasetH ref_dataset = GDALOpen(ref_filename, GA_ReadOnly);
    if (!ref_dataset) {
        fprintf(stderr, "Error: Unable to open reference raster file %s\n", ref_filename);
        exit(EXIT_FAILURE);
    }

    GDALDriverH driver = GDALGetDriverByName("GTiff");
    GDALDatasetH output_dataset = GDALCreate(driver, filename, cols, rows, 1, GDT_Float64, NULL);
    if (!output_dataset) {
        fprintf(stderr, "Error: Unable to create output raster file %s\n", filename);
        GDALClose(ref_dataset);
        exit(EXIT_FAILURE);
    }

    GDALSetGeoTransform(output_dataset, (double[6]){0});
    GDALSetProjection(output_dataset, GDALGetProjectionRef(ref_dataset));

    GDALRasterBandH band = GDALGetRasterBand(output_dataset, 1);
    GDALSetRasterNoDataValue(band, no_data_value);

    double value;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            callback(row, col, value, user_data);
            if (GDALRasterIO(band, GF_Write, col, row, 1, 1, &value, 1, 1, GDT_Float64, 0, 0) != CE_None) {
                fprintf(stderr, "Error writing cell [%d, %d] to %s\n", row, col, filename);
                exit(EXIT_FAILURE);
            }
        }
    }

    GDALClose(output_dataset);
    GDALClose(ref_dataset);
}
