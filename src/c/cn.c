#include "global.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <mpi.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <gdal.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

/* load lookup table from CSV file, default 255 for nodata */
static void load_lookup_table(const char *hc,
                              const char *arc,
                              int table[256][5])
{
    char fname[PATH_MAX];
    if (snprintf(fname, sizeof(fname), "%s/default_lookup_%s_%s.csv",
                 lookup_table_path, hc, arc) >= PATH_MAX) {
        fprintf(stderr, "lookup table path too long\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "cannot open lookup %s\n", fname);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* initialize table with nodata value (255) */
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 5; j++)
            table[i][j] = 255;

    /* skip header line */
    char line[128];
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "empty lookup table %s\n", fname);
        fclose(f);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* parse CSV rows: grid_code (e.g., 10_A),cn */
    while (fgets(line, sizeof(line), f)) {
        char *tok = strtok(line, ",");
        if (!tok) continue;
        char *grid_code = tok;
        char *us = strchr(grid_code, '_');
        if (!us) {
            fprintf(stderr, "invalid grid_code in %s: %s\n", fname, grid_code);
            continue;
        }
        *us = '\0';  /* split grid_code into lc and sgc */
        int lc = atoi(grid_code);
        char sgc = us[1];
        int sg = (sgc == 'A' ? 1 :
                  sgc == 'B' ? 2 :
                  sgc == 'C' ? 3 : 4);
        tok = strtok(NULL, ",");
        if (!tok) {
            fprintf(stderr, "invalid row in %s: missing cn\n", fname);
            continue;
        }
        int cnv = atoi(tok);
        if (lc >= 0 && lc < 256 && sg >= 0 && sg < 5) {
            table[lc][sg] = cnv;
        } else {
            fprintf(stderr, "invalid values in %s: lc=%d, sg=%d\n", fname, lc, sg);
        }
    }
    fclose(f);
}

/* adjust HYSOGs codes based on drained or undrained condition */
static void modify_hysogs_data(uint8_t *h,
                               int npix,
                               const char *cond)
{
    if (strcmp(cond, "drained") == 0) {
        for (int i = 0; i < npix; i++)
            if (h[i] >= 11 && h[i] <= 14)
                h[i] = 4;
    } else {
        for (int i = 0; i < npix; i++) {
            if (h[i] == 11) h[i] = 1;
            else if (h[i] == 12) h[i] = 2;
            else if (h[i] == 13) h[i] = 3;
            else if (h[i] == 14) h[i] = 4;
        }
    }
}

/* generate CN raster by applying lookup table to ESA and HYSOGs data */
static void calculate_cn(const uint8_t *esa,
                         const uint8_t *hsg,
                         int npix,
                         int table[256][5],
                         uint8_t *out)
{
    /* combine ESA land cover and HYSOGs soil group to assign CN values */
    for (int i = 0; i < npix; i++) {
        int land_cover = esa[i];  /* ESA land cover class */
        int soil_group = hsg[i];  /* HYSOGs soil group */
        if (land_cover >= 0 && land_cover < 256 && soil_group >= 0 && soil_group < 5) {
            int cn_value = table[land_cover][soil_group];  /* lookup CN value */
            if (cn_value < 255) out[i] = (uint8_t)cn_value;  /* set valid CN, else keep nodata (255) */
        }
    }
}

