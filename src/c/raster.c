#include "global.h"
#include "gdal.h"
#include "cpl_conv.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

/* register gdal/ogr drivers once */
static void register_drivers(void) {
    static bool done = false;
    if (done) return;
    GDALAllRegister();
    OGRRegisterAll();
    done = true;
}

/* read integer IDs from text file */
int *read_block_list(const char *path, int *n_blocks) {
    register_drivers();
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int cap = 128, cnt = 0;
    int *ids = malloc(cap * sizeof(int));
    while (fscanf(f, "%d", &ids[cnt]) == 1) {
        if (++cnt == cap) {
            cap *= 2;
            ids = realloc(ids, cap * sizeof(int));
        }
    }
    fclose(f);
    *n_blocks = cnt;
    return ids;
}

/* read all IDs from shapefile attribute "ID" */
int get_all_blocks(int **out_ids) {
    register_drivers();
    OGRDataSourceH ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) return -1;
    OGRLayerH layer = OGR_DS_GetLayer(ds, 0);
    int total = OGR_L_GetFeatureCount(layer, TRUE);
    int *ids = malloc(total * sizeof(int));
    OGR_L_ResetReading(layer);
    OGRFeatureH feat;
    int i = 0;
    while ((feat = OGR_L_GetNextFeature(layer))) {
        ids[i++] = OGR_F_GetFieldAsInteger(
            feat, OGR_F_GetFieldIndex(feat, "ID"));
        OGR_F_Destroy(feat);
    }
    OGR_DS_Destroy(ds);
    *out_ids = ids;
    return i;
}

/* load and clip a window from 'path' into a byte buffer */
uint8_t *load_raster(const char *path,
                     const double *bbox,
                     int *xsize, int *ysize,
                     double *gt,
                     OGRSpatialReferenceH *srs) {
    register_drivers();
    GDALDatasetH ds = GDALOpen(path, GA_ReadOnly);
    if (!ds) {
        fprintf(stderr, "gdal open failed: %s\n", path);
        return NULL;
    }

    double t[6];
    GDALGetGeoTransform(ds, t);

    /* raw pixel offsets & size for bbox */
    int xoff   = (int)floor((bbox[0] - t[0]) / t[1]);
    int yoff   = (int)floor((bbox[3] - t[3]) / t[5]);
    int xcount = (int)ceil ((bbox[2] - bbox[0]) / t[1]);
    int ycount = (int)ceil ((bbox[1] - bbox[3]) / t[5]);

    /* clamp to dataset bounds */
    int rx = GDALGetRasterXSize(ds);
    int ry = GDALGetRasterYSize(ds);
    if (xoff < 0)       { xcount += xoff; xoff = 0; }
    if (yoff < 0)       { ycount += yoff; yoff = 0; }
    if (xoff >= rx || yoff >= ry || xcount <= 0 || ycount <= 0) {
        GDALClose(ds);
        return NULL;
    }
    if (xoff + xcount > rx) xcount = rx - xoff;
    if (yoff + ycount > ry) ycount = ry - yoff;

    /* output sizes */
    *xsize = xcount;
    *ysize = ycount;

    /* clipped geotransform */
    gt[0] = t[0] + xoff * t[1];
    gt[1] = t[1];
    gt[2] = t[2];
    gt[3] = t[3] + yoff * t[5];
    gt[4] = t[4];
    gt[5] = t[5];

    /* spatial reference */
    const char *wkt = GDALGetProjectionRef(ds);
    *srs = OSRNewSpatialReference(wkt);

    size_t np = (size_t)xcount * ycount;
    uint8_t *buf = malloc(np);
    if (!buf) {
        fprintf(stderr, "out of memory: %s\n", path);
        GDALClose(ds);
        return NULL;
    }

    CPLErr err = GDALRasterIO(
        GDALGetRasterBand(ds,1),
        GF_Read,
        xoff, yoff, xcount, ycount,
        buf, xcount, ycount,
        GDT_Byte, 0, 0
    );
    GDALClose(ds);
    if (err != CE_None) {
        fprintf(stderr, "GDALRasterIO error %d on %s\n", err, path);
        free(buf);
        return NULL;
    }

    return buf;
}

/* save buffer as deflate-tiled GeoTIFF */
void save_raster(const uint8_t *data,
                 int xsize,
                 int ysize,
                 const double *gt,
                 OGRSpatialReferenceH srs,
                 const char *path)
{
    register_drivers();
    GDALDriverH drv = GDALGetDriverByName("GTiff");
    char **opts = NULL;
    opts = CSLSetNameValue(opts, "COMPRESS", "DEFLATE");
    opts = CSLSetNameValue(opts, "TILED",    "YES");

    GDALDatasetH ds = GDALCreate(
        drv, path, xsize, ysize, 1, GDT_Byte, opts);
    GDALSetGeoTransform(ds, (double *)gt);

    char *wkt = NULL;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(ds, wkt);
    CPLFree(wkt);

    CPLErr err = GDALRasterIO(
        GDALGetRasterBand(ds,1),
        GF_Write,
        0, 0, xsize, ysize,
        (void *)data, xsize, ysize,
        GDT_Byte, 0, 0
    );
    if (err != CE_None) {
        fprintf(stderr, "write error %d on %s\n", err, path);
    }

    GDALClose(ds);
    CSLDestroy(opts);
}
