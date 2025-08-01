#ifndef MPI_H
#define MPI_H

#include <stdbool.h>

#include "../common/utils.h"

/**
 * Main solving interface for the MPI implementation
 * This function orchestrates the solving process across all MPI processes
 */
SolverStats mpi_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * Initializes the MPI environment. Must be called at the start of main
 */
void mpi_init(int* argc, char*** argv);

/**
 * Finalizes the MPI environment. Must be called at the end of main
 */
void mpi_finalize(void);

/**
 * Sets the task generation multiplication factor for MPI.
 * @param factor The multiplier for the number of workers (e.g., 1.0 for 1x tasks).
 */
void mpi_set_task_factor(double factor);

#endif  // MPI_H