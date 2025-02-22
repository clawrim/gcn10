#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "global.h"

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 5) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s <cn_rasters_list.txt> <rainfall_rasters_list.txt> <output_dir> <threads>\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[4]);
    omp_set_num_threads(num_threads);

    FILE *cn_file = fopen(argv[1], "r");
    FILE *rainfall_file = fopen(argv[2], "r");
    if (!cn_file || !rainfall_file) {
        fprintf(stderr, "Error: Unable to open input raster list files\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    char cn_filename[256], rainfall_filename[256];
    int file_count = 0;
    while (fgets(cn_filename, sizeof(cn_filename), cn_file) && fgets(rainfall_filename, sizeof(rainfall_filename), rainfall_file)) {
        file_count++;
    }
    rewind(cn_file);
    rewind(rainfall_file);

    int files_per_rank = file_count / size;
    int remainder = file_count % size;

    int start_idx = rank * files_per_rank + (rank < remainder ? rank : remainder);
    int end_idx = start_idx + files_per_rank + (rank < remainder);

    for (int i = start_idx; i < end_idx; i++) {
        fgets(cn_filename, sizeof(cn_filename), cn_file);
        fgets(rainfall_filename, sizeof(rainfall_filename), rainfall_file);

        cn_filename[strcspn(cn_filename, "\n")] = 0;
        rainfall_filename[strcspn(rainfall_filename, "\n")] = 0;

        printf("MPI Rank %d processing: CN=%s, Rainfall=%s\n", rank, cn_filename, rainfall_filename);

        Raster *curve_number = read_raster(cn_filename);
        Raster *rainfall = read_raster(rainfall_filename);
        if (!curve_number || !rainfall) {
            fprintf(stderr, "MPI Rank %d: Error reading raster data\n", rank);
            continue;
        }

        Raster *runoff = allocate_raster(curve_number->nrows, curve_number->ncols, curve_number->no_data_value);
        double cn_geotransform[6];
        get_geotransform(cn_filename, cn_geotransform);

        calculate_runoff(rainfall, curve_number, runoff);

        char output_filename[512];
        snprintf(output_filename, sizeof(output_filename), "%s/runoff_%d.tif", argv[3], rank);
        write_raster(output_filename, runoff, cn_filename);

        free_raster(curve_number);
        free_raster(rainfall);
        free_raster(runoff);
    }

    fclose(cn_file);
    fclose(rainfall_file);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
