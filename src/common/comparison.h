#ifndef COMPARISON_H
#define COMPARISON_H

#include "futoshiki_common.h"  // For SolverStats and other types

/**
 * Comparison utilities for Futoshiki solver
 * Used to compare different solving strategies and implementations
 */

/**
 * Print detailed statistics for a single solver run
 * @param stats Pointer to solver statistics
 * @param prefix String to prefix the output (e.g., "Sequential", "OpenMP")
 */
void print_stats(const SolverStats* stats, const char* prefix);

/**
 * Print comparison between two solver runs
 * Typically used to compare with and without pre-coloring
 * @param with_precolor Stats from run with pre-coloring enabled
 * @param without_precolor Stats from run with pre-coloring disabled
 */
void print_comparison(const SolverStats* with_precolor, const SolverStats* without_precolor);

/**
 * Run comparison between solver with and without precoloring
 * This function runs the solver twice and displays comparison
 * @param filename Path to the puzzle file
 */
void run_comparison(const char* filename);

/**
 * Compare different implementations (Sequential vs Parallel)
 * @param seq_stats Stats from sequential run
 * @param par_stats Stats from parallel run
 * @param impl_name Name of parallel implementation ("OpenMP" or "MPI")
 * @param thread_count Number of threads/processes used
 */
void compare_implementations(const SolverStats* seq_stats, const SolverStats* par_stats,
                             const char* impl_name, int thread_count);

/**
 * Save statistics to CSV file for analysis
 * @param stats Solver statistics
 * @param filename Output CSV filename
 * @param impl_name Implementation name
 * @param threads Number of threads/processes
 * @param puzzle_name Name of the puzzle file
 */
void save_stats_to_csv(const SolverStats* stats, const char* filename, const char* impl_name,
                       int threads, const char* puzzle_name);

/**
 * Calculate and display parallel efficiency metrics
 * @param sequential_time Time taken by sequential implementation
 * @param parallel_time Time taken by parallel implementation
 * @param num_processors Number of processors/threads used
 */
void calculate_parallel_metrics(double sequential_time, double parallel_time, int num_processors);

#endif  // COMPARISON_H