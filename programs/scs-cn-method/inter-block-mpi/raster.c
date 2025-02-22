#include "global.h"

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "error: unable to allocate memory for raster structure\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = malloc(nrows * ncols * sizeof(double));
    if (!rast->data) {
        fprintf(stderr, "error: unable to allocate memory for raster data\n");
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

Raster *read_raster(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "error: unable to open raster file %s\n", filename);
        return NULL;
    }

    int rows = GDALGetRasterYSize(dataset);
    int cols = GDALGetRasterXSize(dataset);
    double no_data_value = GDALGetRasterNoDataValue(GDALGetRasterBand(dataset, 1), NULL);

    Raster *rast = allocate_raster(rows, cols, no_data_value);
    if (!rast) {
        GDALClose(dataset);
        return NULL;
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Read, 0, 0, cols, rows, rast->data, cols, rows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "error: failed to read raster data from %s\n", filename);
        free_raster(rast);
        GDALClose(dataset);
        return NULL;
    }

    GDALClose(dataset);
    return rast;
}

void write_raster(const char *filename, Raster *rast, const char *ref_file) {
    GDALAllRegister();

    GDALDatasetH ref_dataset = GDALOpen(ref_file, GA_ReadOnly);
    if (!ref_dataset) {
        fprintf(stderr, "error: unable to open reference raster %s\n", ref_file);
        return;
    }

    double geotransform[6];
    GDALGetGeoTransform(ref_dataset, geotransform);
    const char *projection = GDALGetProjectionRef(ref_dataset);

    GDALDatasetH dataset = GDALCreate(GDALGetDriverByName("GTiff"), filename, rast->ncols, rast->nrows, 1, GDT_Float64, NULL);
    if (!dataset) {
        fprintf(stderr, "error: unable to create raster file %s\n", filename);
        GDALClose(ref_dataset);
        return;
    }

    GDALSetGeoTransform(dataset, geotransform);
    GDALSetProjection(dataset, projection);

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Write, 0, 0, rast->ncols, rast->nrows, rast->data, rast->ncols, rast->nrows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "error: failed to write raster data to %s\n", filename);
    }

    GDALClose(dataset);
    GDALClose(ref_dataset);
}

GDALDatasetH open_rainfall_dataset(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "error: unable to open rainfall raster %s\n", filename);
    }
    return dataset;
}

void distribute_and_process_blocks(const char *cn_list_file, const char *rainfall_list_file, const char *output_dir, int num_threads) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    FILE *cn_fp = fopen(cn_list_file, "r");
    FILE *rain_fp = fopen(rainfall_list_file, "r");
    if (!cn_fp || !rain_fp) {
        if (rank == 0) fprintf(stderr, "error opening cn or rainfall list file\n");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    char cn_file[256], rainfall_file[256];
    int block_index = 0;

    while (fscanf(cn_fp, "%s", cn_file) != EOF && fscanf(rain_fp, "%s", rainfall_file) != EOF) {
        if (block_index % size == rank) {
            printf("mpi rank %d processing: cn=%s, rainfall=%s\n", rank, cn_file, rainfall_file);

            GDALDatasetH rainfall_ds = open_rainfall_dataset(rainfall_file);
            Raster *curve_number = read_raster(cn_file);
            if (!rainfall_ds || !curve_number) {
                fprintf(stderr, "mpi rank %d: error loading raster data for %s\n", rank, cn_file);
                continue;
            }

            double cn_geotransform[6];
            GDALDatasetH cn_ds = GDALOpen(cn_file, GA_ReadOnly);
            GDALGetGeoTransform(cn_ds, cn_geotransform);
            GDALClose(cn_ds);

            Raster *runoff = allocate_raster(curve_number->nrows, curve_number->ncols, curve_number->no_data_value);

            calculate_runoff(rainfall_ds, curve_number, runoff, cn_geotransform);

            char output_file[300];
            snprintf(output_file, sizeof(output_file), "%s/runoff_%d.tif", output_dir, block_index);
            write_raster(output_file, runoff, cn_file);

            free_raster(curve_number);
            free_raster(runoff);
            GDALClose(rainfall_ds);
        }
        block_index++;
    }

    fclose(cn_fp);
    fclose(rain_fp);
}

