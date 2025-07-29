/* Implements process_block(), which
 * applies the Curve Number method to
 * each block's HSG and landcover data
 * and writes out the resulting CN raster */

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

/* load lookup table from CSV file, default 255 for nodata */
static void load_lookup_table(const char *hc,
                              const char *arc,
                              int table[256][5])
{
    char fname[PATH_MAX];
    if (snprintf(fname, sizeof(fname), "%s/default_lookup_%s_%s.csv",
                 lookup_table_path, hc, arc) >= PATH_MAX) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Lookup table path too long: %s", fname);
        log_message("ERROR", msg, true);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    FILE *f = fopen(fname, "r");
    if (!f) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Cannot open lookup table %s", fname);
        log_message("ERROR", msg, true);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* initialize table with nodata value (255) */
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 5; j++)
            table[i][j] = 255;

    /* skip header line */
    char line[128];
    if (!fgets(line, sizeof(line), f)) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Empty lookup table %s", fname);
        log_message("ERROR", msg, true);
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
            char msg[8192];
            snprintf(msg, sizeof(msg), "Invalid grid_code in %s: %s", fname, grid_code);
            log_message("ERROR", msg, true);
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
            char msg[8192];
            snprintf(msg, sizeof(msg), "Invalid row in %s: missing cn", fname);
            log_message("ERROR", msg, true);
            continue;
        }
        int cnv = atoi(tok);
        if (lc >= 0 && lc < 256 && sg >= 0 && sg < 5) {
            table[lc][sg] = cnv;
        } else {
            char msg[8192];
            snprintf(msg, sizeof(msg), "Invalid values in %s: lc=%d, sg=%d", fname, lc, sg);
            log_message("ERROR", msg, true);
        }
    }
    fclose(f);
}

/* Handle dual HSG's, adjust HYSOGs codes
 * based on drained or undrained condition */
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

/* generate CN raster by applying
 * lookup table to ESA and HYSOGs data */
static void calculate_cn(const uint8_t *esa,
                         const uint8_t *hsg,
                         int npix,
                         int table[256][5],
                         uint8_t *out)
{
    /* combine ESA land cover and HYSOGs
     * soil group to assign CN values */
    for (int i = 0; i < npix; i++) {
        int land_cover = esa[i];  /* ESA land cover class */
        int soil_group = hsg[i];  /* HYSOGs soil group */
        if (land_cover >= 0 && land_cover < 256 && soil_group >= 0 && soil_group < 5) {
            int cn_value = table[land_cover][soil_group];  /* lookup CN value */
            if (cn_value < 255) out[i] = (uint8_t)cn_value;  /* set valid CN, else keep nodata (255) */
        }
    }
}

