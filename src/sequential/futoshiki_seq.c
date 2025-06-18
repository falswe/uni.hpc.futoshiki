#include "../common/comparison.h"
#include "../common/futoshiki_common.h"

// Sequential backtracking algorithm
static bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    // Check if we have completed the grid
    if (row >= puzzle->size) {
        return true;
    }

    // Move to the next row when current row is complete
    if (col >= puzzle->size) {
        return color_g_seq(puzzle, solution, row + 1, 0);
    }

    // Skip given cells
    if (puzzle->board[row][col] != EMPTY) {
        solution[row][col] = puzzle->board[row][col];
        return color_g_seq(puzzle, solution, row, col + 1);
    }

    // Try each possible color for current cell
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (color_g_seq(puzzle, solution, row, col + 1)) {
                return true;
            }
            solution[row][col] = EMPTY;  // Backtrack
        }
    }

    return false;
}

// Public solving function
bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    print_progress("Starting sequential backtracking");

    // Initialize solution with given board values
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            solution[r][c] = puzzle->board[r][c];
        }
    }

    return color_g_seq(puzzle, solution, 0, 0);
}

// Main solving function that follows the same interface as other implementations
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

        if (print_solution) {
            print_progress("\nPossible colors for each cell:\n");
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
            printf("\nTiming Results:\n");
            printf("Pre-coloring phase: %.6f seconds\n", stats.precolor_time);
            printf("List-coloring phase: %.6f seconds\n", stats.coloring_time);
            printf("Total solving time: %.6f seconds\n", stats.total_time);

            if (stats.found_solution) {
                printf("\nSolution:\n");
                print_board(&puzzle, solution);
            } else {
                printf("\nNo solution found.\n");
            }
        }
    }

    return stats;
}