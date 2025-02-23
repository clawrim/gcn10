#include <stdio.h>
#include <stdlib.h>
#include "global.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <rainfall.tif> <curve_number.tif> <output_runoff.tif> <threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[4]);
    omp_set_num_threads(num_threads);
    GDALSetCacheMax(1024 * 1024 * 512); // 512mb cache

    printf("Starting SCS runoff calculation with %d threads...\n", num_threads);

    Raster *rainfall = read_raster_parallel(argv[1], num_threads);
    Raster *curve_number = read_raster_parallel(argv[2], num_threads);
    if (!rainfall || !curve_number) {
        fprintf(stderr, "Error: Failed to read input rasters (nrows=%d, ncols=%d)\n", 
                rainfall ? rainfall->nrows : 0, curve_number ? curve_number->nrows : 0);
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }

    Raster *runoff = allocate_raster(curve_number->nrows, curve_number->ncols, curve_number->no_data_value);
    if (!runoff) {
        fprintf(stderr, "Error: Failed to allocate runoff raster (nrows=%d, ncols=%d)\n", 
                curve_number->nrows, curve_number->ncols);
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }

    printf("Computing runoff for raster (nrows=%d, ncols=%d)...\n", runoff->nrows, runoff->ncols);
    calculate_runoff(rainfall, curve_number, runoff);

    printf("Converting runoff to Float32 for output...\n");
    float *runoff_float = malloc(runoff->nrows * runoff->ncols * sizeof(float));
    if (!runoff_float) {
        fprintf(stderr, "Error: Failed to allocate Float32 buffer (size=%lu bytes)\n", 
                runoff->nrows * runoff->ncols * sizeof(float));
        free_raster(rainfall);
        free_raster(curve_number);
        free_raster(runoff);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < runoff->nrows * runoff->ncols; i++) {
        runoff_float[i] = (float)runoff->data[i];
    }

    printf("Writing runoff raster to %s...\n", argv[3]);
    write_raster(argv[3], runoff_float, runoff->nrows, runoff->ncols, runoff->no_data_value, argv[2]);

    free_raster(rainfall);
    free_raster(curve_number);
    free(runoff_float);
    free_raster(runoff);
    printf("SCS runoff calculation completed successfully (nrows=%d, ncols=%d)\n", 
           runoff->nrows, runoff->ncols);
    return 0;
}
