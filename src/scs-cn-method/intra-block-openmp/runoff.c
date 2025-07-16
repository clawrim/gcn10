#include "global.h"

double get_rainfall_value(GDALDatasetH rainfall_ds, int col, int row, double *cn_geotransform) {
    double rainfall_geotransform[6];
    GDALGetGeoTransform(rainfall_ds, rainfall_geotransform);

    // convert CN (col, row) into lat-lon coordinates
    double x = cn_geotransform[0] + col * cn_geotransform[1] + row * cn_geotransform[2];
    double y = cn_geotransform[3] + col * cn_geotransform[4] + row * cn_geotransform[5];

    // convert coordinates into rainfall raster pixel coordinates
    int px = (int)((x - rainfall_geotransform[0]) / rainfall_geotransform[1]);
    int py = (int)((y - rainfall_geotransform[3]) / rainfall_geotransform[5]);

    int rows = GDALGetRasterYSize(rainfall_ds);
    int cols = GDALGetRasterXSize(rainfall_ds);

    // check if the pixel is within bounds
    if (px < 0 || px >= cols || py < 0 || py >= rows)
        return -9999; // No-data value

    double rainfall_value;
    GDALRasterBandH band = GDALGetRasterBand(rainfall_ds, 1);
    
    // check if GDALRasterIO reads successfully
    if (GDALRasterIO(band, GF_Read, px, py, 1, 1, &rainfall_value, 1, 1, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Warning: GDALRasterIO failed to read rainfall at (%d, %d)\n", px, py);
        return -9999;
    }

    return rainfall_value;
}

void calculate_runoff(GDALDatasetH rainfall_ds, const Raster *curve_number, Raster *runoff, double *cn_geotransform) {
    int nrows = runoff->nrows;
    int ncols = runoff->ncols;

    // Preload rainfall raster into memory
    double *rainfall_data = malloc(nrows * ncols * sizeof(double));
    if (!rainfall_data) {
        fprintf(stderr, "Error: Unable to allocate memory for rainfall data.\n");
        return;
    }

    GDALRasterBandH band = GDALGetRasterBand(rainfall_ds, 1);
    if (GDALRasterIO(band, GF_Read, 0, 0, ncols, nrows, rainfall_data, ncols, nrows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "Error: Failed to read entire rainfall raster.\n");
        free(rainfall_data);
        return;
    }

#pragma omp parallel for
    for (int row = 0; row < nrows; row++) {
        for (int col = 0; col < ncols; col++) {
            int i = row * ncols + col;
            double rainfall = rainfall_data[i];  // Use preloaded data
            double CN = curve_number->data[i];

            if (rainfall < 0 || CN < 0) {
                runoff->data[i] = runoff->no_data_value;
            } else {
                double S = (25400.0 / CN) - 254.0;
                double Q = (rainfall > 0.2 * S) ? ((rainfall - 0.2 * S) * (rainfall - 0.2 * S)) / (rainfall + 0.8 * S) : 0.0;
                runoff->data[i] = Q;
            }
        }
    }

    free(rainfall_data);
}
