#include <stdio.h>
#include <string.h>

#include "../common/comparison.h"
#include "futoshiki_mpi.h"

int main(int argc, char* argv[]) {
    init_mpi(&argc, &argv);

    if (argc < 2) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "Usage: %s <puzzle_file> [options]\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  -n : Disable pre-coloring optimization\n");
            fprintf(stderr, "  -q : Quiet mode (only essential results and errors)\n");
            fprintf(stderr, "  -v : Verbose mode (shows progress and details)\n");
            fprintf(stderr, "  -d : Debug mode (shows all messages)\n");
        }
        finalize_mpi();
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

    if (g_mpi_rank == 0) {
        log_info("=============================");
        log_info("Futoshiki MPI Parallel Solver");
        log_info("=============================");
        log_info("Running with %d processes", g_mpi_size);
        log_info("Puzzle file: %s", filename);
        log_info("Mode: %s pre-coloring\n", use_precoloring ? "WITH" : "WITHOUT");
    }

    SolverStats stats = solve_puzzle(filename, use_precoloring, g_mpi_rank == 0);

    if (g_mpi_rank == 0 && stats.found_solution) {
        log_info("\n--- Final Statistics ---");
        print_stats(&stats, "MPI Solver");
    }

    finalize_mpi();
    return stats.found_solution ? 0 : 1;
}