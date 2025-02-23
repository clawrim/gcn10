#include "global.h"

typedef struct {
    const char *filename;
    Raster **rast;
} ReadArgs;

typedef struct {
    GDALDatasetH ds;
    float *data;
    int start_row;
    int num_rows;
    int ncols;
} WriteChunkArgs;

void *read_raster_thread(void *args) {
    ReadArgs *rargs = (ReadArgs *)args;
    *rargs->rast = read_raster(rargs->filename);
    return NULL;
}

void *write_raster_chunk(void *args) {
    WriteChunkArgs *wargs = (WriteChunkArgs *)args;
    GDALRasterBandH band = GDALGetRasterBand(wargs->ds, 1);
    int offset = wargs->start_row * wargs->ncols;
    if (GDALRasterIO(band, GF_Write, 0, wargs->start_row, wargs->ncols, wargs->num_rows,
                     wargs->data + offset, wargs->ncols, wargs->num_rows, GDT_Float32, 0, 0) != CE_None) {
        fprintf(stderr, "error: writing chunk at row %d\n", wargs->start_row);
    }
    return NULL;
}

Raster *allocate_raster(int nrows, int ncols, double no_data_value) {
    Raster *rast = malloc(sizeof(Raster));
    if (!rast) {
        fprintf(stderr, "error: alloc raster structure\n");
        return NULL;
    }
    rast->nrows = nrows;
    rast->ncols = ncols;
    rast->no_data_value = no_data_value;
    rast->data = aligned_alloc(64, nrows * ncols * sizeof(double));
    if (!rast->data) {
        fprintf(stderr, "error: alloc raster data\n");
        free(rast);
        return NULL;
    }
    return rast;
}

void free_raster(Raster *rast) {
    if (rast) {
        free(rast->data);
        free(rast);
    }
}

Raster *read_raster(const char *filename) {
    GDALAllRegister();
    GDALDatasetH ds = GDALOpen(filename, GA_ReadOnly);
    if (!ds) {
        fprintf(stderr, "error: open %s\n", filename);
        return NULL;
    }
    int rows = GDALGetRasterYSize(ds);
    int cols = GDALGetRasterXSize(ds);
    double no_data = GDALGetRasterNoDataValue(GDALGetRasterBand(ds, 1), NULL);
    Raster *rast = allocate_raster(rows, cols, no_data);
    if (!rast) {
        GDALClose(ds);
        return NULL;
    }
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    if (GDALRasterIO(band, GF_Read, 0, 0, cols, rows, rast->data, cols, rows, GDT_Float64, 0, 0) != CE_None) {
        fprintf(stderr, "error: read %s\n", filename);
        free_raster(rast);
        GDALClose(ds);
        return NULL;
    }
    GDALClose(ds);
    return rast;
}

Raster *read_raster_parallel(const char *filename, int num_threads) {
    Raster *rast;
    ReadArgs args = {filename, &rast};
    pthread_t thread;
    pthread_create(&thread, NULL, read_raster_thread, &args);
    pthread_join(thread, NULL);
    return rast;
}

void write_raster(const char *filename, float *data, int nrows, int ncols, double no_data_value, const char *ref_file) {
    GDALAllRegister();
    GDALDatasetH ref = GDALOpen(ref_file, GA_ReadOnly);
    if (!ref) {
        fprintf(stderr, "error: open %s\n", ref_file);
        return;
    }
    double gt[6];
    GDALGetGeoTransform(ref, gt);
    const char *proj = GDALGetProjectionRef(ref);
    char *opts[] = {"COMPRESS=DEFLATE", "ZLEVEL=1", "TILED=YES", "BLOCKXSIZE=256", "BLOCKYSIZE=256", NULL};
    GDALDatasetH ds = GDALCreate(GDALGetDriverByName("GTiff"), filename, ncols, nrows, 1, GDT_Float32, opts);
    if (!ds) {
        fprintf(stderr, "error: create %s\n", filename);
        GDALClose(ref);
        return;
    }
    GDALSetGeoTransform(ds, gt);
    GDALSetProjection(ds, proj);
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    GDALSetRasterNoDataValue(band, (float)no_data_value);

    int num_threads = 6; // fixed at 6 for balance
    int chunk_size = nrows / num_threads;
    pthread_t threads[num_threads];
    WriteChunkArgs args[num_threads];
    for (int i = 0; i < num_threads; i++) {
        args[i].ds = ds;
        args[i].data = data;
        args[i].start_row = i * chunk_size;
        args[i].num_rows = (i == num_threads - 1) ? (nrows - i * chunk_size) : chunk_size;
        args[i].ncols = ncols;
        pthread_create(&threads[i], NULL, write_raster_chunk, &args[i]);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    GDALClose(ds);
    GDALClose(ref);
}
