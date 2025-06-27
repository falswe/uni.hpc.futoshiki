#include "comparison.h"

#include <stdio.h>

void print_stats(const SolverStats* stats, const char* prefix) {
    printf("\n%s Results:\n", prefix);
    printf("========================================\n");
    printf("Solution found: %s\n", stats->found_solution ? "Yes" : "No");

    if (!stats->found_solution) return;

    printf("\nColor Statistics:\n");
    printf("  Colors removed by pre-coloring: %d\n", stats->colors_removed);
    printf("  Colors remaining: %d\n", stats->remaining_colors);
    if (stats->colors_removed > 0) {
        double reduction =
            (double)stats->colors_removed / (stats->colors_removed + stats->remaining_colors) * 100;
        printf("  Search space reduction: %.1f%%\n", reduction);
    }

    printf("\nTiming Breakdown:\n");
    printf("  Pre-coloring phase: %.6f seconds\n", stats->precolor_time);
    printf("  Solving phase:      %.6f seconds\n", stats->coloring_time);
    printf("  Total time:         %.6f seconds\n", stats->total_time);

    if (stats->total_time > 0) {
        printf("\nTime Distribution:\n");
        printf("  Pre-coloring: %.1f%%\n", (stats->precolor_time / stats->total_time) * 100);
        printf("  Solving:      %.1f%%\n", (stats->coloring_time / stats->total_time) * 100);
    }
}

void print_comparison(const SolverStats* with_precolor, const SolverStats* without_precolor) {
    printf("\n========================================\n");
    printf("Comparison Analysis: Pre-coloring Impact\n");
    printf("========================================\n");

    // Verify both found solutions
    if (!with_precolor->found_solution || !without_precolor->found_solution) {
        printf("WARNING: Solution status differs between methods!\n");
        return;
    }

    // Time comparison
    printf("\nTiming Comparison:\n");
    printf("┌─────────────────┬──────────────┬──────────────┐\n");
    printf("│ Phase           │ Without PC   │ With PC      │\n");
    printf("├─────────────────┼──────────────┼──────────────┤\n");
    printf("│ Pre-coloring    │    0.000000s │ %10.6fs │\n", with_precolor->precolor_time);
    printf("│ Solving         │ %10.6fs │ %10.6fs │\n", without_precolor->coloring_time,
           with_precolor->coloring_time);
    printf("│ Total           │ %10.6fs │ %10.6fs │\n", without_precolor->total_time,
           with_precolor->total_time);
    printf("└─────────────────┴──────────────┴──────────────┘\n");

    // Performance metrics
    double speedup = without_precolor->total_time / with_precolor->total_time;
    double time_saved = without_precolor->total_time - with_precolor->total_time;

    printf("\nPerformance Metrics:\n");
    printf("  Overall speedup: %.2fx\n", speedup);
    printf("  Time saved: %.6f seconds (%.1f%%)\n", time_saved,
           (time_saved / without_precolor->total_time) * 100);

    // Search space analysis
    printf("\nSearch Space Analysis:\n");
    printf("  Initial colors: %d\n", without_precolor->remaining_colors);
    printf("  After pre-coloring: %d\n", with_precolor->remaining_colors);
    printf("  Reduction: %d colors (%.1f%%)\n",
           without_precolor->remaining_colors - with_precolor->remaining_colors,
           (double)(without_precolor->remaining_colors - with_precolor->remaining_colors) /
               without_precolor->remaining_colors * 100);

    // Summary
    printf("\nSummary:\n");
    if (speedup > 1.0) {
        printf("  ✓ Pre-coloring provided %.1f%% performance improvement\n", (speedup - 1.0) * 100);
    } else {
        printf("  ✗ Pre-coloring did not improve performance for this puzzle\n");
    }
}

void run_comparison(const char* filename) {
    printf("\n========================================\n");
    printf("Running Pre-coloring Comparison Analysis\n");
    printf("========================================\n");
    printf("Puzzle: %s\n", filename);

    // Run without precoloring
    printf("\nTest 1: WITHOUT pre-coloring optimization\n");
    printf("----------------------------------------\n");
    SolverStats without_precolor = solve_puzzle(filename, false, false);

    // Run with precoloring
    printf("\nTest 2: WITH pre-coloring optimization\n");
    printf("--------------------------------------\n");
    SolverStats with_precolor = solve_puzzle(filename, true, false);

    // Print results
    print_stats(&without_precolor, "Without Pre-coloring");
    print_stats(&with_precolor, "With Pre-coloring");
    print_comparison(&with_precolor, &without_precolor);
}

void compare_implementations(const SolverStats* seq_stats, const SolverStats* par_stats,
                             const char* impl_name, int thread_count) {
    printf("\n========================================\n");
    printf("Sequential vs %s Comparison\n", impl_name);
    printf("========================================\n");
    printf("Threads/Processes: %d\n", thread_count);

    if (!seq_stats->found_solution || !par_stats->found_solution) {
        printf("WARNING: Solution status differs!\n");
        return;
    }

    double speedup = seq_stats->total_time / par_stats->total_time;

    printf("\nTiming Comparison:\n");
    printf("┌─────────────────┬──────────────┬──────────────┐\n");
    printf("│ Implementation  │ Time (s)     │ Speedup      │\n");
    printf("├─────────────────┼──────────────┼──────────────┤\n");
    printf("│ Sequential      │ %10.6f   │ 1.00x        │\n", seq_stats->total_time);
    printf("│ %-15s │ %10.6f   │ %.2fx        │\n", impl_name, par_stats->total_time, speedup);
    printf("└─────────────────┴──────────────┴──────────────┘\n");

    calculate_parallel_metrics(seq_stats->total_time, par_stats->total_time, thread_count);
}

void calculate_parallel_metrics(double sequential_time, double parallel_time, int num_processors) {
    double speedup = sequential_time / parallel_time;
    double efficiency = speedup / num_processors;

    printf("\nParallel Performance Metrics:\n");
    printf("  Speedup (S): %.2fx\n", speedup);
    printf("  Efficiency (E): %.1f%%\n", efficiency * 100);
    printf("  Cost (pT): %.6f processor-seconds\n", parallel_time * num_processors);

    // Performance classification
    printf("\nPerformance Classification:\n");
    if (efficiency > 0.9) {
        printf("  ✓ Excellent: Near-linear speedup\n");
    } else if (efficiency > 0.7) {
        printf("  ✓ Good: Efficient parallelization\n");
    } else if (efficiency > 0.5) {
        printf("  ⚡ Fair: Moderate parallel overhead\n");
    } else {
        printf("  ✗ Poor: High parallel overhead\n");
    }
}