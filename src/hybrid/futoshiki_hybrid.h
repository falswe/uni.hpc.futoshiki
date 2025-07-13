#ifndef FUTOSHIKI_HYBRID_H
#define FUTOSHIKI_HYBRID_H

#include <stdbool.h>
#include "../common/futoshiki_common.h"

// --- Global MPI variables accessible from main ---
// The 'extern' keyword tells the compiler that these variables exist
// but are defined in another file (futoshiki_hybrid.c).
extern int g_mpi_rank;
extern int g_mpi_size;

/**
 * @brief Initializes the MPI environment.
 */
void init_hybrid(int* argc, char*** argv);

/**
 * @brief Finalizes the MPI environment.
 */
void finalize_hybrid(void);

/**
 * @brief Sets the task generation factor for the MPI master process.
 * 
 * @param factor A multiplier to determine how many work units to create.
 */
void hybrid_set_mpi_task_factor(double factor);

/**
 * @brief Sets the task generation factor for the OpenMP tasks within each worker.
 * 
 * @param factor A multiplier to determine how many sub-tasks to create.
 */
void hybrid_set_omp_task_factor(double factor);

/**
 * @brief The main entry point for the hybrid Futoshiki solver.
 *
 * This function coordinates the puzzle solving process using MPI for
 * inter-process communication and OpenMP for intra-process parallelism.
 *
 * @param filename The path to the puzzle file.
 * @param use_precoloring Whether to use the pre-coloring optimization.
 * @param print_solution If true, the master process will print the final solution.
 * @return SolverStats A struct containing performance and result statistics.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif // FUTOSHIKI_HYBRID_H