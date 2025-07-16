#include <stdio.h>
#include <stdlib.h>
#include "header.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <rainfall.tif> <curve_number.tif> <output_runoff.tif>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Processing raster tiles...\n");
    if (process_runoff_timestep(argv[1], argv[2], argv[3], 1000) != 0) { // Tile size = 1000
        fprintf(stderr, "Error processing runoff.\n");
        return EXIT_FAILURE;
    }

    printf("Runoff calculation completed successfully.\n");
    return EXIT_SUCCESS;
}
