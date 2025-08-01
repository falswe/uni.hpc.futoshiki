#ifndef HYBRID_H
#define HYBRID_H

#include "../common/utils.h"
#include "../mpi/mpi.h"
#include "../omp/omp.h"

// --- Global MPI variables accessible from main ---
// The 'extern' keyword tells the compiler that these variables exist
// but are defined in another file (hybrid.c).
extern int g_mpi_rank;
extern int g_mpi_size;

/**
 * @brief Sets the task generation factor for the MPI master process.
 *
 * @param factor A multiplier to determine how many work units to create.
 */
void hybrid_set_mpi_task_factor(double factor);

/**
 * @brief The main entry point for the hybrid Futoshiki solver.
 *
 * This function coordinates the puzzle solving process using MPI for
 * inter-process communication and OpenMP for intra-process parallelism.
 *
 * @param filename The path to the puzzle file.
 * @param use_precoloring Whether to use the pre-coloring optimization.
 * @param print_solution If true, the master process will print the final
 * solution.
 * @return SolverStats A struct containing performance and result statistics.
 */
SolverStats hybrid_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif  // HYBRID_H