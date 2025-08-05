/* implements curve number method for processing blocks */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "global.h"

/* load lookup table from csv file */
static void
load_lookup_table(const char *hc, const char *arc, int table[256][5])
{
    FILE *f;
    char fname[PATH_MAX], line[128], msg[8192];
    char *tok, *grid_code, *us;
    int i, j, lc, sg, cnv;

    if (snprintf(fname, sizeof(fname), "%s/default_lookup_%s_%s.csv",
                 lookup_table_path, hc, arc) >= PATH_MAX) {
        snprintf(msg, sizeof(msg), "lookup table path too long: %s", fname);
        log_message("ERROR", msg, true);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    f = fopen(fname, "r");
    if (!f) {
        snprintf(msg, sizeof(msg), "cannot open lookup table %s", fname);
        log_message("ERROR", msg, true);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* initialize table with nodata value (255) */
    for (i = 0; i < 256; i++) {
        for (j = 0; j < 5; j++) {
            table[i][j] = 255;
        }
    }

    /* skip header line */
    if (!fgets(line, sizeof(line), f)) {
        snprintf(msg, sizeof(msg), "empty lookup table %s", fname);
        log_message("ERROR", msg, true);
        fclose(f);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* parse csv rows */
    while (fgets(line, sizeof(line), f)) {
        tok = strtok(line, ",");
        if (!tok) {
            continue;
        }
        grid_code = tok;
        us = strchr(grid_code, '_');
        if (!us) {
            snprintf(msg, sizeof(msg), "invalid grid_code in %s: %s", fname, grid_code);
            log_message("ERROR", msg, true);
            continue;
        }
        *us = '\0';
        lc = atoi(grid_code);
        sg = (us[1] == 'A' ? 1 : us[1] == 'B' ? 2 : us[1] == 'C' ? 3 : 4);
        tok = strtok(NULL, ",");
        if (!tok) {
            snprintf(msg, sizeof(msg), "invalid row in %s: missing cn", fname);
            log_message("ERROR", msg, true);
            continue;
        }
        cnv = atoi(tok);
        if (lc >= 0 && lc < 256 && sg >= 0 && sg < 5) {
            table[lc][sg] = cnv;
        } else {
            snprintf(msg, sizeof(msg), "invalid values in %s: lc=%d, sg=%d", fname, lc, sg);
            log_message("ERROR", msg, true);
        }
    }
    fclose(f);
}

/* adjust hysogs data based on drainage condition */
static void
modify_hysogs_data(uint8_t *h, int npix, const char *cond)
{
    int i;

    if (strcmp(cond, "drained") == 0) {
        for (i = 0; i < npix; i++) {
            if (h[i] >= 11 && h[i] <= 14) {
                h[i] = 4;
            }
        }
    } else {
        for (i = 0; i < npix; i++) {
            if (h[i] == 11) h[i] = 1;
            else if (h[i] == 12) h[i] = 2;
            else if (h[i] == 13) h[i] = 3;
            else if (h[i] == 14) h[i] = 4;
        }
    }
}

/* generate cn raster using lookup table */
static void
calculate_cn(const uint8_t *esa, const uint8_t *hsg, int npix, int table[256][5], uint8_t *out)
{
    int i, land_cover, soil_group, cn_value;

    for (i = 0; i < npix; i++) {
        land_cover = esa[i];
        soil_group = hsg[i];
        if (land_cover >= 0 && land_cover < 256 && soil_group >= 0 && soil_group < 5) {
            cn_value = table[land_cover][soil_group];
            if (cn_value < 255) {
                out[i] = (uint8_t)cn_value;
            }
        }
    }
}

/* process a single block and generate cn rasters */
void
process_block(int block_id, bool overwrite, int total_blocks)
{
    int rank, xsize, ysize, hsx, hsy, esax, esay, npix, c, hi, ai;
    OGRDataSourceH ds;
    OGRLayerH layer;
    OGRFeatureH feat;
    OGREnvelope env;
    OGRSpatialReferenceH srs, soil_srs;
    uint8_t *esa, *hysogs_coarse, *hysogs_resampled, *hysogs_adjusted, *cn;
    double bbox[4], gt[6], soil_gt[6];
    char filter[64], outdir[PATH_MAX], *outpath, msg[8192];
    const char *conds[2] = {"drained", "undrained"};
    const char *hcs[3] = {"p", "f", "g"};
    const char *arcs[3] = {"i", "ii", "iii"};
    int table[256][5];
    size_t len;
    FILE *f;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* fetch block geometry */
    ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) {
        snprintf(msg, sizeof(msg), "ogr open failed: %s", blocks_shp_path);
        log_message("ERROR", msg, true);
        return;
    }
    layer = OGR_DS_GetLayer(ds, 0);
    if (snprintf(filter, sizeof(filter), "\"ID\"=%d", block_id) >= sizeof(filter)) {
        snprintf(msg, sizeof(msg), "filter string too long for block %d", block_id);
        log_message("ERROR", msg, true);
        OGR_DS_Destroy(ds);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    OGR_L_SetAttributeFilter(layer, filter);
    feat = OGR_L_GetNextFeature(layer);
    if (!feat) {
        snprintf(msg, sizeof(msg), "block %d not found", block_id);
        log_message("ERROR", msg, true);
        OGR_DS_Destroy(ds);
        return;
    }
    OGR_G_GetEnvelope(OGR_F_GetGeometryRef(feat), &env);
    bbox[0] = env.MinX;
    bbox[1] = env.MinY;
    bbox[2] = env.MaxX;
    bbox[3] = env.MaxY;
    OGR_F_Destroy(feat);
    OGR_DS_Destroy(ds);

    /* load esa land cover raster */
    esa = load_raster(esa_data_path, bbox, &xsize, &ysize, gt, &srs);
    if (!esa) {
        snprintf(msg, sizeof(msg), "esa load failed for block %d", block_id);
        log_message("ERROR", msg, true);
        return;
    }

    /* load hysogs soil raster */
    hysogs_coarse = load_raster(hysogs_data_path, bbox, &hsx, &hsy, soil_gt, &soil_srs);
    if (!hysogs_coarse) {
        snprintf(msg, sizeof(msg), "hysogs load failed for block %d", block_id);
        log_message("ERROR", msg, true);
        free(esa);
        return;
    }

    /* upsample hysogs to match esa grid */
    esax = xsize;
    esay = ysize;
    npix = esax * esay;
    hysogs_resampled = malloc((size_t)npix);
    if (!hysogs_resampled) {
        snprintf(msg, sizeof(msg), "malloc failed for hysogs resampling, block %d", block_id);
        log_message("ERROR", msg, true);
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
            hysogs_resampled[y * esax + x] = hysogs_coarse[cj * hsx + ci];
        }
    }
    free(hysogs_coarse);

    /* process all combinations of conditions, hcs, and arcs */
    for (c = 0; c < 2; c++) {
        if (snprintf(outdir, PATH_MAX, "cn_rasters_%s", conds[c]) >= PATH_MAX) {
            snprintf(msg, sizeof(msg), "output directory path too long for %s", conds[c]);
            log_message("ERROR", msg, true);
            free(esa);
            free(hysogs_resampled);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
#ifdef _WIN32
        if (_mkdir(outdir) != 0 && errno != EEXIST) {
#else
        if (mkdir(outdir, 0755) != 0 && errno != EEXIST) {
#endif
            snprintf(msg, sizeof(msg), "failed to create output directory %s", outdir);
            log_message("ERROR", msg, true);
            free(esa);
            free(hysogs_resampled);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (hi = 0; hi < 3; hi++) {
            for (ai = 0; ai < 3; ai++) {
                /* load lookup table */
                load_lookup_table(hcs[hi], arcs[ai], table);

                /* adjust hysogs data */
                hysogs_adjusted = malloc((size_t)npix);
                if (!hysogs_adjusted) {
                    snprintf(msg, sizeof(msg), "malloc failed for hysogs adjusted, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memcpy(hysogs_adjusted, hysogs_resampled, npix);
                modify_hysogs_data(hysogs_adjusted, npix, conds[c]);

                /* generate cn raster */
                cn = malloc((size_t)npix);
                if (!cn) {
                    snprintf(msg, sizeof(msg), "malloc failed for cn raster, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memset(cn, 255, npix);
                calculate_cn(esa, hysogs_adjusted, npix, table, cn);

                /* construct output path */
                len = strlen(outdir) + strlen(hcs[hi]) + strlen(arcs[ai]) + 32;
                outpath = malloc(len);
                if (!outpath) {
                    snprintf(msg, sizeof(msg), "malloc failed for output path, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    free(cn);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                if (snprintf(outpath, len, "%s/cn_%s_%s_%d.tif", outdir, hcs[hi], arcs[ai], block_id) >= len) {
                    snprintf(msg, sizeof(msg), "output path too long for block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    free(cn);
                    free(outpath);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                if (!overwrite) {
                    f = fopen(outpath, "r");
                    if (f) {
                        fclose(f);
                        char *newpath = malloc(len + 1);
                        if (!newpath) {
                            snprintf(msg, sizeof(msg), "malloc failed for new output path, block %d", block_id);
                            log_message("ERROR", msg, true);
                            free(esa);
                            free(hysogs_resampled);
                            free(hysogs_adjusted);
                            free(cn);
                            free(outpath);
                            MPI_Abort(MPI_COMM_WORLD, 1);
                        }
                        if (snprintf(newpath, len + 1, "%s/cn_%s_%s_%d_.tif", outdir, hcs[hi], arcs[ai], block_id) >= len + 1) {
                            snprintf(msg, sizeof(msg), "new output path too long for block %d", block_id);
                            log_message("ERROR", msg, true);
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

                /* save cn raster */
                save_raster(cn, esax, esay, gt, srs, outpath);

                /* log completion */
                snprintf(msg, sizeof(msg), "completed block %d for %s/%s/%s", block_id, conds[c], hcs[hi], arcs[ai]);
                log_message("INFO", msg, false);
                report_block_completion(block_id, total_blocks);

                free(outpath);
                free(cn);
                free(hysogs_adjusted);
            }
        }
    }

    free(esa);
    free(hysogs_resampled);
}

/* report block completion to rank 0 */
void
report_block_completion(int block_id, int total_blocks)
{
    int rank, signal;
    static int blocks_completed = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0) {
        signal = block_id;
        MPI_Send(&signal, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    } else {
        blocks_completed++;
        char msg[8192];
        snprintf(msg, sizeof(msg), "progress: %d/%d blocks completed", blocks_completed / 6, total_blocks);
        log_message("INFO", msg, true);
    }
}
