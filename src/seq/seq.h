#ifndef SEQ_H
#define SEQ_H

#include "../common/utils.h"

/**
 * Sequential Futoshiki solver
 *
 * This header exports both the main solving interface and the core
 * sequential backtracking function for use by other implementations.
 */

// Main solving interface
SolverStats seq_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

/**
 * Core sequential backtracking solver
 * Exported for use by parallel implementations as a fallback or for subtasks
 *
 * @param puzzle The Futoshiki puzzle instance
 * @param solution The solution matrix to fill
 * @param row Starting row position
 * @param col Starting column position
 * @return true if solution found, false otherwise
 */
bool seq_color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col);

#endif  // SEQ_H