#include "global.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>

int main(int argc, char **argv)
{
    int rank, size;
    char *conf_file = NULL;
    int *block_ids = NULL;
    int n_blocks = 0, i;

    /* init mpi */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* log MPI+OMP config on rank 0 */
    if (rank == 0) {
        int nthreads = omp_get_max_threads();

        fprintf(stdout,
                "MPI ranks: %d, OpenMP threads per rank: %d\n",
                size, nthreads);
    }

    /* parse args: -c config [-l list.txt] */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            conf_file = argv[++i];
        }
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            use_list_mode = true;
            block_ids_file = argv[++i];
        }
    }

    if (!conf_file) {
        if (rank == 0)
            fprintf(stderr, "missing -c config.txt\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* read and verify config */
    parse_config(conf_file);
    if (rank == 0) {
        fprintf(stderr, "config loaded:\n");
        fprintf(stderr, "  hysogs_data_path   = %s\n", hysogs_data_path);
        fprintf(stderr, "  esa_data_path      = %s\n", esa_data_path);
        fprintf(stderr, "  blocks_shp_path    = %s\n", blocks_shp_path);
        fprintf(stderr, "  lookup_table_path  = %s\n", lookup_table_path);
        fprintf(stderr, "  log_dir            = %s\n", log_dir);
    }

    /* setup per-rank logging */
    init_logging(rank);

    /* load block IDs */
    if (use_list_mode) {
        block_ids = read_block_list(block_ids_file, &n_blocks);
        if (!block_ids || n_blocks == 0) {
            fprintf(stderr, "no IDs found in %s\n", block_ids_file);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    else {
        n_blocks = get_all_blocks(&block_ids);
        if (n_blocks < 0) {
            fprintf(stderr, "failed to read shapefile %s\n", blocks_shp_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (n_blocks == 0) {
            fprintf(stderr, "no blocks found in %s\n", blocks_shp_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    /* debug: how many blocks? */
    fprintf(stderr, "rank %d: loaded %d blocks\n", rank, n_blocks);

    /* distribute blocks round-robin */
    for (i = rank; i < n_blocks; i += size) {
        process_block(block_ids[i]);
    }

    /* summary on rank 0 */
    if (rank == 0) {
        printf("processed %d blocks on %d ranks\n", n_blocks, size);
    }

    /* cleanup */
    finalize_logging();
    MPI_Finalize();
    free(block_ids);
    return 0;
}
