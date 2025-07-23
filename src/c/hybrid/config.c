#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

/* parse simple key=value (allows spaces around '=') */
void parse_config(const char *conf_file) {
    FILE *f = fopen(conf_file, "r");
    if (!f) {
        fprintf(stderr, "cannot open config '%s'\n", conf_file);
        exit(1);
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
        }
        else if (strcmp(key, "esa_data_path") == 0) {
            esa_data_path = strdup(val);
        }
        else if (strcmp(key, "blocks_shp_path") == 0) {
            blocks_shp_path = strdup(val);
        }
        else if (strcmp(key, "lookup_table_path") == 0) {
            lookup_table_path = strdup(val);
        }
        else if (strcmp(key, "log_dir") == 0) {
            log_dir = strdup(val);
        }
    }
    fclose(f);

    if (!hysogs_data_path || !esa_data_path ||
        !blocks_shp_path || !lookup_table_path ||
        !log_dir) {
        fprintf(stderr,
            "missing one of: hysogs_data_path, esa_data_path,\n"
            "blocks_shp_path, lookup_table_path, log_dir\n");
        exit(1);
    }
}

