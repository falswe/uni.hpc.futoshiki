#ifndef COMPARISON_H
#define COMPARISON_H

#include <stdbool.h>

typedef struct {
    double precolor_time;
    double coloring_time;
    double total_time;
    int colors_removed;
    int remaining_colors;
    int total_processed;
    bool found_solution;
} SolverStats;

// Print detailed statistics for a single solver run
void print_stats(const SolverStats* stats, const char* prefix);

// Print comparison between two solver runs
void print_comparison(const SolverStats* with_precolor, const SolverStats* without_precolor);

// Run comparison between solver with and without precoloring
void run_comparison(const char* filename);

#endif  // COMPARISON_H
