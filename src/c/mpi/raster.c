/* spatial i/o functions for reading/writing/clipping rasters and vectors */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "global.h"

static bool drivers_registered = false;

/* register gdal/ogr drivers */
static void
register_drivers(void)
{
    if (drivers_registered) {
        return;
    }
    GDALAllRegister();
    OGRRegisterAll();
    drivers_registered = true;
}

/* read integer ids from text file */
int *
read_block_list(const char *path, int *n_blocks)
{
    FILE *f;
    int *ids, cap, cnt;
    char msg[512];

    register_drivers();
    f = fopen(path, "r");
    if (!f) {
        snprintf(msg, sizeof(msg), "cannot open block list file %s", path);
        log_message("ERROR", msg, true);
        return NULL;
    }

    cap = 128;
    cnt = 0;
    ids = malloc(cap * sizeof(int));
    if (!ids) {
        snprintf(msg, sizeof(msg), "malloc failed for block ids");
        log_message("ERROR", msg, true);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    while (fscanf(f, "%d", &ids[cnt]) == 1) {
        cnt++;
        if (cnt == cap) {
            int *new_ids;
            cap *= 2;
            new_ids = realloc(ids, cap * sizeof(int));
            if (!new_ids) {
                snprintf(msg, sizeof(msg), "realloc failed for block ids");
                log_message("ERROR", msg, true);
                free(ids);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            ids = new_ids;
        }
    }
    fclose(f);
    *n_blocks = cnt;
    return ids;
}

/* read all ids from shapefile attribute "id" */
int
get_all_blocks(int **out_ids)
{
    OGRDataSourceH ds;
    OGRLayerH layer;
    OGRFeatureH feat;
    int *ids, total, i;
    char msg[512];

    register_drivers();
    ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) {
        snprintf(msg, sizeof(msg), "ogr open failed: %s", blocks_shp_path);
        log_message("ERROR", msg, true);
        return -1;
    }

    layer = OGR_DS_GetLayer(ds, 0);
    total = OGR_L_GetFeatureCount(layer, TRUE);
    ids = malloc(total * sizeof(int));
    if (!ids) {
        snprintf(msg, sizeof(msg), "malloc failed for shapefile ids");
        log_message("ERROR", msg, true);
        OGR_DS_Destroy(ds);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    OGR_L_ResetReading(layer);
    i = 0;
    while ((feat = OGR_L_GetNextFeature(layer))) {
        ids[i++] = OGR_F_GetFieldAsInteger(feat, OGR_F_GetFieldIndex(feat, "ID"));
        OGR_F_Destroy(feat);
    }
    OGR_DS_Destroy(ds);
    *out_ids = ids;
    return i;
}

/* load and clip raster window into byte buffer */
uint8_t *
load_raster(const char *path, const double *bbox, int *xsize, int *ysize, double *gt, OGRSpatialReferenceH *srs)
{
    GDALDatasetH ds;
    double t[6];
    int xoff, yoff, xcount, ycount, rx, ry;
    uint8_t *buf;
    size_t pixel_count;
    const char *wkt;
    CPLErr err;
    char msg[512];

    register_drivers();
    ds = GDALOpen(path, GA_ReadOnly);
    if (!ds) {
        snprintf(msg, sizeof(msg), "gdal open failed: %s", path);
        log_message("ERROR", msg, true);
        return NULL;
    }

    GDALGetGeoTransform(ds, t);
    xoff = (int)floor((bbox[0] - t[0]) / t[1]);
    yoff = (int)floor((bbox[3] - t[3]) / t[5]);
    xcount = (int)ceil((bbox[2] - bbox[0]) / t[1]);
    ycount = (int)ceil((bbox[1] - bbox[3]) / t[5]);

    rx = GDALGetRasterXSize(ds);
    ry = GDALGetRasterYSize(ds);
    if (xoff < 0) {
        xcount += xoff;
        xoff = 0;
    }
    if (yoff < 0) {
        ycount += yoff;
        yoff = 0;
    }
    if (xoff >= rx || yoff >= ry || xcount <= 0 || ycount <= 0) {
        snprintf(msg, sizeof(msg), "invalid raster bounds for %s", path);
        log_message("ERROR", msg, true);
        GDALClose(ds);
        return NULL;
    }
    if (xoff + xcount > rx) {
        xcount = rx - xoff;
    }
    if (yoff + ycount > ry) {
        ycount = ry - yoff;
    }

    *xsize = xcount;
    *ysize = ycount;
    gt[0] = t[0] + xoff * t[1];
    gt[1] = t[1];
    gt[2] = t[2];
    gt[3] = t[3] + yoff * t[5];
    gt[4] = t[4];
    gt[5] = t[5];

    wkt = GDALGetProjectionRef(ds);
    *srs = OSRNewSpatialReference(wkt);

    pixel_count = (size_t)xcount * ycount;
    buf = malloc(pixel_count);
    if (!buf) {
        snprintf(msg, sizeof(msg), "out of memory for raster %s", path);
        log_message("ERROR", msg, true);
        GDALClose(ds);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    err = GDALRasterIO(GDALGetRasterBand(ds, 1), GF_Read,
                       xoff, yoff, xcount, ycount,
                       buf, xcount, ycount, GDT_Byte, 0, 0);
    GDALClose(ds);
    if (err != CE_None) {
        snprintf(msg, sizeof(msg), "gdalrasterio error %d on %s", err, path);
        log_message("ERROR", msg, true);
        free(buf);
        return NULL;
    }

    return buf;
}

/* save buffer as deflate-tiled geotiff */
void
save_raster(const uint8_t *data, int xsize, int ysize, const double *gt, OGRSpatialReferenceH srs, const char *path)
{
    GDALDriverH drv;
    GDALDatasetH ds;
    char **opts;
    char *wkt;
    CPLErr err;
    char msg[512];

    register_drivers();
    drv = GDALGetDriverByName("GTiff");
    opts = NULL;
    opts = CSLSetNameValue(opts, "COMPRESS", "DEFLATE");
    opts = CSLSetNameValue(opts, "TILED", "YES");

    ds = GDALCreate(drv, path, xsize, ysize, 1, GDT_Byte, opts);
    GDALSetGeoTransform(ds, (double *)gt);

    wkt = NULL;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);

    err = GDALRasterIO(GDALGetRasterBand(ds, 1), GF_Write,
                       0, 0, xsize, ysize,
                       (void *)data, xsize, ysize, GDT_Byte, 0, 0);
    if (err != CE_None) {
        snprintf(msg, sizeof(msg), "write error %d on %s", err, path);
        log_message("ERROR", msg, true);
    }

    GDALClose(ds);
    CSLDestroy(opts);
}
