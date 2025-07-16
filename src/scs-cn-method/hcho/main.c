
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "global.h"

int main(int argc, char *argv[]) {
    struct timeval start_time, end_time;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <rainfall.tif> <curve_number.tif> <output_runoff.tif>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Reading rainfall raster...\n");
    gettimeofday(&start_time, NULL);
    Raster *rainfall = read_raster(argv[1]);
    if (!rainfall) {
        fprintf(stderr, "Error reading rainfall raster\n");
        return EXIT_FAILURE;
    }
    gettimeofday(&end_time, NULL);
    printf("\t%lld ms\n", timeval_diff(NULL, &end_time, &start_time));

    printf("Reading curve number raster...\n");
    gettimeofday(&start_time, NULL);
    Raster *curve_number = read_raster(argv[2]);
    if (!curve_number) {
        fprintf(stderr, "Error reading curve number raster\n");
        free_raster(rainfall);
        return EXIT_FAILURE;
    }
    gettimeofday(&end_time, NULL);
    printf("\t%lld ms\n", timeval_diff(NULL, &end_time, &start_time));

    printf("Allocating memory for runoff raster...\n");
    gettimeofday(&start_time, NULL);
    Raster *runoff = allocate_raster(rainfall->nrows, rainfall->ncols, rainfall->no_data_value);
    if (!runoff) {
        fprintf(stderr, "Error allocating memory for runoff raster\n");
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }
    gettimeofday(&end_time, NULL);
    printf("\t%lld ms\n", timeval_diff(NULL, &end_time, &start_time));

    printf("Calculating runoff...\n");
    gettimeofday(&start_time, NULL);
    calculate_runoff(rainfall, curve_number, runoff);
    gettimeofday(&end_time, NULL);
#pragma omp parallel
#pragma omp single
    printf("compute\t%d threads, %lld ms\n", omp_get_num_threads(), timeval_diff(NULL, &end_time, &start_time));

    printf("Writing runoff raster...\n");
    gettimeofday(&start_time, NULL);
    write_raster(argv[3], runoff, argv[1]);
    gettimeofday(&end_time, NULL);
    printf("\t%lld ms\n", timeval_diff(NULL, &end_time, &start_time));

    free_raster(rainfall);
    free_raster(curve_number);
    free_raster(runoff);

    printf("Runoff calculation completed successfully.\n");
    return EXIT_SUCCESS;
}
