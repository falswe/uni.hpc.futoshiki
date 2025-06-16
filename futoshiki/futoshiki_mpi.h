#ifndef FUTOSHIKI_H
#define FUTOSHIKI_H

#include <stdbool.h>

#include "comparison.h"  // For SolverStats

// MPI-specific global variables
extern int g_mpi_rank;
extern int g_mpi_size;

// Main solving function
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

// Progress display control
void set_progress_display(bool show);

// MPI initialization and finalization
void init_mpi(int* argc, char*** argv);
void finalize_mpi();

#endif  // FUTOSHIKI_H