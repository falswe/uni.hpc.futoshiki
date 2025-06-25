#include "futoshiki_omp.h"

#include <omp.h>
#include <string.h>

// Private helper for this file only -> static
static bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col);

bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    print_progress("Starting parallel backtracking with OpenMP");

    bool found_solution = false;

    // Find first empty cell to parallelize on
    // TODO: consider the subsequent empty cells for parallelization
    //       rn only 2 threads are doing the work if there's only 2 colors in the first empty cell
    //       this could be improved greatly by just going on with other empty cells
    int start_row = 0, start_col = 0;
    bool found_empty = false;

    for (int r = 0; r < puzzle->size && !found_empty; r++) {
        for (int c = 0; c < puzzle->size && !found_empty; c++) {
            if (puzzle->board[r][c] == EMPTY) {
                start_row = r;
                start_col = c;
                found_empty = true;
            } else {
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    if (!found_empty) {
        // No empty cells, puzzle is already solved
        return true;
    }

    print_progress("Parallelizing on first empty cell");
    print_progress("First empty cell at (%d,%d) with %d possible colors", start_row, start_col,
                   puzzle->pc_lengths[start_row][start_col]);

    // Manually create private copies for each task to avoid issues
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

/**
 * Standard sequential backtracking solver. This is called by each OpenMP task
 * to solve the sub-problem assigned to it.
 */
static bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    if (row >= puzzle->size) return true;  // Entire grid has been filled

    int next_row = (col == puzzle->size - 1) ? row + 1 : row;
    int next_col = (col == puzzle->size - 1) ? 0 : col + 1;

    // If the cell is not empty, it was either part of the original puzzle or
    // set by a previous step in the recursion. Move to the next cell.
    if (solution[row][col] != EMPTY) {
        return color_g_seq(puzzle, solution, next_row, next_col);
    }

    // Try all valid, pre-filtered colors for this cell.
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (color_g_seq(puzzle, solution, next_row, next_col)) {
                return true;
            }
        }
    }
    solution[row][col] = EMPTY;  // Backtrack: no valid color worked, so reset and return false.
    return false;
}

/**
 * Main solving interface for the OpenMP implementation.
 * It orchestrates the pre-coloring and parallel solving phases.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    if (read_puzzle_from_file(filename, &puzzle)) {
        if (print_solution) {
            printf("Initial puzzle:\n");
            int initial_board[MAX_N][MAX_N];
            memcpy(initial_board, puzzle.board, sizeof(initial_board));
            print_board(&puzzle, initial_board);
        }

        // Time the pre-coloring phase
        double start_precolor = get_time();
        stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
        double end_precolor = get_time();
        stats.precolor_time = end_precolor - start_precolor;

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

        stats.found_solution = color_g(&puzzle, solution, 0, 0);

        double end_coloring = get_time();
        stats.coloring_time = end_coloring - start_coloring;
        stats.total_time = stats.precolor_time + stats.coloring_time;

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
