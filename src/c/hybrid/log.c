/* logging functions */
#include "global.h"

static FILE *log_fp = NULL;

void init_logging(int rank) {
    /* create logs directory */
    mkdir("logs", 0755);

    /* open log file for rank */
    char log_file[MAX_PATH];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(log_file, MAX_PATH, "logs/parallel_cn_%04d%02d%02d_%02d%02d_rank%d.log",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, rank);
    log_fp = fopen(log_file, "w");
    if (!log_fp) {
        fprintf(stderr, "failed to open log file %s\n", log_file);
    }
}

void log_message(const char *level, const char *message) {
    if (!log_fp) return;

    /* get timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S", tm);

    /* write log */
    fprintf(log_fp, "[%s] [Rank %d] [%s] %s\n", timestamp, 0, level, message);
    fflush(log_fp);
}

void close_logging(void) {
    if (log_fp) fclose(log_fp);
}
