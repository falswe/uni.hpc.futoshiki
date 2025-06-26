#ifndef FUTOSHIKI_SEQ_H
#define FUTOSHIKI_SEQ_H

#include "../common/futoshiki_common.h"

/**
 * Sequential Futoshiki solver implementation
 * Uses backtracking with list coloring optimization
 */

/**
 * Main solving function for sequential implementation
 * @param puzzle The Futoshiki puzzle structure
 * @param solution Output solution matrix
 * @param row Starting row (usually 0)
 * @param col Starting column (usually 0)
 * @return true if solution found, false otherwise
 */
bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col);

/**
 * Solve a Futoshiki puzzle from file
 * @param filename Path to the puzzle file
 * @param use_precoloring Whether to use pre-coloring optimization
 * @param print_solution Whether to print the solution
 * @return SolverStats structure with timing and solution information
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif  // FUTOSHIKI_SEQ_H