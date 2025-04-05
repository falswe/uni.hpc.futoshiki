#include "futoshiki.h"

#include <ctype.h>
#include <omp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "comparison.h"

#define MAX_N 50
#define EMPTY 0

typedef enum { NO_CONS = 0, GREATER = 1, SMALLER = 2 } Constraint;

typedef struct {
    int size;                             // Size of the puzzle (N)
    int board[MAX_N][MAX_N];              // The puzzle grid (0 means empty cell)
    Constraint h_cons[MAX_N][MAX_N - 1];  // Horizontal inequality constraints
    Constraint v_cons[MAX_N - 1][MAX_N];  // Vertical inequality constraints
    int pc_list[MAX_N][MAX_N][MAX_N];     // Possible colors for each cell
                                          // [row][col][possible_values]
    int pc_lengths[MAX_N][MAX_N];         // Possible colors list length for each cell
} Futoshiki;

static bool g_show_progress = false;

void set_progress_display(bool show) { g_show_progress = show; }

static void print_progress(const char* format, ...) {
    if (!g_show_progress) return;

    va_list args;
    va_start(args, format);
    printf("[PROGRESS] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

static void print_cell_colors(const Futoshiki* puzzle, int row, int col) {
    if (!g_show_progress) return;

    printf("[PROGRESS] Cell [%d][%d]: ", row, col);
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        printf("%d ", puzzle->pc_list[row][col][i]);
    }
    printf("\n");
}

bool safe(const Futoshiki* puzzle, int row, int col, int solution[MAX_N][MAX_N], int color) {
    // If cell has a given color, only allow that color
    if (puzzle->board[row][col] != EMPTY) {
        return puzzle->board[row][col] == color;
    }

    // Check horizontal inequality constraints
    if (col > 0) {
        if (puzzle->h_cons[row][col - 1] == GREATER &&  //
            solution[row][col - 1] != EMPTY &&          //
            solution[row][col - 1] <= color) {
            return false;
        }
        if (puzzle->h_cons[row][col - 1] == SMALLER &&  //
            solution[row][col - 1] != EMPTY &&          //
            solution[row][col - 1] >= color) {
            return false;
        }
    }
    if (col < puzzle->size - 1) {
        if (puzzle->h_cons[row][col] == GREATER &&  //
            solution[row][col + 1] != EMPTY &&      //
            color <= solution[row][col + 1]) {
            return false;
        }
        if (puzzle->h_cons[row][col] == SMALLER &&  //
            solution[row][col + 1] != EMPTY &&      //
            color >= solution[row][col + 1]) {
            return false;
        }
    }

    // Check vertical inequality constraints
    if (row > 0) {
        if (puzzle->v_cons[row - 1][col] == GREATER &&  //
            solution[row - 1][col] != EMPTY &&          //
            solution[row - 1][col] <= color) {
            return false;
        }
        if (puzzle->v_cons[row - 1][col] == SMALLER &&  //
            solution[row - 1][col] != EMPTY &&          //
            solution[row - 1][col] >= color) {
            return false;
        }
    }
    if (row < puzzle->size - 1) {
        if (puzzle->v_cons[row][col] == GREATER &&  //
            solution[row + 1][col] != EMPTY &&      //
            color <= solution[row + 1][col]) {
            return false;
        }
        if (puzzle->v_cons[row][col] == SMALLER &&  //
            solution[row + 1][col] != EMPTY &&      //
            color >= solution[row + 1][col]) {
            return false;
        }
    }

    // Check for duplicates in row and column
    for (int i = 0; i < puzzle->size; i++) {
        if (i != col && solution[row][i] == color) return false;
        if (i != row && solution[i][col] == color) return false;
    }

    return true;
}

bool has_valid_neighbor(const Futoshiki* puzzle, int row, int col, int color, bool need_greater) {
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int neighbor_color = puzzle->pc_list[row][col][i];
        if ((need_greater && neighbor_color > color) ||  //
            (!need_greater && neighbor_color < color)) {
            return true;
        }
    }
    return false;
}

