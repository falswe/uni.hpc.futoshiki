#include <omp.h>  // Referencing external library

#include "hybrid.h"

int main(int argc, char* argv[]) {
    // Initialize the hybrid MPI+OpenMP environment
    mpi_init(&argc, &argv);

    // Print usage information if not enough arguments are provided
    if (argc < 2) {
        if (g_mpi_rank == 0) {
            printf("Usage: %s <puzzle_file> [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -n : Disable pre-coloring optimization\n");
            printf("  -q : Quiet mode (only essential results and errors)\n");
            printf("  -v : Verbose mode (shows progress and details)\n");
            printf("  -d : Debug mode (shows all messages)\n");
            printf(
                "  -mf <factor>: Set MPI task generation factor "
                "(for master-worker distribution)\n");
            printf(
                "  -of <factor>: Set OpenMP task generation factor "
                "(for thread-level distribution)\n");
        }
        mpi_finalize();
        return 1;
    }

    // Default parameters
    const char* filename = argv[1];
    bool use_precoloring = true;
    LogLevel log_level = LOG_INFO;
    double mpi_task_factor = 1.0;
    double omp_task_factor = 1.0;

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
                mpi_finalize();
                return 1;
            }
        } else if (strcmp(argv[i], "-of") == 0 && i + 1 < argc) {
            omp_task_factor = atof(argv[++i]);
            if (omp_task_factor <= 0) {
                if (g_mpi_rank == 0) fprintf(stderr, "Error: Invalid OpenMP task factor\n");
                mpi_finalize();
                return 1;
            }
        }
    }

    // Initialize logger and set task factors
    logger_init(log_level);
    hybrid_set_mpi_task_factor(mpi_task_factor);
    omp_set_task_factor(omp_task_factor);

    // Master process prints header information
    if (g_mpi_rank == 0) {
        log_info("=============================");
        log_info("Futoshiki Hybrid Solver");
        log_info("=============================");
        log_info("Running with %d process(es) and %d OpenMP thread(s) per process", g_mpi_size,
                 omp_get_max_threads());
        log_info("Puzzle file: %s", filename);
        log_info("Mode: %s pre-coloring\n", use_precoloring ? "WITH" : "WITHOUT");
    }

    // Call the main hybrid solver function
    // The third argument controls whether the solution is printed (only by
    // master)
    SolverStats stats = hybrid_solve_puzzle(filename, use_precoloring, g_mpi_rank == 0);

    // Master process prints the final statistics
    if (g_mpi_rank == 0 && stats.found_solution) {
        log_info("\n--- Final Statistics ---");
        print_stats(&stats, "Hybrid Solver");
    }

    // Finalize the MPI environment
    mpi_finalize();

    // Return 0 on success (solution found), 1 on failure
    return stats.found_solution ? 0 : 1;
}
