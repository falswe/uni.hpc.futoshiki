#include "futoshiki_omp.h"

#include <omp.h>
#include <string.h>

/**
 * OpenMP parallel implementation of the Futoshiki solver
 *
 * Parallelization strategy:
 * 1. Find the first empty cell in the puzzle
 * 2. Create parallel tasks for each possible color choice
 * 3. Each task solves its subproblem sequentially
 * 4. First solution found terminates other tasks
 */

// Parallel solving with OpenMP tasks
static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting OpenMP parallel backtracking");

    bool found_solution = false;
    int start_row, start_col;

    // Find first empty cell and initialize solution
    if (!find_first_empty_cell(puzzle, solution, &start_row, &start_col)) {
        return true;  // No empty cells, puzzle already solved
    }

    print_progress("First empty cell at (%d,%d) with %d possible colors", start_row, start_col,
                   puzzle->pc_lengths[start_row][start_col]);

    int num_colors = puzzle->pc_lengths[start_row][start_col];

    // Allocate solution storage for each task
    int(*task_solutions)[MAX_N][MAX_N] = calloc(num_colors, sizeof(*task_solutions));
    if (!task_solutions) {
        print_progress("Failed to allocate memory for parallel solutions");
        return false;
    }

#pragma omp parallel
    {
#pragma omp single
        {
            print_progress("Using %d OpenMP threads", omp_get_num_threads());

            // Create tasks for each possible color at the first empty cell
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
                        local_solution[start_row][start_col] = color;

                        // Try to solve sequentially from this point
                        if (color_g_seq(puzzle, local_solution, start_row, start_col + 1)) {
#pragma omp critical
                            {
                                if (!found_solution) {
                                    found_solution = true;
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

            print_progress("Waiting for all tasks to complete");
#pragma omp taskwait
        }
    }

    // Copy successful solution to output
    if (found_solution) {
        for (int i = 0; i < num_colors; i++) {
            if (task_solutions[i][start_row][start_col] != 0) {
                memcpy(solution, task_solutions[i], sizeof(int) * MAX_N * MAX_N);
                break;
            }
        }
    }

    free(task_solutions);
    return found_solution;
}

// Main solving function
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    if (!read_puzzle_from_file(filename, &puzzle)) {
        return stats;
    }

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

    // Time the solving phase
    int solution[MAX_N][MAX_N] = {{0}};
    double start_coloring = get_time();
    stats.found_solution = color_g(&puzzle, solution);
    stats.coloring_time = get_time() - start_coloring;
    stats.total_time = stats.precolor_time + stats.coloring_time;

    // Calculate statistics
    stats.remaining_colors = 0;
    for (int row = 0; row < puzzle.size; row++) {
        for (int col = 0; col < puzzle.size; col++) {
            stats.remaining_colors += puzzle.pc_lengths[row][col];
        }
    }
    stats.total_processed = puzzle.size * puzzle.size * puzzle.size;

    if (print_solution) {
        if (stats.found_solution) {
            printf("\nSolution:\n");
            print_board(&puzzle, solution);
        } else {
            printf("\nNo solution found.\n");
        }
    }

    return stats;
}