void process_block(int block_id, bool overwrite)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* fetch block geometry from shapefile */
    OGRDataSourceH ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) {
        fprintf(stderr, "ogr open failed: %s\n", blocks_shp_path);
        return;
    }
    OGRLayerH layer = OGR_DS_GetLayer(ds, 0);
    char filter[64];
    if (snprintf(filter, sizeof(filter), "\"ID\"=%d", block_id) >= sizeof(filter)) {
        fprintf(stderr, "filter string too long for block %d\n", block_id);
        OGR_DS_Destroy(ds);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    OGR_L_SetAttributeFilter(layer, filter);
    OGRFeatureH feat = OGR_L_GetNextFeature(layer);
    if (!feat) {
        fprintf(stderr, "block %d not found\n", block_id);
        OGR_DS_Destroy(ds);
        return;
    }
    OGREnvelope env;
    OGR_G_GetEnvelope(OGR_F_GetGeometryRef(feat), &env);
    double bbox[4] = { env.MinX, env.MinY, env.MaxX, env.MaxY };
    OGR_F_Destroy(feat);
    OGR_DS_Destroy(ds);

    /* load ESA land cover raster clipped to block bounding box */
    int xsize, ysize;
    double gt[6];
    OGRSpatialReferenceH srs;
    uint8_t *esa = load_raster(esa_data_path, bbox,
                               &xsize, &ysize, gt, &srs);
    if (!esa) {
        fprintf(stderr, "esa load fail %d\n", block_id);
        return;
    }

    /* load coarse HYSOGs soil raster clipped to block bounding box */
    int hsx, hsy;
    double soil_gt[6];
    OGRSpatialReferenceH soil_srs;
    uint8_t *hysogs_coarse = load_raster(hysogs_data_path, bbox,
                                         &hsx, &hsy, soil_gt, &soil_srs);
    if (!hysogs_coarse) {
        fprintf(stderr, "hsg load fail %d\n", block_id);
        free(esa);
        return;
    }

    /* upsample HYSOGs to match ESA grid using nearest-neighbor interpolation */
    int esax = xsize, esay = ysize;
    int npix = esax * esay;
    uint8_t *hysogs_resampled = malloc((size_t)npix);
    if (!hysogs_resampled) {
        fprintf(stderr, "malloc hsg fail %d\n", block_id);
        free(esa);
        free(hysogs_coarse);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for (int y = 0; y < esay; y++) {
        double py = gt[3] + (y + 0.5) * gt[5];
        for (int x = 0; x < esax; x++) {
            double px = gt[0] + (x + 0.5) * gt[1];
            double dc = (px - soil_gt[0]) / soil_gt[1];
            double dr = (soil_gt[3] - py) / fabs(soil_gt[5]);
            int ci = (int)round(dc);
            int cj = (int)round(dr);
            ci = ci < 0 ? 0 : (ci >= hsx ? hsx - 1 : ci);
            cj = cj < 0 ? 0 : (cj >= hsy ? hsy - 1 : cj);
            hysogs_resampled[y*esax + x] = hysogs_coarse[cj*hsx + ci];
        }
    }
    free(hysogs_coarse);

    /* generate CN rasters for all combinations of conditions, hc, and arc */
    const char *conds[2] = {"drained","undrained"};
    const char *hcs[3]   = {"p","f","g"};
    const char *arcs[3]  = {"i","ii","iii"};

    for (int c = 0; c < 2; c++) {
        char outdir[PATH_MAX];
        if (snprintf(outdir, PATH_MAX, "cn_rasters_%s", conds[c]) >= PATH_MAX) {
            fprintf(stderr, "output directory path too long for %s\n", conds[c]);
            free(esa);
            free(hysogs_resampled);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
#ifdef _WIN32
        if (_mkdir(outdir) != 0 && errno != EEXIST) {
#else
        if (mkdir(outdir, 0755) != 0 && errno != EEXIST) {
#endif
            fprintf(stderr, "failed to create output directory %s\n", outdir);
            free(esa);
            free(hysogs_resampled);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (int hi = 0; hi < 3; hi++) {
            for (int ai = 0; ai < 3; ai++) {
                /* load lookup table for this hc/arc combination */
                int table[256][5];
                load_lookup_table(hcs[hi], arcs[ai], table);

                /* adjust HYSOGs data for drained/undrained condition */
                uint8_t *hysogs_adjusted = malloc((size_t)npix);
                if (!hysogs_adjusted) {
                    fprintf(stderr, "malloc hysogs_adjusted fail %d\n", block_id);
                    free(esa);
                    free(hysogs_resampled);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memcpy(hysogs_adjusted, hysogs_resampled, npix);
                modify_hysogs_data(hysogs_adjusted, npix, conds[c]);

                /* generate CN raster */
                uint8_t *cn = malloc((size_t)npix);
                if (!cn) {
                    fprintf(stderr, "malloc cn fail %d\n", block_id);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memset(cn, 255, npix);
                calculate_cn(esa, hysogs_adjusted, npix, table, cn);

                /* construct output path, appending underscore if file exists and not overwriting */
                size_t len = strlen(outdir) + strlen(hcs[hi]) + strlen(arcs[ai]) + 32;
                char *outpath = malloc(len);
                if (!outpath) {
                    fprintf(stderr, "malloc outpath fail %d\n", block_id);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    free(cn);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                if (snprintf(outpath, len, "%s/cn_%s_%s_%d.tif", outdir, hcs[hi], arcs[ai], block_id) >= len) {
                    fprintf(stderr, "output path too long for block %d\n", block_id);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    free(cn);
                    free(outpath);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                if (!overwrite) {
                    FILE *f = fopen(outpath, "r");
                    if (f) {
                        fclose(f);
                        char *newpath = malloc(len + 1);
                        if (!newpath) {
                            fprintf(stderr, "malloc newpath fail %d\n", block_id);
                            free(esa);
                            free(hysogs_resampled);
                            free(hysogs_adjusted);
                            free(cn);
                            free(outpath);
                            MPI_Abort(MPI_COMM_WORLD, 1);
                        }
                        if (snprintf(newpath, len + 1, "%s/cn_%s_%s_%d_.tif", outdir, hcs[hi], arcs[ai], block_id) >= len + 1) {
                            fprintf(stderr, "new output path too long for block %d\n", block_id);
                            free(esa);
                            free(hysogs_resampled);
                            free(hysogs_adjusted);
                            free(cn);
                            free(outpath);
                            free(newpath);
                            MPI_Abort(MPI_COMM_WORLD, 1);
                        }
                        free(outpath);
                        outpath = newpath;
                    }
                }

                /* save CN raster as GeoTIFF */
                save_raster(cn, esax, esay, gt, srs, outpath);

                free(outpath);
                free(cn);
                free(hysogs_adjusted);
            }
        }
    }

    free(esa);
    free(hysogs_resampled);

    if (rank == 0) {
        printf("finished block %d\n", block_id);
    }
}
