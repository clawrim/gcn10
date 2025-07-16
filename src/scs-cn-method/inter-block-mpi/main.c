#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "global.h"

int main(int argc, char *argv[]) {
    // initialize mpi
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc != 5) {
        if (rank == 0) {
            fprintf(stderr, "usage: %s <cn_list.txt> <rainfall_list.txt> <output_dir> <threads>\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[4]);
    omp_set_num_threads(num_threads);

    // distribute and process blocks using mpi
    distribute_and_process_blocks(argv[1], argv[2], argv[3], num_threads);

    // finalize mpi
    MPI_Finalize();
    return 0;
}

