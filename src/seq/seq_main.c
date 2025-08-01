#include <stdio.h>
#include <string.h>

#include "seq.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -n : Disable pre-coloring optimization\n");
        printf("  -q : Quiet mode (only essential results and errors)\n");
        printf("  -v : Verbose mode (shows progress and details)\n");
        printf("  -d : Debug mode (shows all messages)\n");
        return 1;
    }

    const char* filename = argv[1];
    bool use_precoloring = true;
    LogLevel log_level = LOG_INFO;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
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

    log_info("===========================");
    log_info("Futoshiki Sequential Solver");
    log_info("===========================");
    log_info("Running with 1 process");
    log_info("Puzzle file: %s", filename);

    log_info("Mode: %s pre-coloring\n", use_precoloring ? "WITH" : "WITHOUT");
    SolverStats stats = seq_solve_puzzle(filename, use_precoloring, true);
    print_stats(&stats, "Sequential Solver");

    return 0;
}