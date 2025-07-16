#include "global.h"

void calculate_runoff(const Raster *rainfall, const Raster *curve_number, Raster *runoff) {
    int row, col;

#pragma omp parallel for schedule(static) private(col)
    for (row = 0; row < runoff->nrows; row++) {
        for (col = 0; col < runoff->ncols; col++) {
	    int i = row * runoff->ncols + col;
            double R = rainfall->data[i];
            double CN = curve_number->data[i];

            if (R < 0 || CN < 0)
                runoff->data[i] = runoff->no_data_value;
	    else{
		double S = (25400.0 / CN) - 254.0;
		double Q = (R > 0.2 * S) ? ((R - 0.2 * S) * (R - 0.2 * S)) / (R + 0.8 * S) : 0.0;
		runoff->data[i] = Q;
	    }
        }
    }
}
