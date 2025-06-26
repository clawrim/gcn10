#include "global.h"

void calculate_runoff(Raster *rainfall, const Raster *curve_number, Raster *runoff) {
    int nrows = runoff->nrows;
    int ncols = runoff->ncols;
    printf("Calculating runoff for raster (nrows=%d, ncols=%d)...\n", nrows, ncols);
    #pragma omp parallel for schedule(static) num_threads(6)
    for (int row = 0; row < nrows; row++) {
        for (int col = 0; col < ncols; col++) {
            int i = row * ncols + col;
            double R = rainfall->data[i]; // direct access
            double CN = curve_number->data[i];
            if (R < 0 || CN < 0) {
                runoff->data[i] = runoff->no_data_value;
            } else {
                double S = (25400.0 / CN) - 254.0;
                double Ia = 0.2 * S;
                double Q = (R > Ia) ? ((R - Ia) * (R - Ia)) / (R + 0.8 * S) : 0.0;
                runoff->data[i] = Q;
            }
        }
    }
}
