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
#include <omp.h>

/* load lookup <hc>_<arc> into table, default 255 nodata */
static void load_lookup_table(const char *hc,
                              const char *arc,
                              int table[256][5])
{
    char fname[PATH_MAX];
    snprintf(fname, sizeof(fname),
             "%s/default_lookup_%s_%s.csv",
             lookup_table_path, hc, arc);
    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "cannot open lookup %s\n", fname);
        exit(1);
    }

    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 5; j++)
            table[i][j] = 255;

    char line[128];
    fgets(line, sizeof(line), f);  /* skip header */
    while (fgets(line, sizeof(line), f)) {
        char *tok = strtok(line, ",");
        if (!tok) continue;
        int lc = atoi(tok);
        char *us = strchr(tok, '_');
        char sgc = us ? us[1] : 'D';
        int sg = (sgc == 'A' ? 1 :
                  sgc == 'B' ? 2 :
                  sgc == 'C' ? 3 : 4);
        tok = strtok(NULL, ",");
        if (!tok) continue;
        int cnv = atoi(tok);
        if (lc >= 0 && lc < 256 && sg >= 0 && sg < 5)
            table[lc][sg] = cnv;
    }
    fclose(f);
}

/* adjust HYSOGs codes for drained/undrained */
static void modify_hysogs_data(uint8_t *h,
                               int npix,
                               const char *cond)
{
    if (strcmp(cond, "drained") == 0) {
	#pragma omp parallel for schedule(static)
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

/* generate CN raster via lookup */
static void calculate_cn(const uint8_t *esa,
                         const uint8_t *hsg,
                         int npix,
                         int table[256][5],
                         uint8_t *out)
{
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < npix; i++) {
        int lc = esa[i];
        int sg = hsg[i];
        if (lc >= 0 && lc < 256 && sg >= 0 && sg < 5) {
            int v = table[lc][sg];
            if (v < 255) out[i] = (uint8_t)v;
        }
    }
}

void process_block(int block_id)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* fetch block geometry */
    OGRDataSourceH ds = OGROpen(blocks_shp_path, FALSE, NULL);
    if (!ds) {
        fprintf(stderr, "ogr open failed: %s\n", blocks_shp_path);
        return;
    }
    OGRLayerH layer = OGR_DS_GetLayer(ds, 0);
    char filter[64];
    snprintf(filter, sizeof(filter), "\"ID\"=%d", block_id);
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

    /* load ESA clipped to bbox */
    int xsize, ysize;
    double gt[6];
    OGRSpatialReferenceH srs;
    uint8_t *esa = load_raster(esa_data_path, bbox,
                               &xsize, &ysize, gt, &srs);
    if (!esa) {
        fprintf(stderr, "esa load fail %d\n", block_id);
        return;
    }

    /* load coarse HYSOGs clipped to bbox */
    int hsx, hsy;
    double soil_gt[6];
    OGRSpatialReferenceH soil_srs;
    uint8_t *h0 = load_raster(hysogs_data_path, bbox,
                              &hsx, &hsy, soil_gt, &soil_srs);
    if (!h0) {
        fprintf(stderr, "hsg load fail %d\n", block_id);
        free(esa);
        return;
    }

    /* upsample HYSOGs onto ESA grid */
    int esax = xsize, esay = ysize;
    int npix = esax * esay;
    uint8_t *hsg = malloc((size_t)npix);
    if (!hsg) {
        fprintf(stderr, "malloc hsg fail %d\n", block_id);
        free(esa);
        free(h0);
        return;
    }
    
    #pragma omp parallel for schedule(static)
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
            hsg[y*esax + x] = h0[cj*hsx + ci];
        }
    }
    free(h0);

    /* replace any missing or out-of-range with HSG D (4) 
     * because HSG raster has holes for dense urban areas
     * and water bodies and they both correspond to high
     * runoff potential */
    
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < npix; i++) {
	uint8_t v = hsg[i];
	if (v < 1 || v > 4) {
	    hsg[i] = 4;
	}
    }

    /* generate CN rasters */
    const char *conds[2] = {"drained","undrained"};
    const char *hcs[3]   = {"p","f","g"};
    const char *arcs[3]  = {"i","ii","iii"};

    #pragma omp parallel for schedule(dynamic)
    for (int c = 0; c < 2; c++) {
        char outdir[PATH_MAX];
        snprintf(outdir, PATH_MAX, "cn_rasters_%s", conds[c]);
        mkdir(outdir, 0755);

        for (int hi = 0; hi < 3; hi++) {
            for (int ai = 0; ai < 3; ai++) {
                int table[256][5];
                load_lookup_table(hcs[hi], arcs[ai], table);

                uint8_t *h2 = malloc((size_t)npix);
                memcpy(h2, hsg, npix);
                modify_hysogs_data(h2, npix, conds[c]);

                uint8_t *cn = malloc((size_t)npix);
                memset(cn, 255, npix);
                calculate_cn(esa, h2, npix, table, cn);

                size_t len = strlen(outdir) + strlen(hcs[hi])
                           + strlen(arcs[ai]) + 32;
                char *outpath = malloc(len);
                snprintf(outpath, len,
                         "%s/cn_%s_%s_%d.tif",
                         outdir, hcs[hi], arcs[ai], block_id);
                save_raster(cn, esax, esay, gt, srs, outpath);

                free(outpath);
                free(cn);
                free(h2);
            }
        }
    }

    free(esa);
    free(hsg);

    if (rank == 0) {
        printf("finished block %d\n", block_id);
    }
}
