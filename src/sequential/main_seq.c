#include <stdio.h>
#include <string.h>

#include "../common/comparison.h"
#include "../common/futoshiki_common.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -c : Run comparison mode (with vs without pre-coloring)\n");
        printf("  -n : Disable pre-coloring optimization\n");
        printf("  -v : Verbose mode (show progress messages)\n");
        return 1;
    }

    printf("Futoshiki Sequential Solver\n");
    printf("===========================\n\n");

    // Parse command-line options
    const char* filename = argv[1];
    bool use_precoloring = true;
    bool verbose = false;
    bool comparison_mode = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            comparison_mode = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
    }

    set_progress_display(verbose);

    if (comparison_mode) {
        run_comparison(filename);
    } else {
        SolverStats stats = solve_puzzle(filename, use_precoloring, true);
        print_stats(&stats, "Sequential Solver");
    }

    return 0;
}