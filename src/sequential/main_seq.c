#include <stdio.h>
#include <string.h>

#include "../common/comparison.h"
#include "../common/futoshiki_common.h"

// External function from futoshiki_seq.c
extern SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <puzzle_file> [-c|-n|-v]\n", argv[0]);
        printf("  -c: comparison mode (run both with and without precoloring)\n");
        printf("  -n: disable precoloring\n");
        printf("  -v: verbose mode (show progress messages)\n");
        return 1;
    }

    printf("Futoshiki Sequential Solver\n");
    printf("===========================\n\n");

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

    // Print final statistics
    printf("\nFinal Statistics:\n");
    printf("=================\n");
    print_stats(&stats, "Sequential Solver");

    return stats.found_solution ? 0 : 1;
}