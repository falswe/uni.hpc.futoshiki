#include "comparison.h"

#include <stdio.h>

#include "futoshiki.h"

void print_stats(const SolverStats* stats, const char* prefix) {
    printf("%s Results:\n", prefix);
    printf("  Colors removed in precoloring: %d\n", stats->colors_removed);
    printf("  Colors remaining after precoloring: %d\n", stats->remaining_colors);
    printf("\n  Timing:\n");
    printf("  Pre-coloring phase: %.6f seconds\n", stats->precolor_time);
    printf("  List-coloring phase: %.6f seconds\n", stats->coloring_time);
    printf("  Total solving time: %.6f seconds\n", stats->total_time);

    printf("  Found solution: %s\n", stats->found_solution ? "Yes" : "No");
}

void print_comparison(const SolverStats* with_precolor, const SolverStats* without_precolor) {
    printf("\nComparison (without vs with pre-coloring):\n");
    printf("Time Differences:\n");
    printf("  Total time: %+.6f seconds (factor %.2f)\n",
           without_precolor->total_time - with_precolor->total_time,
           without_precolor->total_time / with_precolor->total_time);
    printf("  List-coloring phase: %+.6f seconds (factor %.2f)\n",
           without_precolor->coloring_time - with_precolor->coloring_time,
           without_precolor->coloring_time / with_precolor->coloring_time);

    printf("\nColor Statistics:\n");
    int color_diff = without_precolor->remaining_colors - with_precolor->remaining_colors;
    printf("  Search space reduction: %d colors (factor %.2f)\n", color_diff,
           ((double)with_precolor->remaining_colors / without_precolor->remaining_colors));
}

void run_comparison(const char* filename) {
    printf("Running comparison mode...\n");

    // Run with precoloring
    printf("\nTesting with precoloring enabled...\n");
    SolverStats with_precolor = solve_puzzle(filename, true, false);

    // Run without precoloring
    printf("\nTesting with precoloring disabled...\n");
    SolverStats without_precolor = solve_puzzle(filename, false, false);

    // Print results
    print_stats(&with_precolor, "\nWith Precoloring");
    print_stats(&without_precolor, "\nWithout Precoloring");
    print_comparison(&with_precolor, &without_precolor);
}
