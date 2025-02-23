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

    printf("processing single block\n");

    Raster *rainfall = read_raster_parallel(argv[1], num_threads);
    Raster *curve_number = read_raster_parallel(argv[2], num_threads);
    if (!rainfall || !curve_number) {
        fprintf(stderr, "error reading rasters\n");
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }

    GDALDatasetH rain_ds = GDALOpen(argv[1], GA_ReadOnly);
    GDALDatasetH cn_ds = GDALOpen(argv[2], GA_ReadOnly);
    double rainfall_gt[6], cn_geotransform[6];
    GDALGetGeoTransform(rain_ds, rainfall_gt);
    GDALGetGeoTransform(cn_ds, cn_geotransform);
    GDALClose(rain_ds);
    GDALClose(cn_ds);

    Raster *runoff = allocate_raster(curve_number->nrows, curve_number->ncols, curve_number->no_data_value);
    if (!runoff) {
        fprintf(stderr, "error allocating runoff raster\n");
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }

    printf("computing runoff...\n");
    calculate_runoff(rainfall, curve_number, runoff, cn_geotransform, rainfall_gt);

    // pre-convert to float32
    printf("converting to float32...\n");
    float *runoff_float = malloc(runoff->nrows * runoff->ncols * sizeof(float));
    if (!runoff_float) {
        fprintf(stderr, "error: alloc float buffer\n");
        free_raster(rainfall);
        free_raster(curve_number);
        free_raster(runoff);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < runoff->nrows * runoff->ncols; i++) {
        runoff_float[i] = (float)runoff->data[i];
    }

    printf("writing runoff raster...\n");
    write_raster(argv[3], runoff_float, runoff->nrows, runoff->ncols, runoff->no_data_value, argv[2]);

    free_raster(rainfall);
    free_raster(curve_number);
    free(runoff_float);
    free_raster(runoff);
    printf("single block processing completed successfully.\n");
    return 0;
}
