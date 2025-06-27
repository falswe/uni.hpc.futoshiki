#ifndef FUTOSHIKI_SEQ_H
#define FUTOSHIKI_SEQ_H

#include "../common/futoshiki_common.h"

/**
 * Sequential Futoshiki solver
 *
 * This header is kept minimal since the sequential implementation
 * only needs to export the standard solve_puzzle interface.
 * The actual solving logic is implemented in futoshiki_seq.c
 */

// Main solving interface (required by all implementations)
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif  // FUTOSHIKI_SEQ_H