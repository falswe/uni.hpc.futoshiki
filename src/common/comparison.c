#include "comparison.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void print_stats(const SolverStats* stats, const char* prefix) {
    printf("\n%s Results:\n", prefix);
    printf("========================================\n");
    printf("Solution found: %s\n", stats->found_solution ? "Yes" : "No");

    if (stats->found_solution) {
        printf("\nColor Statistics:\n");
        printf("  Colors removed by pre-coloring: %d\n", stats->colors_removed);
        printf("  Colors remaining: %d\n", stats->remaining_colors);
        printf("  Search space reduction: %.1f%%\n",
               stats->colors_removed > 0
                   ? (double)stats->colors_removed /
                         (stats->colors_removed + stats->remaining_colors) * 100
                   : 0);

        printf("\nTiming Breakdown:\n");
        printf("  Pre-coloring phase: %.6f seconds\n", stats->precolor_time);
        printf("  Solving phase:      %.6f seconds\n", stats->coloring_time);
        printf("  Total time:         %.6f seconds\n", stats->total_time);

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
        printf("  With pre-coloring: %s\n", with_precolor->found_solution ? "Found" : "Not found");
        printf("  Without pre-coloring: %s\n",
               without_precolor->found_solution ? "Found" : "Not found");
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
    double time_saved = without_precolor->total_time - with_precolor->total_time;
    double speedup = without_precolor->total_time / with_precolor->total_time;
    double solving_speedup = without_precolor->coloring_time / with_precolor->coloring_time;

    printf("\nPerformance Metrics:\n");
    printf("  Overall speedup: %.2fx\n", speedup);
    printf("  Solving phase speedup: %.2fx\n", solving_speedup);
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

    // ROI analysis
    double precolor_overhead = with_precolor->precolor_time;
    double solving_benefit = without_precolor->coloring_time - with_precolor->coloring_time;
    double roi = solving_benefit / precolor_overhead;

    printf("\nPre-coloring ROI Analysis:\n");
    printf("  Investment (pre-coloring time): %.6f seconds\n", precolor_overhead);
    printf("  Return (solving time saved): %.6f seconds\n", solving_benefit);
    printf("  Return on Investment: %.2fx\n", roi);

    // Summary
    printf("\nSummary:\n");
    if (speedup > 1.0) {
        printf("  ✓ Pre-coloring provided %.1f%% performance improvement\n", (speedup - 1.0) * 100);
        printf("  ✓ Solving phase was %.1fx faster with pre-coloring\n", solving_speedup);
    } else {
        printf("  ✗ Pre-coloring did not improve performance for this puzzle\n");
        printf("  ✗ Overhead exceeded benefits by %.1f%%\n", (1.0 - speedup) * 100);
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

    // Print individual results
    print_stats(&without_precolor, "Without Pre-coloring");
    print_stats(&with_precolor, "With Pre-coloring");

    // Print comparison
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

    // Timing comparison
    printf("\nTiming Comparison:\n");
    printf("┌─────────────────┬──────────────┬──────────────┐\n");
    printf("│ Implementation  │ Time (s)     │ Speedup      │\n");
    printf("├─────────────────┼──────────────┼──────────────┤\n");
    printf("│ Sequential      │ %10.6f   │ 1.00x        │\n", seq_stats->total_time);
    printf("│ %-15s │ %10.6f   │ %.2fx        │\n", impl_name, par_stats->total_time,
           seq_stats->total_time / par_stats->total_time);
    printf("└─────────────────┴──────────────┴──────────────┘\n");

    // Calculate parallel metrics
    calculate_parallel_metrics(seq_stats->total_time, par_stats->total_time, thread_count);

    // Phase-wise comparison
    printf("\nPhase-wise Speedup:\n");
    printf("  Pre-coloring: %.2fx\n", seq_stats->precolor_time / par_stats->precolor_time);
    printf("  Solving: %.2fx\n", seq_stats->coloring_time / par_stats->coloring_time);
}

void calculate_parallel_metrics(double sequential_time, double parallel_time, int num_processors) {
    double speedup = sequential_time / parallel_time;
    double efficiency = speedup / num_processors;
    double overhead = parallel_time * num_processors - sequential_time;

    // Amdahl's Law estimation (assuming some serial fraction)
    double serial_fraction = (1.0 / speedup - 1.0 / num_processors) / (1.0 - 1.0 / num_processors);
    if (serial_fraction < 0) serial_fraction = 0;

    printf("\nParallel Performance Metrics:\n");
    printf("  Speedup (S): %.2fx\n", speedup);
    printf("  Efficiency (E): %.1f%%\n", efficiency * 100);
    printf("  Overhead: %.6f seconds\n", overhead);
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

    // Amdahl's Law analysis
    if (serial_fraction > 0) {
        printf("\nAmdahl's Law Analysis:\n");
        printf("  Estimated serial fraction: %.1f%%\n", serial_fraction * 100);
        printf("  Maximum theoretical speedup: %.1fx\n", 1.0 / serial_fraction);
    }
}

void save_stats_to_csv(const SolverStats* stats, const char* filename, const char* impl_name,
                       int threads, const char* puzzle_name) {
    FILE* file = fopen(filename, "a");
    if (!file) {
        printf("Warning: Could not open %s for writing\n", filename);
        return;
    }

    // Check if file is empty (need header)
    fseek(file, 0, SEEK_END);
    long size = ftell(file);

    if (size == 0) {
        // Write header
        fprintf(file, "Timestamp,Implementation,Threads,Puzzle,Found,PrecolorTime,");
        fprintf(file, "SolvingTime,TotalTime,ColorsRemoved,ColorsRemaining,Speedup\n");
    }

    // Get current timestamp
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Write data row
    fprintf(file, "%s,%s,%d,%s,%s,%.6f,%.6f,%.6f,%d,%d,", timestamp, impl_name, threads,
            puzzle_name, stats->found_solution ? "Yes" : "No", stats->precolor_time,
            stats->coloring_time, stats->total_time, stats->colors_removed,
            stats->remaining_colors);

    // Calculate speedup if we have sequential reference (assume 1 thread = sequential)
    if (threads == 1) {
        fprintf(file, "1.00\n");
    } else {
        fprintf(file, "N/A\n");  // Would need sequential time for accurate speedup
    }

    fclose(file);
    printf("Results saved to %s\n", filename);
}