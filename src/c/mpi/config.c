#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

/* configured paths (from config.txt) */
char *hysogs_data_path   = NULL;
char *esa_data_path      = NULL;
char *blocks_shp_path    = NULL;
char *lookup_table_path  = NULL;
char *log_dir            = NULL;

/* mode flags */
bool use_list_mode       = false;
char *block_ids_file     = NULL;

/* trim leading/trailing whitespace */
static char *trim_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

/*
 * Parse key=value config file. Expects lines in the format:
 * key = value
 * where key is one of: hysogs_data_path, esa_data_path, blocks_shp_path,
 * lookup_table_path, log_dir. Lines starting with # are ignored.
 * Whitespace around = is allowed. All keys are required.
 */
void parse_config(const char *conf_file) {
    FILE *f = fopen(conf_file, "r");
    if (!f) {
        fprintf(stderr, "cannot open config '%s'\n", conf_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim_ws(line);
        if (!*p || *p == '#') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_ws(p);
        char *val = trim_ws(eq + 1);

        if (strcmp(key, "hysogs_data_path") == 0) {
            hysogs_data_path = strdup(val);
            if (!hysogs_data_path) {
                fprintf(stderr, "malloc failed for hysogs_data_path\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        else if (strcmp(key, "esa_data_path") == 0) {
            esa_data_path = strdup(val);
            if (!esa_data_path) {
                fprintf(stderr, "malloc failed for esa_data_path\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        else if (strcmp(key, "blocks_shp_path") == 0) {
            blocks_shp_path = strdup(val);
            if (!blocks_shp_path) {
                fprintf(stderr, "malloc failed for blocks_shp_path\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        else if (strcmp(key, "lookup_table_path") == 0) {
            lookup_table_path = strdup(val);
            if (!lookup_table_path) {
                fprintf(stderr, "malloc failed for lookup_table_path\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
        else if (strcmp(key, "log_dir") == 0) {
            log_dir = strdup(val);
            if (!log_dir) {
                fprintf(stderr, "malloc failed for log_dir\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }
    fclose(f);

    if (!hysogs_data_path || !esa_data_path ||
        !blocks_shp_path || !lookup_table_path ||
        !log_dir) {
        fprintf(stderr,
            "missing one of: hysogs_data_path, esa_data_path,\n"
            "blocks_shp_path, lookup_table_path, log_dir\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

/* free all dynamically allocated config strings */
void free_config(void) {
    free(hysogs_data_path);
    free(esa_data_path);
    free(blocks_shp_path);
    free(lookup_table_path);
    free(log_dir);
    hysogs_data_path = NULL;
    esa_data_path = NULL;
    blocks_shp_path = NULL;
    lookup_table_path = NULL;
    log_dir = NULL;
    block_ids_file = NULL;
}
