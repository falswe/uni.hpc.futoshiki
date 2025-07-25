#ifndef FUTOSHIKI_OMP_H
#define FUTOSHIKI_OMP_H

#include "../common/futoshiki_common.h"

/**
 * Solve a Futoshiki puzzle from file using OpenMP parallelization.
 * This is the primary function exported by the OpenMP solver implementation.
 * It reads a puzzle, solves it, and returns statistics about the run.
 *
 * @param filename Path to the puzzle file.
 * @param use_precoloring Whether to use pre-coloring optimization.
 * @param print_solution Whether to print the puzzle and its solution.
 * @return SolverStats structure with timing and solution information.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * Sets the task generation multiplication factor for OpenMP.
 * @param factor The multiplier for the number of threads (e.g., 4.0 for 4x tasks).
 */
void omp_set_task_factor(double factor);

#endif  // FUTOSHIKI_OMP_H