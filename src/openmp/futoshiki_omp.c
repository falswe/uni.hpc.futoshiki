#include "futoshiki_omp.h"

#include <omp.h>
#include <string.h>

/**
 * Parallel solving strategy (shared by OpenMP and MPI):
 * 1. Find the first empty cell in the puzzle
 * 2. Distribute its possible color choices among threads/processes
 * 3. Each thread/process solves its assigned subproblem sequentially
 * 4. First solution found is returned; others are terminated
 */

// Parallelization that creates tasks for first level choices
bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting parallel backtracking");

    bool found_solution = false;
    int start_row, start_col;

    // Use the common function to find first empty cell
    bool found_empty = find_first_empty_cell(puzzle, solution, &start_row, &start_col);

    if (!found_empty) {
        // No empty cells, puzzle is already solved
        return true;
    }

    print_progress("First empty cell at (%d,%d) with %d possible colors", start_row, start_col,
                   puzzle->pc_lengths[start_row][start_col]);

    int num_colors = puzzle->pc_lengths[start_row][start_col];
    int task_solutions[MAX_N][MAX_N][MAX_N];  // One solution matrix per possible color

#pragma omp parallel
    {
#pragma omp single
        {
            print_progress("Using %d threads for parallel solving", omp_get_num_threads());

            for (int i = 0; i < num_colors && !found_solution; i++) {
                int color = puzzle->pc_list[start_row][start_col][i];

                if (safe(puzzle, start_row, start_col, solution, color)) {
#pragma omp task firstprivate(i, color) shared(found_solution, task_solutions)
                    {
                        print_progress("Thread %d trying color %d at (%d,%d)", omp_get_thread_num(),
                                       color, start_row, start_col);

                        // Create a local copy of the solution
                        int local_solution[MAX_N][MAX_N];
                        memcpy(local_solution, solution, sizeof(local_solution));

                        // Set the color for this branch
                        local_solution[start_row][start_col] = color;

                        // Try to solve using sequential algorithm from this point
                        if (color_g_seq(puzzle, local_solution, start_row, start_col + 1)) {
#pragma omp critical
                            {
                                if (!found_solution) {
                                    found_solution = true;
                                    // Save to task solutions array for the main thread to access
                                    memcpy(task_solutions[i], local_solution,
                                           sizeof(local_solution));
                                    print_progress("Thread %d found solution with color %d",
                                                   omp_get_thread_num(), color);
                                }
                            }
                        }
                    }
                }
            }

            print_progress("Waiting for tasks to complete");
#pragma omp taskwait
            print_progress("All tasks completed");
        }
    }

    // Copy solution from successful task to output solution matrix
    if (found_solution) {
        for (int i = 0; i < num_colors; i++) {
            if (task_solutions[i][start_row][start_col] != 0) {
                memcpy(solution, task_solutions[i], sizeof(task_solutions[i]));
                break;
            }
        }
    }

    return found_solution;
}

SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;
    double global_start_time = get_time();

    if (read_puzzle_from_file(filename, &puzzle)) {
        if (print_solution) {
            printf("Initial puzzle:\n");
            print_board(&puzzle, puzzle.board);
        }

        // Time the pre-coloring phase
        double start_precolor = get_time();
        stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
        stats.precolor_time = get_time() - start_precolor;

        if (print_solution && g_show_progress) {
            print_progress("Possible colors for each cell:");
            for (int row = 0; row < puzzle.size; row++) {
                for (int col = 0; col < puzzle.size; col++) {
                    print_cell_colors(&puzzle, row, col);
                }
            }
        }

        // Time the list-coloring phase
        int solution[MAX_N][MAX_N] = {{0}};
        double start_coloring = get_time();

        stats.found_solution = color_g(&puzzle, solution);

        stats.coloring_time = get_time() - start_coloring;
        stats.total_time = get_time() - global_start_time;

        // Calculate remaining colors and total processed
        stats.remaining_colors = 0;
        for (int row = 0; row < puzzle.size; row++) {
            for (int col = 0; col < puzzle.size; col++) {
                stats.remaining_colors += puzzle.pc_lengths[row][col];
            }
        }
        stats.total_processed = puzzle.size * puzzle.size * puzzle.size;

        if (print_solution) {
            if (stats.found_solution) {
                printf("Solution:\n");
                print_board(&puzzle, solution);
            } else {
                printf("No solution found.\n");
            }
        }
    }

    return stats;
}
