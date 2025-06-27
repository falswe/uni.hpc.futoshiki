#include "../common/futoshiki_common.h"

/**
 * Sequential implementation of the Futoshiki solver
 * Uses backtracking with list coloring optimization
 */

// Sequential solving entry point
static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting sequential backtracking");

    // Initialize solution with board values
    memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);

    return color_g_seq(puzzle, solution, 0, 0);
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