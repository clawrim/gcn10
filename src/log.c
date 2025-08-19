/* logging functions for per-rank log files and console output */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "global.h"
#include <mpi.h>

static FILE *log_fp = NULL;
static int current_rank = 0;

/* progress state for nonblocking reporting on rank 0
   rank 0 receives worker completion messages asynchronously and polls */
static int prog_expected = 0;   /* number of worker completion messages expected */
static int prog_done = 0;       /* number of worker completion messages received so far */
static MPI_Request prog_recv_req = MPI_REQUEST_NULL;
static int prog_recv_buf = -1;
static const int PROG_TAG = 100;        /* tag used for progress messages */

/* small helpers */
static void prog_post_recv(void);
static int count_rr_for_rank(int r, int size, int n);
static void ensure_log_open(void);
static void ensure_log_dir(void);
static void now_iso8601(char *buf, size_t n);

/* count how many blocks are assigned to a rank
 * under round robin assignment; formula counts
 * i in [0, n-1] such that i % size == r */
static int count_rr_for_rank(int r, int size, int n)
{
    if (n <= 0)
        return 0;
    if (r >= n)
        return 0;
    int last = n - 1;

    return 1 + (last - r) / size;
}

/* create log directory if it does not exist */
static void ensure_log_dir(void)
{
    if (!log_dir || !*log_dir)
        return;
    struct stat st;

    if (stat(log_dir, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return;
        /* path exists but is not a directory: best effort message to stderr */
        fprintf(stderr, "log: path '%s' exists and is not a directory\n",
                log_dir);
        return;
    }
    if (mkdir(log_dir, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "log: failed to create directory '%s': %s\n", log_dir,
                strerror(errno));
    }
}

/* open per-rank log file on first use */
static void ensure_log_open(void)
{
    if (log_fp)
        return;

    ensure_log_dir();

    char path[4096];

    snprintf(path, sizeof(path), "%s/rank_%d.log", log_dir ? log_dir : ".",
             current_rank);

    log_fp = fopen(path, "a");
    if (!log_fp) {
        /* fallback to stderr only if file open fails */
        fprintf(stderr,
                "log: failed to open %s: %s (fallback to stderr only)\n",
                path, strerror(errno));
    }
}

/* timestamp in iso-8601
 * local time with seconds */
static void now_iso8601(char *buf, size_t n)
{
    time_t t = time(NULL);
    struct tm tmv;

    localtime_r(&t, &tmv);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tmv);
}

/* init per-rank logging;
   opens rank_n.log under log_dir and
   remembers rank for message prefixes */
void init_logging(int rank)
{
    current_rank = rank;
    ensure_log_open();

    char ts[64];

    now_iso8601(ts, sizeof(ts));

    if (log_fp) {
        fprintf(log_fp, "[%s] [rank %d] logging started\n", ts, current_rank);
        fflush(log_fp);
    }
    /* always mirror a short start line to stderr for visibility */
    fprintf(stderr, "[%s] [rank %d] logging started\n", ts, current_rank);
}

/* post one nonblocking receive
 * for progress if none is active */
static void prog_post_recv(void)
{
    if (prog_recv_req == MPI_REQUEST_NULL && prog_expected > 0) {
        MPI_Irecv(&prog_recv_buf, 1, MPI_INT, MPI_ANY_SOURCE, PROG_TAG,
                  MPI_COMM_WORLD, &prog_recv_req);
    }
}

/* initialize nonblocking progress tracking before
 * processing starts; rank 0 does not self-send;
 * expected messages are from workers only */
void progress_init(int rank, int size, int n_blocks)
{
    int local0 = count_rr_for_rank(0, size, n_blocks);

    prog_expected = n_blocks - local0;
    prog_done = 0;
    prog_recv_req = MPI_REQUEST_NULL;
    prog_recv_buf = -1;

    if (rank == 0) {
        prog_post_recv();
    }
}

/* formatted logging helper;
   writes to per-rank file if
   available and optionally to stderr */
void log_message(const char *level, const char *msg, bool also_console)
{
    char ts[64];

    now_iso8601(ts, sizeof(ts));

    ensure_log_open();

    if (log_fp) {
        fprintf(log_fp, "[%s] [%s] [rank %d] %s\n", ts,
                level ? level : "INFO", current_rank, msg ? msg : "");
        fflush(log_fp);
    }
    if (also_console) {
        fprintf(stderr, "[%s] [%s] [rank %d] %s\n", ts,
                level ? level : "INFO", current_rank, msg ? msg : "");
    }
}

/* poll for any arrived completion messages without blocking;
   rank 0 drains all arrivals that are ready and then returns */
void progress_poll(int rank, int n_blocks)
{
    if (rank != 0)
        return;

    MPI_Status st;
    int flag = 0;

    while (1) {
        if (prog_recv_req == MPI_REQUEST_NULL) {
            prog_post_recv();
            if (prog_recv_req == MPI_REQUEST_NULL)
                break;
        }
        MPI_Test(&prog_recv_req, &flag, &st);
        if (!flag)
            break;

        /* one worker completion arrived;
         * block id is in prog_recv_buf */
        report_block_completion_local(prog_recv_buf, n_blocks);
        prog_done++;
        prog_recv_req = MPI_REQUEST_NULL;
    }
}

/* local sink for rank 0 completion logging;
   kept separate from mpi to avoid
   recursion on the progress path */
void report_block_completion_local(int block_id, int total_blocks)
{
    char line[256];

    snprintf(line, sizeof(line),
             "progress: completed block %d / total %d", block_id,
             total_blocks);
    log_message("INFO", line, false);
}

/* called when a block is completed
   workers send completion to rank 0 asynchronously
   rank 0 logs locally instead of sending to itself */
void report_block_completion(int block_id, int total_blocks)
{
    int rank = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        report_block_completion_local(block_id, total_blocks);
        return;
    }

    MPI_Request sreq;

    MPI_Isend(&block_id, 1, MPI_INT, 0, PROG_TAG, MPI_COMM_WORLD, &sreq);
    MPI_Request_free(&sreq);
}

/* after finishing local work, rank 0 drains
 * remaining worker messages by polling
 this avoids any blocking receives while
 ensuring all messages are consumed */
void progress_finalize(int rank)
{
    if (rank != 0)
        return;

    while (prog_done < prog_expected) {
        progress_poll(0, 0);
    }

    if (prog_recv_req != MPI_REQUEST_NULL) {
        int flag = 0;
        MPI_Status st;

        MPI_Test(&prog_recv_req, &flag, &st);
        if (!flag) {
            MPI_Cancel(&prog_recv_req);
            MPI_Request_free(&prog_recv_req);
        }
        prog_recv_req = MPI_REQUEST_NULL;
    }
}

/* finalize per-rank logging;
   closes the file handle if open and
   mirrors a short stop line to stderr */
void finalize_logging(void)
{
    char ts[64];

    now_iso8601(ts, sizeof(ts));

    if (log_fp) {
        fprintf(log_fp, "[%s] [rank %d] logging finished\n", ts,
                current_rank);
        fflush(log_fp);
        fclose(log_fp);
        log_fp = NULL;
    }
    fprintf(stderr, "[%s] [rank %d] logging finished\n", ts, current_rank);
}
