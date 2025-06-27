#ifndef COMPARISON_H
#define COMPARISON_H

#include "futoshiki_common.h"

/**
 * Comparison utilities for analyzing Futoshiki solver performance
 */

/**
 * Print detailed statistics for a single solver run
 */
void print_stats(const SolverStats* stats, const char* prefix);

/**
 * Print comparison between two solver runs (with vs without pre-coloring)
 */
void print_comparison(const SolverStats* with_precolor, const SolverStats* without_precolor);

/**
 * Run comparison between solver with and without precoloring
 * Runs the solver twice and displays detailed comparison
 */
void run_comparison(const char* filename);

/**
 * Compare sequential vs parallel implementations
 */
void compare_implementations(const SolverStats* seq_stats, const SolverStats* par_stats,
                             const char* impl_name, int thread_count);

/**
 * Calculate and display parallel efficiency metrics
 */
void calculate_parallel_metrics(double sequential_time, double parallel_time, int num_processors);

#endif  // COMPARISON_H