/* logging functions */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#endif

static FILE *log_fp = NULL;
static int current_rank = 0;

void init_logging(int rank) {
    current_rank = rank;

    /* create logs directory */
#ifdef _WIN32
    if (_mkdir(log_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
#endif
        fprintf(stderr, "[Rank %d] Failed to create log directory %s\n", rank, log_dir);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* open a single log file per rank */
    char log_file[PATH_MAX];
    if (snprintf(log_file, PATH_MAX, "%s/rank_%d.log", log_dir, rank) >= PATH_MAX) {
        fprintf(stderr, "[Rank %d] Log file path too long\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    log_fp = fopen(log_file, "w");
    if (!log_fp) {
        fprintf(stderr, "[Rank %d] Failed to open log file %s\n", rank, log_file);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* redirect stdout and stderr to the log file */
    int fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) {
        dup2(fd, FILENO_STDOUT);
        dup2(fd, FILENO_STDERR);
        close(fd);
    } else {
        fprintf(stderr, "[Rank %d] Failed to redirect output to %s\n", rank, log_file);
        fclose(log_fp);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

void log_message(const char *level, const char *message, bool print_to_screen) {
    /* print stdout in console */
    if (print_to_screen) {
	fprintf(stderr, "[Rank %d] %s\n", current_rank, message);
    }

    if (!log_fp) return; /* after screen output */

    /* get timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S", tm);

    /* write to log file */
    fprintf(log_fp, "[%s] [Rank %d] [%s] %s\n", timestamp, current_rank, level, message);
    fflush(log_fp);

    /* optionally print to screen (using stderr to ensure visibility) */
    if (print_to_screen) {
        fprintf(stderr, "[Rank %d] %s\n", current_rank, message);
    }
}

void finalize_logging(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}
