#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include <stdint.h>
#include <mpi.h>

/* gdal c api */
#include "gdal.h"
#include "gdal_alg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"

/* configured paths (from config.txt) */
extern char *hysogs_data_path;
extern char *esa_data_path;
extern char *blocks_shp_path;
extern char *lookup_table_path;
extern char *log_dir;

/* mode flags */
extern bool use_list_mode;
extern char *block_ids_file;

/* parse key=value config or exit */
void parse_config(const char *conf_file);

/* per-rank log setup/teardown */
void init_logging(int rank);
void finalize_logging(void);

/* block-id sources */
int *read_block_list(const char *path, int *n_blocks);
int get_all_blocks(int **out_ids);

/* raster I/O */
uint8_t *load_raster(const char *path,
                     const double *bbox,
                     int *xsize,
                     int *ysize,
                     double *gt,
                     OGRSpatialReferenceH *srs);
void save_raster(const uint8_t *data,
                 int xsize,
                 int ysize,
                 const double *gt,
                 OGRSpatialReferenceH srs,
                 const char *path);

/* core processing */
void process_block(int block_id);

/* logging */
void init_logging(int rank);
void finalize_logging(void);

#endif /* GLOBAL_H */
