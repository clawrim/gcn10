#include <stdio.h>
#include <stdlib.h>
#include "header.h"

typedef struct {
    double *curve_number;
    double no_data_value;
    double *runoff;
    int cols;
} ProcessingData;

// Callback for reading curve number
void read_curve_number_callback(int row, int col, double value, void *user_data) {
    ProcessingData *data = (ProcessingData *)user_data;
    data->curve_number[row * data->cols + col] = value;
}

// Callback for processing rainfall and calculating runoff
void process_rainfall_callback(int row, int col, double rainfall, void *user_data) {
    ProcessingData *data = (ProcessingData *)user_data;
    double curve_number = data->curve_number[row * data->cols + col];

    if (rainfall < 0 || curve_number < 0) {
        data->runoff[row * data->cols + col] = data->no_data_value;
        return;
    }

    double S = (25400.0 / curve_number) - 254.0;
    double Q = (rainfall > 0.2 * S) ? ((rainfall - 0.2 * S) * (rainfall - 0.2 * S)) / (rainfall + 0.8 * S) : 0.0;
    data->runoff[row * data->cols + col] = Q;
}

// Callback for writing runoff raster
void write_runoff_callback(int row, int col, double value, void *user_data) {
    ProcessingData *data = (ProcessingData *)user_data;
    value = data->runoff[row * data->cols + col];
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <rainfall.tif> <curve_number.tif> <output_runoff.tif>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int rows, cols;
    double no_data_value;

    // Initialize raster metadata
    get_raster_metadata(argv[2], &rows, &cols, &no_data_value);

    // Initialize data struct
    ProcessingData data;
    data.cols = cols;
    data.no_data_value = no_data_value;

    // Allocate memory for curve number and runoff rasters
    data.curve_number = malloc(rows * cols * sizeof(double));
    data.runoff = malloc(rows * cols * sizeof(double));
    if (!data.curve_number || !data.runoff) {
        fprintf(stderr, "Error allocating memory\n");
        free(data.curve_number);
        free(data.runoff);
        return EXIT_FAILURE;
    }

    // Read curve number raster
    read_raster_cell_by_cell(argv[2], read_curve_number_callback, &data);

    // Process rainfall raster and calculate runoff
    read_raster_cell_by_cell(argv[1], process_rainfall_callback, &data);

    // Write runoff raster
    write_raster_cell_by_cell(argv[3], rows, cols, no_data_value, argv[1], write_runoff_callback, &data);

    // Free memory
    free(data.curve_number);
    free(data.runoff);

    return EXIT_SUCCESS;
}