void process_block(int block_id, bool overwrite, int total_blocks)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* fetch block geometry from shapefile */
    OGRDataSourceH ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "OGR open failed: %s", blocks_shp_path);
        log_message("ERROR", msg, true);
        return;
    }
    OGRLayerH layer = OGR_DS_GetLayer(ds, 0);
    char filter[64];
    if (snprintf(filter, sizeof(filter), "\"ID\"=%d", block_id) >= sizeof(filter)) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Filter string too long for block %d", block_id);
        log_message("ERROR", msg, true);
        OGR_DS_Destroy(ds);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    OGR_L_SetAttributeFilter(layer, filter);
    OGRFeatureH feat = OGR_L_GetNextFeature(layer);
    if (!feat) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Block %d not found", block_id);
        log_message("ERROR", msg, true);
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
        char msg[8192];
        snprintf(msg, sizeof(msg), "ESA load failed for block %d", block_id);
        log_message("ERROR", msg, true);
        return;
    }

    /* load coarse HYSOGs soil raster clipped to block bounding box */
    int hsx, hsy;
    double soil_gt[6];
    OGRSpatialReferenceH soil_srs;
    uint8_t *hysogs_coarse = load_raster(hysogs_data_path, bbox,
                                         &hsx, &hsy, soil_gt, &soil_srs);
    if (!hysogs_coarse) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "HYSOGs load failed for block %d", block_id);
        log_message("ERROR", msg, true);
        free(esa);
        return;
    }

    /* upsample HYSOGs to match ESA grid
     * using nearest-neighbor interpolation */
    int esax = xsize, esay = ysize;
    int npix = esax * esay;
    uint8_t *hysogs_resampled = malloc((size_t)npix);
    if (!hysogs_resampled) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Malloc failed for HYSOGs resampling, block %d", block_id);
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
            hysogs_resampled[y*esax + x] = hysogs_coarse[cj*hsx + ci];
        }
    }
    free(hysogs_coarse);

    /* generate CN rasters for all combinations of 
     * drainage conditions, hcs, and arcs */
    const char *conds[2] = {"drained","undrained"};
    const char *hcs[3]   = {"p","f","g"};
    const char *arcs[3]  = {"i","ii","iii"};

    for (int c = 0; c < 2; c++) {
        char outdir[PATH_MAX];
        if (snprintf(outdir, PATH_MAX, "cn_rasters_%s", conds[c]) >= PATH_MAX) {
            char msg[8192];
            snprintf(msg, sizeof(msg), "Output directory path too long for %s", conds[c]);
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
            char msg[8192];
            snprintf(msg, sizeof(msg), "Failed to create output directory %s", outdir);
            log_message("ERROR", msg, true);
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
                    char msg[8192];
                    snprintf(msg, sizeof(msg), "Malloc failed for HYSOGs adjusted, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memcpy(hysogs_adjusted, hysogs_resampled, npix);
                modify_hysogs_data(hysogs_adjusted, npix, conds[c]);

                /* generate CN raster */
                uint8_t *cn = malloc((size_t)npix);
                if (!cn) {
                    char msg[8192];
                    snprintf(msg, sizeof(msg), "Malloc failed for CN raster, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                memset(cn, 255, npix);
                calculate_cn(esa, hysogs_adjusted, npix, table, cn);

                /* construct output path, appending underscore
		 * if file exists and not overwriting */
                size_t len = strlen(outdir) + strlen(hcs[hi]) + strlen(arcs[ai]) + 32;
                char *outpath = malloc(len);
                if (!outpath) {
                    char msg[8192];
                    snprintf(msg, sizeof(msg), "Malloc failed for output path, block %d", block_id);
                    log_message("ERROR", msg, true);
                    free(esa);
                    free(hysogs_resampled);
                    free(hysogs_adjusted);
                    free(cn);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                if (snprintf(outpath, len, "%s/cn_%s_%s_%d.tif", outdir, hcs[hi], arcs[ai], block_id) >= len) {
                    char msg[8192];
                    snprintf(msg, sizeof(msg), "Output path too long for block %d", block_id);
                    log_message("ERROR", msg, true);
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
                            char msg[8192];
                            snprintf(msg, sizeof(msg), "Malloc failed for new output path, block %d", block_id);
                            log_message("ERROR", msg, true);
                            free(esa);
                            free(hysogs_resampled);
                            free(hysogs_adjusted);
                            free(cn);
                            free(outpath);
                            MPI_Abort(MPI_COMM_WORLD, 1);
                        }
                        if (snprintf(newpath, len + 1, "%s/cn_%s_%s_%d_.tif", outdir, hcs[hi], arcs[ai], block_id) >= len + 1) {
                            char msg[8192];
                            snprintf(msg, sizeof(msg), "New output path too long for block %d", block_id);
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

                /* save CN raster as GeoTIFF */
                save_raster(cn, esax, esay, gt, srs, outpath);

                /* log completion and notify
		 * rank 0 for progress tracking */
                char msg[8192];
                snprintf(msg, sizeof(msg), "Completed block %d for %s/%s/%s", block_id, conds[c], hcs[hi], arcs[ai]);
                log_message("INFO", msg, false); /* log to file only */
                report_block_completion(block_id, total_blocks); /* notify rank 0 */

                free(outpath);
                free(cn);
                free(hysogs_adjusted);
            }
        }
    }

    free(esa);
    free(hysogs_resampled);
}

/* report block completion to
 * rank 0 for progress tracking */
void report_block_completion(int block_id, int total_blocks)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    /* send completion signal to rank 0 */
    if (rank != 0) {
        int signal = block_id;
        MPI_Send(&signal, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    } else {
        /* rank 0 tracks progress */
        static int blocks_completed = 0;
        blocks_completed++;
        char msg[8192];
        snprintf(msg, sizeof(msg), "Progress: %d/%d blocks completed", blocks_completed / 6, total_blocks);
        log_message("INFO", msg, true); /* print to screen and log */
    }
}
