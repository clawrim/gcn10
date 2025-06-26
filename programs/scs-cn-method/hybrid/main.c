#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "global.h"
#include <sys/sysinfo.h>

void check_memory_availability(int rank, int size) {
    struct sysinfo info;
    sysinfo(&info);
    double available_memory_gb = (double)info.freeram / (1024.0 * 1024.0 * 1024.0);
    double required_memory_gb = 30.0 / size;  // Adjusted per process

    if (available_memory_gb < required_memory_gb) {
        if (rank == 0) {
            fprintf(stderr, "Warning: Limited memory. Running with %d processes may cause swapping!\n", size);
        }
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    check_memory_availability(rank, size);

    printf("MPI Rank %d started\n", rank);
    fflush(stdout);

    if (argc != 5) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s <cn_rasters_list.txt> <rainfall_rasters_list.txt> <output_dir> <threads>\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int num_threads = atoi(argv[4]);
    omp_set_num_threads(num_threads);

    if (rank == 0) {
        printf("Running with %d MPI processes and %d OpenMP threads per process\n", size, num_threads);
    }

    distribute_and_process_blocks(argv[1], argv[2], argv[3], num_threads);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
