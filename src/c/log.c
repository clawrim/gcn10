/* logging functions */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static FILE *log_fp = NULL;
static int current_rank = 0;

void init_logging(int rank) {
    current_rank = rank;

    /* create logs directory */
    if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "failed to create log directory %s\n", log_dir);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* open log file for rank */
    char log_file[PATH_MAX];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (snprintf(log_file, PATH_MAX, "%s/parallel_cn_%04d%02d%02d_%02d%02d_rank%d.log",
                 log_dir, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, rank) >= PATH_MAX) {
        fprintf(stderr, "log file path too long\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    log_fp = fopen(log_file, "w");
    if (!log_fp) {
        fprintf(stderr, "failed to open log file %s\n", log_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* redirect stdout and stderr to per-rank files */
    char outpath[PATH_MAX], errpath[PATH_MAX];
    if (snprintf(outpath, PATH_MAX, "%s/rank_%d.out", log_dir, rank) >= PATH_MAX ||
        snprintf(errpath, PATH_MAX, "%s/rank_%d.err", log_dir, rank) >= PATH_MAX) {
        fprintf(stderr, "output path too long for rank %d\n", rank);
        fclose(log_fp);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int fd;
    if ((fd = open(outpath, O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
        dup2(fd, STDOUT_FILENO);
        close(fd);
    } else {
        fprintf(stderr, "failed to open stdout file %s\n", outpath);
        fclose(log_fp);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if ((fd = open(errpath, O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
        dup2(fd, STDERR_FILENO);
        close(fd);
    } else {
        fprintf(stderr, "failed to open stderr file %s\n", errpath);
        fclose(log_fp);
        MPI_Abort(MPI_COMM_WORLD, 1);
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
    fprintf(log_fp, "[%s] [Rank %d] [%s] %s\n", timestamp, current_rank, level, message);
    fflush(log_fp);
}

void finalize_logging(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
    fflush(stdout);
    fflush(stderr);
}
