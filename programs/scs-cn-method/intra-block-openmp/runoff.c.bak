#include "global.h"

double get_rainfall_value(Raster *rainfall, int col, int row, double *cn_geotransform, double *rainfall_gt) {
    double x = cn_geotransform[0] + col * cn_geotransform[1] + row * cn_geotransform[2];
    double y = cn_geotransform[3] + col * cn_geotransform[4] + row * cn_geotransform[5];
    int px = (int)((x - rainfall_gt[0]) / rainfall_gt[1]);
    int py = (int)((y - rainfall_gt[3]) / rainfall_gt[5]);
    int rows = rainfall->nrows;
    int cols = rainfall->ncols;
    if (px < 0 || px >= cols || py < 0 || py >= rows) {
        return -9999;
    }
    return rainfall->data[py * cols + px];
}

void calculate_runoff(Raster *rainfall, const Raster *curve_number, Raster *runoff, double *cn_geotransform, double *rainfall_gt) {
    int nrows = runoff->nrows;
    int ncols = runoff->ncols;
    #pragma omp parallel for schedule(dynamic, 16)
    for (int row = 0; row < nrows; row++) {
        for (int col = 0; col < ncols; col += 4) {
            int i = row * ncols + col;
            for (int k = 0; k < 4 && col + k < ncols; k++) {
                double R = get_rainfall_value(rainfall, col + k, row, cn_geotransform, rainfall_gt);
                double CN = curve_number->data[i + k];
                if (R < 0 || CN < 0) {
                    runoff->data[i + k] = runoff->no_data_value;
                } else {
                    double S = (25400.0 / CN) - 254.0;
                    double Ia = 0.2 * S;
                    runoff->data[i + k] = (R > Ia) ? ((R - Ia) * (R - Ia)) / (R + 0.8 * S) : 0.0;
                }
            }
        }
    }
}
