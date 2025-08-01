#include "seq.h"

bool seq_color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    if (row >= puzzle->size) {
        return true;
    }

    if (col >= puzzle->size) {
        return seq_color_g(puzzle, solution, row + 1, 0);
    }

    if (puzzle->board[row][col] != EMPTY) {
        solution[row][col] = puzzle->board[row][col];
        return seq_color_g(puzzle, solution, row, col + 1);
    }

    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (seq_color_g(puzzle, solution, row, col + 1)) {
                return true;
            }
            solution[row][col] = EMPTY;
        }
    }

    return false;
}

static bool seq_solve(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    log_verbose("Starting sequential backtracking.");
    memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
    return seq_color_g(puzzle, solution, 0, 0);
}

SolverStats seq_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    if (!read_puzzle_from_file(filename, &puzzle)) {
        return stats;
    }

    if (print_solution) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    double start_precolor = get_time();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = get_time() - start_precolor;

    log_debug("Possible colors for each cell after pre-coloring:");
    for (int row = 0; row < puzzle.size; row++) {
        for (int col = 0; col < puzzle.size; col++) {
            char buffer[256];
            int offset = sprintf(buffer, "Cell [%d][%d]:", row, col);
            for (int i = 0; i < puzzle.pc_lengths[row][col]; i++) {
                if (offset < sizeof(buffer) - 5) {  // Safety check
                    offset += sprintf(buffer + offset, " %d", puzzle.pc_list[row][col][i]);
                }
            }
            log_debug("%s", buffer);
        }
    }

    int solution[MAX_N][MAX_N] = {{0}};
    double start_coloring = get_time();
    stats.found_solution = seq_solve(&puzzle, solution);
    stats.coloring_time = get_time() - start_coloring;
    stats.total_time = stats.precolor_time + stats.coloring_time;

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