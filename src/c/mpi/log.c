/* logging functions for per-rank log files and console output */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "global.h"

static FILE *log_fp = NULL;
static int current_rank = 0;

/* initialize logging for each mpi rank */
void init_logging(int rank)
{
	char log_file[PATH_MAX];
	int fd;

	current_rank = rank;

	/* create logs directory */
#ifdef _WIN32
	if (_mkdir(log_dir) != 0 && errno != EEXIST) {
#else
	if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
#endif
		fprintf(stderr, "[rank %d] failed to create log directory %s\n",
			rank, log_dir);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	/* open log file for this rank */
	if (snprintf(log_file, PATH_MAX, "%s/rank_%d.log", log_dir, rank) >=
	    PATH_MAX) {
		fprintf(stderr, "[rank %d] log file path too long\n", rank);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	log_fp = fopen(log_file, "w");
	if (!log_fp) {
		fprintf(stderr, "[rank %d] failed to open log file %s\n", rank,
			log_file);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	/* redirect stdout and stderr to log file */
	fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND, 0644);
	if (fd >= 0) {
		dup2(fd, FILENO_STDOUT);
		dup2(fd, FILENO_STDERR);
		close(fd);
	} else {
		fprintf(stderr, "[rank %d] failed to redirect output to %s\n",
			rank, log_file);
		fclose(log_fp);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
}

/* log message to file and optionally to screen */
void log_message(const char *level, const char *message, bool print_to_screen)
{
	time_t now;
	struct tm *tm;
	char timestamp[32];

	if (print_to_screen) {
		fprintf(stderr, "[rank %d] %s\n", current_rank, message);
	}
	if (!log_fp) {
		return;
	}

	/* get timestamp */
	now = time(NULL);
	tm = localtime(&now);
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

	/* write to log file */
	fprintf(log_fp, "[%s] [rank %d] [%s] %s\n", timestamp, current_rank,
		level, message);
	fflush(log_fp);
}

/* close log file */
void finalize_logging(void)
{
	if (log_fp) {
		fclose(log_fp);
		log_fp = NULL;
	}
}
