#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comparison.h"
#include "futoshiki.h"

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <puzzle_file> [-c|-n]\n", argv[0]);
        printf("  -c: comparison mode (run both with and without precoloring)\n");
        printf("  -n: disable precoloring\n");
        printf("  -v: verbose mode (show progress messages)\n");
        return 1;
    }

    // Set up OpenMP environment
    char* omp_num_threads = getenv("OMP_NUM_THREADS");
    if (!omp_num_threads) {
        // If OMP_NUM_THREADS is not set, use all available cores
        omp_set_num_threads(omp_get_max_threads());
    }

    printf("Running with %d OpenMP threads\n", omp_get_max_threads());

    // Parse command-line options
    bool use_precoloring = true;
    bool verbose = false;
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            set_progress_display(verbose);
            run_comparison(argv[1]);
            return 0;
        } else if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }

    set_progress_display(verbose);
    SolverStats stats = solve_puzzle(argv[1], use_precoloring, true);
    print_stats(&stats, "");

    return 0;
}