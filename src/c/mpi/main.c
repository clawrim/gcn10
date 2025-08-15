/* main program: parses command-line arguments, initializes mpi,
   loads block ids, and runs block processing loop */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "global.h"

int main(int argc, char *argv[])
{
    int rank, size, n_blocks, i;
    char *conf_file;
    int *block_ids;
    bool overwrite;
    char msg[8192];

    /* initialize variables */
    rank = 0;
    size = 0;
    n_blocks = 0;
    conf_file = NULL;
    block_ids = NULL;
    overwrite = false;

    /* initialize mpi */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* parse command-line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            conf_file = argv[++i];
        }
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            use_list_mode = true;
            block_ids_file = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0) {
            overwrite = true;
        }
    }

    /* validate config file */
    if (!conf_file) {
        if (rank == 0) {
            fprintf(stderr, "[rank 0] missing -c config.txt\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* read and print config */
    parse_config(conf_file);
    if (rank == 0) {
        snprintf(msg, sizeof(msg),
                 "starting processing with %d mpi ranks\n"
                 "check rank_0.log in the log directory for detailed progress\n"
                 "config loaded:\n"
                 "  hysogs_data_path   = %s\n"
                 "  esa_data_path      = %s\n"
                 "  blocks_shp_path    = %s\n"
                 "  lookup_table_path  = %s\n"
                 "  log_dir            = %s",
                 size, hysogs_data_path, esa_data_path,
                 blocks_shp_path, lookup_table_path, log_dir);
        log_message("INFO", msg, true);
    }

    /* setup per-rank logging */
    init_logging(rank);

    /* load block ids */
    if (use_list_mode) {
        block_ids = read_block_list(block_ids_file, &n_blocks);
        if (rank == 0 && (!block_ids || !n_blocks)) {
            snprintf(msg, sizeof(msg), "no ids found in %s", block_ids_file);
            log_message("ERROR", msg, true);
        }
    } else {
        block_ids = get_all_blocks(&n_blocks);
        if (rank == 0 && (!block_ids || !n_blocks)) {
            if (!block_ids) {
                snprintf(msg, sizeof(msg), "failed to read shapefile %s", blocks_shp_path);
                log_message("ERROR", msg, true);
            }
            if (!n_blocks) {
                snprintf(msg, sizeof(msg), "no blocks found in %s", blocks_shp_path);
                log_message("ERROR", msg, true);
            }
        }
    }

    /* validate and abort if nothing to do */
    if (!block_ids || !n_blocks)
        MPI_Abort(MPI_COMM_WORLD, 1);

    /* init async progress so rank 0 can
     * both work and poll without blocking */
    progress_init(rank, size, n_blocks);

    if (rank == 0) {
        /* print total blocks and mode */
        snprintf(msg, sizeof(msg), "processing %d blocks %s", n_blocks,
                 use_list_mode ? "from list file" : "from shapefile");
        log_message("INFO", msg, true);
    }

    /* distribute blocks round-robin */
    for (i = rank; i < n_blocks; i += size) {
        snprintf(msg, sizeof(msg), "processing block %d", block_ids[i]);
        log_message("INFO", msg, true);

        process_block(block_ids[i], overwrite, n_blocks);

        /* rank 0 polls here to drain 
	 * progress without blocking */
        progress_poll(rank, n_blocks);
    }

    /* after finishing local work, rank 0 
     * drains remaining worker signals */
    progress_finalize(rank);

    /* synchronize all ranks */
    MPI_Barrier(MPI_COMM_WORLD);

    /* print summary on rank 0 */
    if (rank == 0) {
        snprintf(msg, sizeof(msg), "processed %d blocks on %d ranks", n_blocks, size);
        log_message("INFO", msg, true);
    }

    /* cleanup */
    finalize_logging();
    free_config();
    free(block_ids);
    MPI_Finalize();

    exit(EXIT_SUCCESS);
}