bool satisfies_inequalities(const Futoshiki* puzzle, int row, int col, int color) {
    // Check horizontal constraints
    if (col > 0) {
        switch (puzzle->h_cons[row][col - 1]) {
            case GREATER:  // Left > Current
                if (!has_valid_neighbor(puzzle, row, col - 1, color, true)) {
                    return false;
                }
                break;
            case SMALLER:  // Left < Current
                if (!has_valid_neighbor(puzzle, row, col - 1, color, false)) {
                    return false;
                }
                break;
            case NO_CONS:
                // Nothing to do
                break;
        }
    }

    if (col < puzzle->size - 1) {
        switch (puzzle->h_cons[row][col]) {
            case GREATER:  // Current > Right
                if (!has_valid_neighbor(puzzle, row, col + 1, color, false)) {
                    return false;
                }
                break;
            case SMALLER:  // Current < Right
                if (!has_valid_neighbor(puzzle, row, col + 1, color, true)) {
                    return false;
                }
                break;
            case NO_CONS:
                // Nothing to do
                break;
        }
    }

    // Check vertical constraints
    if (row > 0) {
        switch (puzzle->v_cons[row - 1][col]) {
            case GREATER:  // Upper > Current
                if (!has_valid_neighbor(puzzle, row - 1, col, color, true)) {
                    return false;
                }
                break;
            case SMALLER:  // Upper < Current
                if (!has_valid_neighbor(puzzle, row - 1, col, color, false)) {
                    return false;
                }
                break;
            case NO_CONS:
                // Nothing to do
                break;
        }
    }

    if (row < puzzle->size - 1) {
        switch (puzzle->v_cons[row][col]) {
            case GREATER:  // Current > Lower
                if (!has_valid_neighbor(puzzle, row + 1, col, color, false)) {
                    return false;
                }
                break;
            case SMALLER:  // Current < Lower
                if (!has_valid_neighbor(puzzle, row + 1, col, color, true)) {
                    return false;
                }
                break;
            case NO_CONS:
                // Nothing to do
                break;
        }
    }

    return true;
}

void filter_possible_colors(Futoshiki* puzzle, int row, int col) {
    if (puzzle->board[row][col] != EMPTY) {
        puzzle->pc_lengths[row][col] = 1;
        puzzle->pc_list[row][col][0] = puzzle->board[row][col];
        return;
    }

    int new_length = 0;
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (satisfies_inequalities(puzzle, row, col, color)) {
            puzzle->pc_list[row][col][new_length++] = color;
        }
    }
    puzzle->pc_lengths[row][col] = new_length;
}

void process_uniqueness(Futoshiki* puzzle, int row, int col) {
    if (puzzle->pc_lengths[row][col] == 1) {
        int color = puzzle->pc_list[row][col][0];
        for (int i = 0; i < puzzle->size; i++) {
            if (i != col) {  // Remove from row
                int new_length = 0;
                for (int j = 0; j < puzzle->pc_lengths[row][i]; j++) {
                    if (puzzle->pc_list[row][i][j] != color) {
                        puzzle->pc_list[row][i][new_length++] = puzzle->pc_list[row][i][j];
                    }
                }
                puzzle->pc_lengths[row][i] = new_length;
            }
            if (i != row) {  // Remove from column
                int new_length = 0;
                for (int j = 0; j < puzzle->pc_lengths[i][col]; j++) {
                    if (puzzle->pc_list[i][col][j] != color) {
                        puzzle->pc_list[i][col][new_length++] = puzzle->pc_list[i][col][j];
                    }
                }
                puzzle->pc_lengths[i][col] = new_length;
            }
        }
    }
}

int compute_pc_lists(Futoshiki* puzzle, bool use_precoloring) {
    print_progress("Starting pre-coloring");
    int total_colors_removed = 0;
    int initial_colors = 0;

    // Initialize pc_lists
    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            puzzle->pc_lengths[row][col] = 0;

            // Consider pre-set colors of the board
            if (puzzle->board[row][col] != EMPTY) {
                puzzle->pc_list[row][col][0] = puzzle->board[row][col];
                puzzle->pc_lengths[row][col] = 1;
                initial_colors += 1;  // Only count 1 since it's preset
                continue;
            }

            // Initialize with all possible colors
            for (int color = 1; color <= puzzle->size; color++) {
                puzzle->pc_list[row][col][puzzle->pc_lengths[row][col]++] = color;
            }
            initial_colors += puzzle->size;
        }
    }

    if (use_precoloring) {
        bool changes;
        do {
            changes = false;
            int old_lengths[MAX_N][MAX_N];
            memcpy(old_lengths, puzzle->pc_lengths, sizeof(old_lengths));

            // Process each cell
            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    int before_length = puzzle->pc_lengths[row][col];
                    filter_possible_colors(puzzle, row, col);
                    process_uniqueness(puzzle, row, col);
                    total_colors_removed += before_length - puzzle->pc_lengths[row][col];
                }
            }

            // Check for changes
            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    if (puzzle->pc_lengths[row][col] != old_lengths[row][col]) {
                        changes = true;
                    }
                }
            }
        } while (changes);
    }

    print_progress("Pre-coloring complete");
    return total_colors_removed;
}

// Sequential backtracking algorithm for deeper levels
bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
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

// Parallelization that creates tasks for first level choices
bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    print_progress("Starting parallel backtracking");

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

