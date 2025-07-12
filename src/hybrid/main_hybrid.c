#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#include "../common/comparison.h"
#include "futoshiki_hybrid.h"

int main(int argc, char* argv[]) {
    // Initialize the hybrid MPI+OpenMP environment
    init_hybrid(&argc, &argv);

    // Print usage information if not enough arguments are provided
    if (argc < 2) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "Usage: %s <puzzle_file> [options]\n", argv[0]);
            fprintf(stderr, "Options:\n");
            fprintf(stderr, "  -n : Disable pre-coloring optimization\n");
            fprintf(stderr, "  -q : Quiet mode (only essential results and errors)\n");
            fprintf(stderr, "  -v : Verbose mode (shows progress and details)\n");
            fprintf(stderr, "  -d : Debug mode (shows all messages)\n");
            fprintf(stderr,
                    "  -mf <factor>: Set MPI task generation factor (for master-worker "
                    "distribution)\n");
            fprintf(stderr,
                    "  -of <factor>: Set OpenMP task generation factor (for thread-level "
                    "distribution)\n");
        }
        finalize_hybrid();
        return 1;
    }

    // Default parameters
    const char* filename = argv[1];
    bool use_precoloring = true;
    LogLevel log_level = LOG_INFO;
    double mpi_task_factor = 32.0; // Default MPI factor
    double omp_task_factor = 16.0; // Default OMP factor

    // Parse command-line arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            use_precoloring = false;
        } else if (strcmp(argv[i], "-q") == 0) {
            log_level = LOG_ESSENTIAL;
        } else if (strcmp(argv[i], "-v") == 0) {
            log_level = LOG_VERBOSE;
        } else if (strcmp(argv[i], "-d") == 0) {
            log_level = LOG_DEBUG;
        } else if (strcmp(argv[i], "-mf") == 0 && i + 1 < argc) {
            mpi_task_factor = atof(argv[++i]);
            if (mpi_task_factor <= 0) {
                if (g_mpi_rank == 0) fprintf(stderr, "Error: Invalid MPI task factor\n");
                finalize_hybrid();
                return 1;
            }
        } else if (strcmp(argv[i], "-of") == 0 && i + 1 < argc) {
            omp_task_factor = atof(argv[++i]);
            if (omp_task_factor <= 0) {
                if (g_mpi_rank == 0) fprintf(stderr, "Error: Invalid OpenMP task factor\n");
                finalize_hybrid();
                return 1;
            }
        }
    }

    // Initialize logger and set task factors
    logger_init(log_level);
    hybrid_set_mpi_task_factor(mpi_task_factor);
    hybrid_set_omp_task_factor(omp_task_factor);

    // Master process prints header information
    if (g_mpi_rank == 0) {
        log_info("======================================");
        log_info("Futoshiki Hybrid MPI+OpenMP Solver");
        log_info("======================================");
        log_info("Running with %d MPI processes", g_mpi_size);
        log_info("OMP max threads per process: %d", omp_get_max_threads());
        log_info("Puzzle file: %s", filename);
        log_info("Mode: %s pre-coloring", use_precoloring ? "WITH" : "WITHOUT");
        log_info("MPI Task Factor: %.2f | OpenMP Task Factor: %.2f\n", mpi_task_factor,
                 omp_task_factor);
    }

    // Call the main hybrid solver function
    // The third argument controls whether the solution is printed (only by master)
    SolverStats stats = solve_puzzle(filename, use_precoloring, g_mpi_rank == 0);

    // Master process prints the final statistics
    if (g_mpi_rank == 0 && stats.found_solution) {
        log_info("\n--- Final Statistics ---");
        print_stats(&stats, "Hybrid Solver");
    }

    // Finalize the MPI environment
    finalize_hybrid();

    // Return 0 on success (solution found), 1 on failure
    return stats.found_solution ? 0 : 1;
}
