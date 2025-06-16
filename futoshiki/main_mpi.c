#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "comparison.h"

// External MPI variables and functions from futoshiki_mpi.c
extern int g_mpi_rank;
extern int g_mpi_size;
extern void init_mpi(int* argc, char*** argv);
extern void finalize_mpi();
extern void set_progress_display(bool show);
extern SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

int main(int argc, char* argv[]) {
    // Initialize MPI
    init_mpi(&argc, &argv);

    if (argc != 2 && argc != 3) {
        if (g_mpi_rank == 0) {
            printf("Usage: %s <puzzle_file> [-c|-n|-v]\n", argv[0]);
            printf("  -c: comparison mode (run both with and without precoloring)\n");
            printf("  -n: disable precoloring\n");
            printf("  -v: verbose mode (show progress messages)\n");
        }
        finalize_mpi();
        return 1;
    }

    // Parse command-line options
    bool use_precoloring = true;
    bool verbose = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            set_progress_display(verbose);
            // Only master runs comparison, but all processes must participate in solve_puzzle
            if (g_mpi_rank == 0) {
                printf("Running comparison mode...\n");

                // Run with precoloring
                printf("\nTesting with precoloring enabled...\n");
            }
            SolverStats with_precolor = solve_puzzle(argv[1], true, false);

            if (g_mpi_rank == 0) {
                // Run without precoloring
                printf("\nTesting with precoloring disabled...\n");
            }
            SolverStats without_precolor = solve_puzzle(argv[1], false, false);

            if (g_mpi_rank == 0) {
                // Print results
                print_stats(&with_precolor, "\nWith Precoloring");
                print_stats(&without_precolor, "\nWithout Precoloring");
                print_comparison(&with_precolor, &without_precolor);
            }

            finalize_mpi();
            return 0;
        } else if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }

    set_progress_display(verbose);

    // All processes need to call solve_puzzle due to collective operations
    SolverStats stats = solve_puzzle(argv[1], use_precoloring, true);

    // Only master prints statistics
    if (g_mpi_rank == 0) {
        print_stats(&stats, "");
    }

    finalize_mpi();
    return 0;
}