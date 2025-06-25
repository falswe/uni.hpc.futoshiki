#include <omp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/comparison.h"
#include "../common/futoshiki_common.h"
#include "futoshiki_omp.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -c       : Run comparison mode (with vs without pre-coloring)\n");
        printf("  -n       : Disable pre-coloring optimization\n");
        printf("  -v       : Verbose mode (show progress messages)\n");
        printf("  -t <num> : Set number of OpenMP threads (default: all available)\n");
        printf("\nExamples:\n");
        printf("  %s puzzles/puz5.txt\n", argv[0]);
        printf("  %s puzzles/puz9.txt -t 4\n", argv[0]);
        printf("  %s puzzles/puz9.txt -c -v\n", argv[0]);
        return 1;
    }

    // Parse command-line options
    const char* filename = argv[1];
    bool use_precoloring = true;
    bool verbose = false;
    bool comparison_mode = false;
    int requested_threads = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            comparison_mode = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            requested_threads = atoi(argv[++i]);  // Increment i to "-t"
        } else {
            printf("Warning: Unknown option '%s' ignored.\n", argv[i]);
        }
    }

    // Configure OpenMP threads
    if (requested_threads > 0) {
        omp_set_num_threads(requested_threads);
    }

    // Set progress display for all modules
    set_progress_display(verbose);

    // Print header
    printf("================================\n");
    printf("Futoshiki OpenMP Parallel Solver\n");
    printf("================================\n");
    printf("Running with %d OpenMP thread(s).\n", omp_get_max_threads());
    printf("Puzzle file: %s\n", filename);

    // Run in requested mode
    if (comparison_mode) {
        run_comparison(filename);
    } else {
        const char* mode_str = use_precoloring ? "WITH pre-coloring" : "WITHOUT pre-coloring";
        printf("Mode: Normal solve (%s)\n\n", mode_str);

        SolverStats stats = solve_puzzle(filename, use_precoloring, true);
        print_stats(&stats, "OpenMP Solver");
    }

    return 0;
}