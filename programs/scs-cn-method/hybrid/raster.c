#include "global.h"
#include <sys/stat.h>
#include <sys/types.h>

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

void ensure_directory_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);  // Create directory with full permissions
    }
}

void distribute_and_process_blocks(const char *cn_list_file, const char *rainfall_list_file, const char *output_dir, int num_threads) {
    int rank, size; MPI_Comm_rank(MPI_COMM_WORLD, &rank); MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (rank == 0 && mkdir(output_dir, 0777) && errno != EEXIST) { fprintf(stderr, "Rank 0: Failed to create %s\n", output_dir); MPI_Finalize(); exit(EXIT_FAILURE); }
    MPI_Barrier(MPI_COMM_WORLD);

    FILE *cn_fp = fopen(cn_list_file, "r"), *rain_fp = fopen(rainfall_list_file, "r");
    if (!cn_fp || !rain_fp) { fprintf(stderr, "Rank %d: Error opening list files\n", rank); if (cn_fp) fclose(cn_fp); if (rain_fp) fclose(rain_fp); MPI_Finalize(); exit(EXIT_FAILURE); }

    char cn_file[256], rainfall_file[256]; int total_blocks = 0;
    while (fgets(cn_file, 256, cn_fp) && fgets(rainfall_file, 256, rain_fp)) total_blocks++;
    rewind(cn_fp); rewind(rain_fp);

    const char *slash = output_dir[strlen(output_dir)-1] == '/' ? "" : "/";
    if (size == 1) {
        for (int i = 0; i < total_blocks; i++) {
            fgets(cn_file, 256, cn_fp); fgets(rainfall_file, 256, rain_fp);
            cn_file[strcspn(cn_file, "\n")] = rainfall_file[strcspn(rainfall_file, "\n")] = 0;
            Raster *cn = read_raster_mpi(cn_file, 0), *rain = read_raster_mpi(rainfall_file, 0), *runoff;
            if (cn && rain && (runoff = allocate_raster(cn->nrows, cn->ncols, cn->no_data_value))) {
                calculate_runoff(rain, cn, runoff);
                char out_file[300]; snprintf(out_file, 300, "%s%soutput_rank0_block%d.tif", output_dir, slash, i);
                write_raster_mpi(out_file, runoff, cn_file, 0, i);
                free_raster(runoff);
            }
            free_raster(cn); free_raster(rain);
        }
    } else if (rank == 0) {
        int next_block = 0, finished_blocks = 0;
        for (int i = 1; i < size && next_block < total_blocks; i++) MPI_Send(&next_block, 1, MPI_INT, i, 0, MPI_COMM_WORLD), next_block++;
        while (finished_blocks < total_blocks) {
            MPI_Status status; int block; MPI_Recv(&block, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
            int signal = next_block < total_blocks ? next_block++ : -1;
            MPI_Send(&signal, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); finished_blocks++;
        }
    } else {
        while (1) {
            int block; MPI_Status status; MPI_Recv(&block, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            if (block < 0) break;
            fgets(cn_file, 256, cn_fp); fgets(rainfall_file, 256, rain_fp);
            cn_file[strcspn(cn_file, "\n")] = rainfall_file[strcspn(rainfall_file, "\n")] = 0;
            Raster *cn = read_raster_mpi(cn_file, rank), *rain = read_raster_mpi(rainfall_file, rank), *runoff;
            if (cn && rain && (runoff = allocate_raster(cn->nrows, cn->ncols, cn->no_data_value))) {
                calculate_runoff(rain, cn, runoff);
                char out_file[300]; snprintf(out_file, 300, "%s%soutput_rank%d_block%d.tif", output_dir, slash, rank, block);
                write_raster_mpi(out_file, runoff, cn_file, rank, block);
                free_raster(runoff);
            }
            free_raster(cn); free_raster(rain);
            MPI_Send(&block, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
        }
    }

    fclose(cn_fp); fclose(rain_fp);
}

Raster *read_raster_mpi(const char *filename, int rank) {
    //GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    GDALAllRegister(); 
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);

    if (!dataset) {
        fprintf(stderr, "MPI Rank %d: Error opening raster file %s\n", rank, filename);
        return NULL;
    }

    int rows = GDALGetRasterYSize(dataset);
    int cols = GDALGetRasterXSize(dataset);
    
    int has_no_data;
    double tmp_no_data = GDALGetRasterNoDataValue(GDALGetRasterBand(dataset, 1), &has_no_data);
    double no_data_value = has_no_data ? tmp_no_data : -9999;

    Raster *rast = allocate_raster(rows, cols, no_data_value);
    if (!rast) {
        GDALClose(dataset);
        return NULL;
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Read, 0, 0, cols, rows, rast->data, cols, rows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "MPI Rank %d: Error reading raster data from %s\n", rank, filename);
        free_raster(rast);
        GDALClose(dataset);
        return NULL;
    }

    GDALGetGeoTransform(dataset, rast->geotransform);
    GDALClose(dataset);
    return rast;
}

void write_raster_mpi(const char *filename, Raster *rast, const char *ref_file, int rank, int block_index) {
    GDALDatasetH ref_dataset = GDALOpen(ref_file, GA_ReadOnly);
    if (!ref_dataset) {
        fprintf(stderr, "MPI Rank %d: Error opening reference raster %s\n", rank, ref_file);
        return;
    }

    GDALDriverH driver = GDALGetDriverByName("GTiff");
    if (!driver) {
        fprintf(stderr, "MPI Rank %d: Error retrieving GDAL GTiff driver.\n", rank);
        GDALClose(ref_dataset);
        return;
    }

    char unique_filename[512];
    snprintf(unique_filename, sizeof(unique_filename), "%s", filename);

    GDALDatasetH dataset = GDALCreate(driver, unique_filename, rast->ncols, rast->nrows, 1, GDT_Float64, NULL);
    if (!dataset) {
        fprintf(stderr, "MPI Rank %d: Error creating raster file %s\n", rank, unique_filename);
        GDALClose(ref_dataset);
        return;
    }

    double transform[6];
    GDALGetGeoTransform(ref_dataset, transform);
    GDALSetGeoTransform(dataset, transform);
    GDALSetProjection(dataset, GDALGetProjectionRef(ref_dataset));

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    GDALSetRasterNoDataValue(band, rast->no_data_value);
//  GDALRasterIO(band, GF_Write, 0, 0, rast->ncols, rast->nrows, rast->data, rast->ncols, rast->nrows, GDT_Float64, 0, 0);
    if (GDALRasterIO(band, GF_Write, 0, 0, rast->ncols, rast->nrows, rast->data, rast->ncols, rast->nrows, GDT_Float64, 0, 0) != CE_None) {
	    fprintf(stderr, "MPI Rank %d: Error writing raster data to %s\n", rank, filename);
    }


    GDALClose(dataset);
    GDALClose(ref_dataset);
}


Raster *read_raster(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open raster file %s\n", filename);
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
        fprintf(stderr, "Error: Failed to read raster data from %s\n", filename);
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
        fprintf(stderr, "Error: Unable to open reference raster %s\n", ref_file);
        return;
    }

    double geotransform[6];
    GDALGetGeoTransform(ref_dataset, geotransform);
    const char *projection = GDALGetProjectionRef(ref_dataset);

    GDALDatasetH dataset = GDALCreate(GDALGetDriverByName("GTiff"), filename, rast->ncols, rast->nrows, 1, GDT_Float64, NULL);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to create raster file %s\n", filename);
        GDALClose(ref_dataset);
        return;
    }

    GDALSetGeoTransform(dataset, geotransform);
    GDALSetProjection(dataset, projection);

    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (GDALRasterIO(band, GF_Write, 0, 0, rast->ncols, rast->nrows, rast->data, rast->ncols, rast->nrows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Error: Failed to write raster data to %s\n", filename);
    }

    GDALClose(dataset);
    GDALClose(ref_dataset);
}

GDALDatasetH open_rainfall_dataset(const char *filename) {
    GDALAllRegister();
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        fprintf(stderr, "Error: Unable to open rainfall raster %s\n", filename);
    }
    return dataset;
}

