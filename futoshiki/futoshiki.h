#ifndef FUTOSHIKI_H
#define FUTOSHIKI_H

#include <stdbool.h>

#include "comparison.h"  // For SolverStats

SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

void set_progress_display(bool show);

#endif  // FUTOSHIKI_H
