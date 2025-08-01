#include <omp.h>  // Referencing external library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/utils.h"
#include "omp.h"  // Referencing our own code

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -n       : Disable pre-coloring optimization\n");
        printf("  -q       : Quiet mode (only essential results and errors)\n");
        printf("  -v       : Verbose mode (shows progress and details)\n");
        printf("  -d       : Debug mode (shows all messages)\n");
        printf("  -t <num> : Set number of OpenMP threads (default: all available)\n");
        return 1;
    }

    const char* filename = argv[1];
    bool use_precoloring = true;
    LogLevel log_level = LOG_INFO;
    int requested_threads = 0;
    double task_factor = 1.0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-q") == 0) {
            log_level = LOG_ESSENTIAL;
        } else if (strcmp(argv[i], "-v") == 0) {
            log_level = LOG_VERBOSE;
        } else if (strcmp(argv[i], "-d") == 0) {
            log_level = LOG_DEBUG;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            requested_threads = atoi(argv[++i]);
            if (requested_threads <= 0) {
                fprintf(stderr, "Error: Invalid thread count\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            task_factor = atof(argv[++i]);
        }
    }

    if (requested_threads > 0) {
        omp_set_num_threads(requested_threads);
    }

    logger_init(log_level);

    omp_set_task_factor(task_factor);

    log_info("================================");
    log_info("Futoshiki OpenMP Parallel Solver");
    log_info("================================");
    log_info("Running with %d OpenMP thread(s)", omp_get_max_threads());
    log_info("Puzzle file: %s", filename);
    log_info("Mode: %s pre-coloring\n", use_precoloring ? "WITH" : "WITHOUT");

    SolverStats stats = omp_solve_puzzle(filename, use_precoloring, true);

    print_stats(&stats, "OpenMP Solver");

    return 0;
}