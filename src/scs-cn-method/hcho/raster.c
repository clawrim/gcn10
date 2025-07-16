// This GDAL I/O is from meshed (https://github.com/HuidaeCho/meshed) with slight modifications

#include <omp.h>
#include <stdlib.h>
#include <stdio.h>
#include "global.h"

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "Error: Unable to allocate memory for raster structure\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = malloc(sizeof(double) * nrows * ncols);
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

Raster *read_raster(const char *filename) {
    int rows, cols;
    double no_data_value;
    get_raster_metadata(filename, &rows, &cols, &no_data_value);

    Raster *rast = allocate_raster(rows, cols, no_data_value);
    if (!rast)
        return NULL;

    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
        free_raster(rast);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    int error = 0;

    #pragma omp parallel for schedule(dynamic)
    for (int row = 0; row < rows; row++) {
        int status;
        #pragma omp critical
        status = GDALRasterIO(band, GF_Read, 0, row, cols, 1, &rast->data[row * cols],
                              cols, 1, GDT_Byte, 0, 0);
        if (status != CE_None) {
            #pragma omp critical
            fprintf(stderr, "Error: Unable to read row %d from %s\n", row, filename);
            error = 1;
        }
    }

    GDALClose(dataset);
    if (error) {
        free_raster(rast);
        return NULL;
    }
    return rast;
}

void write_raster(const char *filename, const Raster *rast, const char *ref_filename) {
    GDALAllRegister();
    GDALDatasetH ref_dataset = GDALOpen(ref_filename, GA_ReadOnly);
    if (!ref_dataset) {
        fprintf(stderr, "Error: Unable to open reference raster file %s\n", ref_filename);
        exit(EXIT_FAILURE);
    }

    double geotransform[6];
    if (GDALGetGeoTransform(ref_dataset, geotransform) != CE_None) {
        fprintf(stderr, "Error: Unable to retrieve geotransform from %s\n", ref_filename);
        GDALClose(ref_dataset);
        exit(EXIT_FAILURE);
    }

    const char *projection = GDALGetProjectionRef(ref_dataset);
    if (!projection) {
        fprintf(stderr, "Warning: Reference raster has no projection information\n");
        projection = "";
    }

    GDALDriverH driver = GDALGetDriverByName("GTiff");
    GDALDatasetH output_dataset = GDALCreate(driver, filename, rast->ncols, rast->nrows, 1, GDT_Float64, NULL);
    if (!output_dataset) {
        fprintf(stderr, "Error: Unable to create output raster file %s\n", filename);
        GDALClose(ref_dataset);
        exit(EXIT_FAILURE);
    }

    // Set georeferencing
    if (GDALSetGeoTransform(output_dataset, geotransform) != CE_None) {
        fprintf(stderr, "Error: Unable to set geotransform for output raster\n");
        GDALClose(ref_dataset);
        GDALClose(output_dataset);
        exit(EXIT_FAILURE);
    }

    if (GDALSetProjection(output_dataset, projection) != CE_None) {
        fprintf(stderr, "Error: Unable to set projection for output raster\n");
        GDALClose(ref_dataset);
        GDALClose(output_dataset);
        exit(EXIT_FAILURE);
    }

    GDALRasterBandH band = GDALGetRasterBand(output_dataset, 1);
    GDALSetRasterNoDataValue(band, rast->no_data_value);

    int error = 0;
    for (int row = 0; row < rast->nrows; row++) {
        if (GDALRasterIO(band, GF_Write, 0, row, rast->ncols, 1, &rast->data[row * rast->ncols],
                         rast->ncols, 1, GDT_Float64, 0, 0) != CE_None) {
            #pragma omp critical
            fprintf(stderr, "Error: Unable to write row %d to %s\n", row, filename);
            error = 1;
        }
    }

    GDALClose(output_dataset);
    GDALClose(ref_dataset);

    if (error) {
        fprintf(stderr, "Error: Failed to write raster\n");
        exit(EXIT_FAILURE);
    }
}
