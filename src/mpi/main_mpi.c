#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/comparison.h"  // For print_stats
#include "futoshiki_mpi.h"

int main(int argc, char* argv[]) {
    // Initialize MPI first
    init_mpi(&argc, &argv);

    // Only master process should validate arguments and print usage
    if (argc < 2) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "Usage: %s <puzzle_file> [options]\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  -n  : Disable pre-coloring optimization\n");
            fprintf(stderr, "  -v  : Verbose mode (show progress messages)\n");
        }
        finalize_mpi();
        return 1;
    }

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

    // Set progress display; common function handles the rank check.
    set_progress_display(verbose);

    // Master process prints the header
    if (g_mpi_rank == 0) {
        printf("=============================\n");
        printf("Futoshiki MPI Parallel Solver\n");
        printf("=============================\n");
        printf("Running with %d processes.\n", g_mpi_size);
        printf("Puzzle file: %s\n", filename);
        const char* mode_str = use_precoloring ? "WITH pre-coloring" : "WITHOUT pre-coloring";
        printf("Mode: Normal solve (%s)\n\n", mode_str);
    }

    // All processes call solve_puzzle.
    // Master coordinates, workers work.
    // The function returns complete stats only on the master process.
    SolverStats stats = solve_puzzle(filename, use_precoloring, true);

    // Master process prints the final results
    if (g_mpi_rank == 0) {
        if (stats.found_solution) {
            printf("\n--- Final Statistics ---\n");
            print_stats(&stats, "MPI Solver");
        } else if (filename != NULL) {  // Check if puzzle was loaded
            printf("\nNo solution was found.\n");
        }
    }

    // Finalize MPI and exit
    finalize_mpi();
    return stats.found_solution ? 0 : 1;
}