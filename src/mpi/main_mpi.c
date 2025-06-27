#include <stdio.h>
#include <string.h>

#include "../common/comparison.h"
#include "futoshiki_mpi.h"

int main(int argc, char* argv[]) {
    // Initialize MPI
    init_mpi(&argc, &argv);

    // Only master validates arguments
    if (argc < 2) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "Usage: %s <puzzle_file> [options]\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  -n : Disable pre-coloring optimization\n");
            fprintf(stderr, "  -v : Verbose mode (show progress messages)\n");
        }
        finalize_mpi();
        return 1;
    }

    // Parse options
    const char* filename = argv[1];
    bool use_precoloring = true;
    bool verbose = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }

    set_progress_display(verbose);

    // Master prints header
    if (g_mpi_rank == 0) {
        printf("=============================\n");
        printf("Futoshiki MPI Parallel Solver\n");
        printf("=============================\n");
        printf("Running with %d processes\n", g_mpi_size);
        printf("Puzzle file: %s\n", filename);
        printf("Mode: %s pre-coloring\n\n", use_precoloring ? "WITH" : "WITHOUT");
    }

    // All processes participate in solving
    SolverStats stats = solve_puzzle(filename, use_precoloring, true);

    // Master prints results
    if (g_mpi_rank == 0 && stats.found_solution) {
        printf("\n--- Final Statistics ---\n");
        print_stats(&stats, "MPI Solver");
    }

    finalize_mpi();
    return stats.found_solution ? 0 : 1;
}