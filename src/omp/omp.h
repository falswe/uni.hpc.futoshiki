#ifndef OMP_H
#define OMP_H

#include "../common/utils.h"

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
SolverStats omp_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * Core OpenMP parallel solver using task-based parallelism
 * Exported for use by hybrid implementation
 *
 * @param puzzle The Futoshiki puzzle instance
 * @param solution The solution matrix to fill
 * @return true if solution found, false otherwise
 */
bool omp_solve(Futoshiki* puzzle, int solution[MAX_N][MAX_N]);

/**
 * Sets the task generation multiplication factor for OpenMP.
 * @param factor The multiplier for the number of threads (e.g., 4.0 for 4x tasks).
 */
void omp_set_task_factor(double factor);

#endif  // OMP_H