#ifndef FUTOSHIKI_HYBRID_H
#define FUTOSHIKI_HYBRID_H

#include <stdbool.h>

#include "../common/futoshiki_common.h"

/**
 * @brief Main solving interface for the Hybrid MPI+OpenMP implementation.
 *
 * This function orchestrates the solving process, with the master process (rank 0)
 * distributing work to worker processes, which in turn use OpenMP threads to
 * solve their assigned sub-problems.
 *
 * @param filename The path to the puzzle file.
 * @param use_precoloring A flag to enable or disable the pre-coloring optimization.
 * @param print_solution A flag to indicate if the final solution should be printed (only done by master).
 * @return A SolverStats struct containing performance and result information.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * @brief Initializes the hybrid MPI+OpenMP environment.
 *
 * This must be called at the start of main. It initializes MPI with thread support.
 * @param argc A pointer to the argument count from main.
 * @param argv A pointer to the argument vector from main.
 */
void init_hybrid(int* argc, char*** argv);

/**
 * @brief Finalizes the MPI environment.
 *
 * This must be called at the end of main to clean up MPI resources.
 */
void finalize_hybrid();

/**
 * @brief Sets the task generation multiplication factor for MPI work distribution.
 *
 * This controls how many large work units the master process creates to distribute
 * among the MPI worker processes.
 * @param factor The multiplier for the number of workers (e.g., 32.0 for 32x tasks).
 */
void hybrid_set_mpi_task_factor(double factor);

/**
 * @brief Sets the task generation multiplication factor for OpenMP task creation.
 *
 * This controls how many smaller tasks an MPI worker creates to be solved by its
 * local pool of OpenMP threads.
 * @param factor The multiplier for the number of threads (e.g., 16.0 for 16x tasks).
 */
void hybrid_set_omp_task_factor(double factor);

#endif  // FUTOSHIKI_HYBRID_H