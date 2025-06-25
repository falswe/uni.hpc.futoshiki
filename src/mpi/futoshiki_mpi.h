#ifndef FUTOSHIKI_MPI_H
#define FUTOSHIKI_MPI_H

#include <stdbool.h>

// Correctly include the common header for shared definitions like SolverStats and Futoshiki.
// The relative path ../common/ is necessary.
#include "../common/futoshiki_common.h"

// Note: The global MPI variables g_mpi_rank and g_mpi_size are declared as 'weak'
// in futoshiki_common.c and will be strongly defined in futoshiki_mpi.c.

/**
 * Main solving interface for the MPI implementation.
 * This function orchestrates the solving process across all MPI processes.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * Initializes the MPI environment. Must be called at the start of main.
 */
void init_mpi(int* argc, char*** argv);

/**
 * Finalizes the MPI environment. Must be called at the end of main.
 */
void finalize_mpi();

#endif  // FUTOSHIKI_MPI_H