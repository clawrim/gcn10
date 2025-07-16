
#include <stdio.h>
#include <stdlib.h>
#include "header.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <rainfall.tif> <curve_number.tif> <output_runoff.tif>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Reading rainfall raster...\n");
    Raster *rainfall = read_raster(argv[1]);
    if (!rainfall) {
        fprintf(stderr, "Error reading rainfall raster\n");
        return EXIT_FAILURE;
    }

    printf("Reading curve number raster...\n");
    Raster *curve_number = read_raster(argv[2]);
    if (!curve_number) {
        fprintf(stderr, "Error reading curve number raster\n");
        free_raster(rainfall);
        return EXIT_FAILURE;
    }

    printf("Allocating memory for runoff raster...\n");
    Raster *runoff = allocate_raster(rainfall->nrows, rainfall->ncols, rainfall->no_data_value);
    if (!runoff) {
        fprintf(stderr, "Error allocating memory for runoff raster\n");
        free_raster(rainfall);
        free_raster(curve_number);
        return EXIT_FAILURE;
    }

    printf("Calculating runoff...\n");
    calculate_runoff(rainfall, curve_number, runoff);

    printf("Writing runoff raster...\n");
    write_raster(argv[3], runoff, argv[1]);

    free_raster(rainfall);
    free_raster(curve_number);
    free_raster(runoff);

    printf("Runoff calculation completed successfully.\n");
    return EXIT_SUCCESS;
}