void print_board(const Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            printf(" %d ", solution[row][col]);
            if (col < puzzle->size - 1) {
                switch (puzzle->h_cons[row][col]) {
                    case GREATER:
                        printf(">");
                        break;
                    case SMALLER:
                        printf("<");
                        break;
                    default:
                        printf(" ");
                        break;
                }
            }
        }
        printf("\n");
        if (row < puzzle->size - 1) {
            for (int col = 0; col < puzzle->size; col++) {
                switch (puzzle->v_cons[row][col]) {
                    case GREATER:
                        printf(" v ");
                        break;
                    case SMALLER:
                        printf(" ^ ");
                        break;
                    default:
                        printf("   ");
                        break;
                }
                if (col < puzzle->size - 1) printf(" ");
            }
            printf("\n");
        }
    }
    printf("\n");
}

bool parse_futoshiki(const char* input, Futoshiki* puzzle) {
    print_progress("Parsing puzzle input");

    // Initialize everything to 0/NO_CONS
    memset(puzzle->board, 0, sizeof(puzzle->board));
    memset(puzzle->h_cons, NO_CONS, sizeof(puzzle->h_cons));
    memset(puzzle->v_cons, NO_CONS, sizeof(puzzle->v_cons));

    // First, determine size by counting numbers in first row
    char first_line[256];
    int line_len = 0;
    while (input[line_len] && input[line_len] != '\n') {
        first_line[line_len] = input[line_len];
        line_len++;
    }
    first_line[line_len] = '\0';

    // Count numbers in first line to determine size
    puzzle->size = 0;
    for (int i = 0; i < line_len; i++) {
        if (isdigit(first_line[i]) || first_line[i] == '0') {
            puzzle->size++;
        }
    }

    if (puzzle->size > MAX_N || puzzle->size == 0) {
        return false;
    }

    char line[256];
    const char* line_start = input;
    int number_row = 0;

    while (*line_start) {
        // Copy line to buffer
        line_len = 0;
        while (line_start[line_len] && line_start[line_len] != '\n') {
            line[line_len] = line_start[line_len];
            line_len++;
        }
        line[line_len] = '\0';

        // Skip empty lines
        if (line_len == 0 || line[0] == '\n') {
            line_start += (line_start[0] == '\n' ? 1 : 0);
            continue;
        }

        // Check if this is a constraint line
        bool is_v_constraint_line = false;
        for (int i = 0; i < line_len; i++) {
            if (line[i] == '^' || line[i] == 'v' || line[i] == 'V') {
                is_v_constraint_line = true;
                break;
            }
        }

        if (!is_v_constraint_line) {  // Number and horizontal constraint line
            int col = 0;
            for (int i = 0; i < line_len && col < puzzle->size; i++) {
                if (line[i] == ' ') continue;

                if (isdigit(line[i])) {
                    puzzle->board[number_row][col] = line[i] - '0';
                    col++;
                } else if (line[i] == '<' && col > 0) {
                    puzzle->h_cons[number_row][col - 1] = SMALLER;
                } else if (line[i] == '>' && col > 0) {
                    puzzle->h_cons[number_row][col - 1] = GREATER;
                }
            }
            number_row++;
        } else {  // Vertical constraint line
            // Create a mapping array for character positions to grid columns
            int col_positions[MAX_N] = {0};  // Position where each column's number would be
            int pos = 0;
            for (int col = 0; col < puzzle->size; col++) {
                col_positions[col] = pos;
                pos += (col == puzzle->size - 1) ? 0 : 4;  // 4 spaces between numbers
            }

            // Now scan the constraint line
            for (int i = 0; i < line_len; i++) {
                if (line[i] != '^' && line[i] != 'v' && line[i] != 'V') continue;

                // Find which column this constraint is closest to
                int col = 0;
                for (int j = 1; j < puzzle->size; j++) {
                    if (abs(i - col_positions[j]) < abs(i - col_positions[col])) {
                        col = j;
                    }
                }

                if (col < puzzle->size) {
                    puzzle->v_cons[number_row - 1][col] = (line[i] == '^') ? SMALLER : GREATER;
                }
            }
        }

        line_start += line_len + (line_start[line_len] == '\n' ? 1 : 0);
    }

    print_progress("Parsing complete");
    print_progress("Puzzle size: %d x %d", puzzle->size, puzzle->size);
    return true;
}

// File reading function
bool read_puzzle_from_file(const char* filename, Futoshiki* puzzle) {
    print_progress("Reading puzzle file");

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return false;
    }

    char buffer[1024] = {0};
    char content[1024] = {0};
    int total_read = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        strcat(content, buffer);
        total_read += strlen(buffer);
        if (total_read >= sizeof(content) - 1) {
            printf("Error: Puzzle file too large\n");
            fclose(file);
            return false;
        }
    }

    fclose(file);
    return parse_futoshiki(content, puzzle);
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

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
