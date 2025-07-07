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
        printf("  -q : Quiet mode (only essential results and errors)\n");
        printf("  -v : Verbose mode (shows progress and details)\n");
        printf("  -d : Debug mode (shows all messages)\n");
        return 1;
    }

    const char* filename = argv[1];
    bool use_precoloring = true;
    bool comparison_mode = false;
    LogLevel log_level = LOG_INFO;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            comparison_mode = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-q") == 0) {
            log_level = LOG_ESSENTIAL;
        } else if (strcmp(argv[i], "-v") == 0) {
            log_level = LOG_VERBOSE;
        } else if (strcmp(argv[i], "-d") == 0) {
            log_level = LOG_DEBUG;
        }
    }

    logger_init(log_level);
    log_info("Futoshiki Sequential Solver");
    log_info("===========================");

    if (comparison_mode) {
        run_comparison(filename);
    } else {
        SolverStats stats = solve_puzzle(filename, use_precoloring, true);
        print_stats(&stats, "Sequential Solver");
    }

    return 0;
}