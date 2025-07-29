/* Main; parses command line arguments, initializes MPI and
 * logging, loads block IDs, runs block processing loop */

#include "global.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char **argv)
{
    int rank, size;
    char *conf_file = NULL;
    int *block_ids = NULL;
    int n_blocks = 0, i;
    bool overwrite = false;

    /* init mpi; get rank/size */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* parse cmd args: -c config [-l list.txt] [-o] */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i+1 < argc) {
            conf_file = argv[++i];
        }
        else if (strcmp(argv[i], "-l") == 0 && i+1 < argc) {
            use_list_mode = true;
            block_ids_file = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0) {
            overwrite = true;
        }
    }
    
    /* fail if missing config */
    if (!conf_file) {
        if (rank == 0) fprintf(stderr, "[Rank 0] Missing -c config.txt\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* read, verify and print the config */
    parse_config(conf_file);
    if (rank == 0) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Starting processing with %d MPI ranks\n"
		 "Check rank_0.log in the log directory for detailed progress\n"
                 "Config loaded:\n"
                 "  hysogs_data_path   = %s\n"
                 "  esa_data_path      = %s\n"
                 "  blocks_shp_path    = %s\n"
                 "  lookup_table_path  = %s\n"
                 "  log_dir            = %s",
                 size, hysogs_data_path, esa_data_path, blocks_shp_path, lookup_table_path, log_dir);
        log_message("INFO", msg, true);
    }

    /* setup per-rank logging */
    init_logging(rank);

    /* load block IDs; either shp or -l blocks */
    if (use_list_mode) {
        block_ids = read_block_list(block_ids_file, &n_blocks);
        if (!block_ids || n_blocks == 0) {
            char msg[8192];
            snprintf(msg, sizeof(msg), "No IDs found in %s", block_ids_file);
            log_message("ERROR", msg, true);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    } else {
        n_blocks = get_all_blocks(&block_ids);
        if (n_blocks < 0) {
            char msg[8192];
            snprintf(msg, sizeof(msg), "Failed to read shapefile %s", blocks_shp_path);
            log_message("ERROR", msg, true);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (n_blocks == 0) {
            char msg[8192];
            snprintf(msg, sizeof(msg), "No blocks found in %s", blocks_shp_path);
            log_message("ERROR", msg, true);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    /* print total blocks and mode */
    if (rank == 0) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Processing %d blocks %s", n_blocks,
                 use_list_mode ? "from list file" : "from shapefile");
        log_message("INFO", msg, true);
    }

    /* setup progress tracking on rank 0 */
    if (rank == 0) {
        for (int r = 1; r < size; r++) {
            for (int i = r; i < n_blocks; i += size) {
                int signal;
                MPI_Recv(&signal, 1, MPI_INT, r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                report_block_completion(signal, n_blocks);
            }
        }
    }

    /* distribute blocks round-robin */
    for (i = rank; i < n_blocks; i += size) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Processing block %d", block_ids[i]);
        log_message("INFO", msg, true);
        process_block(block_ids[i], overwrite, n_blocks);
    }

    /* barrier to ensure all ranks complete */
    MPI_Barrier(MPI_COMM_WORLD);

    /* summary on rank 0 */
    if (rank == 0) {
        char msg[8192];
        snprintf(msg, sizeof(msg), "Processed %d blocks on %d ranks", n_blocks, size);
        log_message("INFO", msg, true);
    }

    /* cleanup */
    finalize_logging();
    free_config();
    free(block_ids);
    MPI_Finalize();
    return 0;
}
