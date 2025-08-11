#ifndef GLOBAL_H
#define GLOBAL_H

/* os portability macros and includes for cross-platform compilation */
#include <stdbool.h>
#include <stdint.h>
#include <mpi.h>
#include <gdal.h>
#include <gdal_alg.h>
#include <cpl_conv.h>
#include <cpl_string.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#define mkdir(path, mode) _mkdir(path)
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#define FILENO_STDOUT _fileno(stdout)
#define FILENO_STDERR _fileno(stderr)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#define FILENO_STDOUT STDOUT_FILENO
#define FILENO_STDERR STDERR_FILENO
#endif

/* configured paths from config file */
extern char *hysogs_data_path;
extern char *esa_data_path;
extern char *blocks_shp_path;
extern char *lookup_table_path;
extern char *log_dir;

/* mode flags */
extern bool use_list_mode;
extern char *block_ids_file;

/* function prototypes */
void parse_config(const char *);
void free_config(void);
void init_logging(int);
void log_message(const char *, const char *, bool);
void finalize_logging(void);
int *read_block_list(const char *, int *);
int *get_all_blocks(int *);
uint8_t *load_raster(const char *, const double *, int *, int *, double *,
                     OGRSpatialReferenceH *);
void save_raster(const uint8_t *, int, int, const double *,
                 OGRSpatialReferenceH, const char *);
void process_block(int, bool, int);
void report_block_completion(int, int);

#endif /* GLOBAL_H */
