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

    printf("Processing single block\n");

    // Open Rainfall raster (large extent)
    GDALDatasetH rainfall_ds = open_rainfall_dataset(argv[1]);
    if (!rainfall_ds) {
        fprintf(stderr, "Error: Unable to open rainfall raster %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // read CN raster
    Raster *curve_number = read_raster(argv[2]);
    if (!curve_number) {
        fprintf(stderr, "Error reading curve number raster %s\n", argv[2]);
        GDALClose(rainfall_ds);
        return EXIT_FAILURE;
    }

    // get CN geotransform
    GDALDatasetH cn_ds = GDALOpen(argv[2], GA_ReadOnly);
    double cn_geotransform[6];
    GDALGetGeoTransform(cn_ds, cn_geotransform);
    GDALClose(cn_ds);

    // allocate runoff raster
    Raster *runoff = allocate_raster(curve_number->nrows, curve_number->ncols, curve_number->no_data_value);

    // compute runoff
    printf("Computing runoff...\n");
    calculate_runoff(rainfall_ds, curve_number, runoff, cn_geotransform);

    // write runoff raster
    printf("Writing runoff raster...\n");
    write_raster(argv[3], runoff, argv[2]);

    // fee allocated memory
    free_raster(curve_number);
    free_raster(runoff);
    GDALClose(rainfall_ds);

    // print success
    printf("Single block processing completed successfully.\n");
    return 0;
}

