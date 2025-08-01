#include <stdio.h>
#include <string.h>

#include "mpi.h"  // Referencing our own code

int main(int argc, char* argv[]) {
    mpi_init(&argc, &argv);

    if (argc < 2) {
        if (g_mpi_rank == 0) {
            printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -n : Disable pre-coloring optimization\n");
            printf("  -q : Quiet mode (only essential results and errors)\n");
            printf("  -v : Verbose mode (shows progress and details)\n");
            printf("  -d : Debug mode (shows all messages)\n");
            printf("  -f <factor>: Set task generation factor (e.g., 1.0, 2.0)\n");
        }
        mpi_finalize();
        return 1;
    }

    const char* filename = argv[1];
    bool use_precoloring = true;
    LogLevel log_level = LOG_INFO;
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
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            task_factor = atof(argv[++i]);
            if (task_factor <= 0) {
                if (g_mpi_rank == 0) fprintf(stderr, "Error: Invalid task factor\n");
                mpi_finalize();
                return 1;
            }
        }
    }

    logger_init(log_level);

    mpi_set_task_factor(task_factor);

    if (g_mpi_rank == 0) {
        log_info("=============================");
        log_info("Futoshiki MPI Parallel Solver");
        log_info("=============================");
        log_info("Running with %d processes", g_mpi_size);
        log_info("Puzzle file: %s", filename);
        log_info("Mode: %s pre-coloring\n", use_precoloring ? "WITH" : "WITHOUT");
    }

    SolverStats stats = mpi_solve_puzzle(filename, use_precoloring, g_mpi_rank == 0);

    if (g_mpi_rank == 0 && stats.found_solution) {
        log_info("\n--- Final Statistics ---");
        print_stats(&stats, "MPI Solver");
    }

    mpi_finalize();
    return stats.found_solution ? 0 : 1;
}