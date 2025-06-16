/**
 * Improved parallel implementation of the Futoshiki solver
 *
 * Key improvements:
 * 1. Multi-level parallelization in backtracking
 * 2. Parallel pre-coloring phase
 * 3. Intelligent cell selection strategy
 * 4. Better code organization and documentation
 */

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
#define MAX_PARALLEL_DEPTH 3  // Maximum depth for parallel task creation

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

// Structure for representing a cell position
typedef struct {
    int row;
    int col;
} Cell;

static bool g_show_progress = false;

/**
 * Enable or disable progress display
 * @param show Whether to show progress messages
 */
void set_progress_display(bool show) { g_show_progress = show; }

/**
 * Print progress message if progress display is enabled
 * @param format printf-style format string
 * @param ... Additional arguments for format string
 */
static void print_progress(const char* format, ...) {
    if (!g_show_progress) return;

    va_list args;
    va_start(args, format);
    printf("[PROGRESS] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

/**
 * Print the colors available for a specific cell
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 */
static void print_cell_colors(const Futoshiki* puzzle, int row, int col) {
    if (!g_show_progress) return;

    printf("[PROGRESS] Cell [%d][%d]: ", row, col);
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        printf("%d ", puzzle->pc_list[row][col][i]);
    }
    printf("\n");
}

/**
 * Check if a color is safe to place in a cell based on constraints
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 * @param solution Current partial solution
 * @param color Color to check
 * @return true if the color is safe, false otherwise
 */
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

/**
 * Check if a cell has valid neighbors that satisfy inequality constraints
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 * @param color Color to check
 * @param need_greater Whether we need a neighbor with a greater value
 * @return true if a valid neighbor exists, false otherwise
 */
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

/**
 * Check if a color satisfies all inequality constraints with neighboring cells
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 * @param color Color to check
 * @return true if the color satisfies all constraints, false otherwise
 */
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

/**
 * Filter the possible colors for a cell based on constraints
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 */
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

/**
 * Process uniqueness constraints for a cell
 * If a cell has only one possible color, remove that color from row and column
 * @param puzzle The Futoshiki puzzle
 * @param row Row index
 * @param col Column index
 */
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

/**
 * Compute the possible color lists for each cell
 * This is the pre-coloring phase, which can be parallelized
 * @param puzzle The Futoshiki puzzle
 * @param use_precoloring Whether to use precoloring optimization
 * @return Number of colors removed in the pre-coloring phase
 */
int compute_pc_lists(Futoshiki* puzzle, bool use_precoloring) {
    print_progress("Starting pre-coloring");
    int total_colors_removed = 0;
    int initial_colors = 0;

// Initialize pc_lists
#pragma omp parallel for collapse(2) reduction(+ : initial_colors) if (use_precoloring)
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
        int iterations = 0;
        do {
            changes = false;
            int old_lengths[MAX_N][MAX_N];
            memcpy(old_lengths, puzzle->pc_lengths, sizeof(old_lengths));
            int colors_removed_this_iter = 0;

// Process each cell in parallel
#pragma omp parallel for collapse(2) reduction(+ : colors_removed_this_iter) schedule(dynamic)
            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    int before_length = puzzle->pc_lengths[row][col];

// We need to synchronize here because process_uniqueness
// affects other cells' color lists
#pragma omp critical
                    {
                        filter_possible_colors(puzzle, row, col);
                        process_uniqueness(puzzle, row, col);
                    }

                    colors_removed_this_iter += before_length - puzzle->pc_lengths[row][col];
                }
            }

            total_colors_removed += colors_removed_this_iter;
            iterations++;

            // Check for changes
            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    if (puzzle->pc_lengths[row][col] != old_lengths[row][col]) {
                        changes = true;
                        break;
                    }
                }
                if (changes) break;
            }

            print_progress("Pre-coloring iteration %d: removed %d colors", iterations,
                           colors_removed_this_iter);

        } while (changes);

        print_progress("Pre-coloring complete after %d iterations", iterations);
    }

    return total_colors_removed;
}

/**
 * Find the most constrained cell (with fewest possible colors)
 * @param puzzle The Futoshiki puzzle
 * @param solution Current partial solution
 * @param start_row Start row for search
 * @param start_col Start column for search
 * @return Position of the most constrained empty cell
 */
Cell find_most_constrained_cell(const Futoshiki* puzzle, int solution[MAX_N][MAX_N], int start_row,
                                int start_col) {
    Cell best_cell = {-1, -1};
    int min_colors = puzzle->size + 1;

    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            // Skip cells that are already filled
            if (solution[row][col] != EMPTY) continue;

            // Skip cells before the starting position
            if (row < start_row || (row == start_row && col < start_col)) continue;

            // Count actual valid colors for this cell
            int valid_colors = 0;
            for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
                int color = puzzle->pc_list[row][col][i];
                if (safe(puzzle, row, col, solution, color)) {
                    valid_colors++;
                }
            }

            // Update best cell if this one has fewer valid colors
            if (valid_colors > 0 && valid_colors < min_colors) {
                min_colors = valid_colors;
                best_cell.row = row;
                best_cell.col = col;
            }
        }
    }

    // If no cell was found, return (-1, -1)
    return best_cell;
}

/**
 * Sequential backtracking algorithm for deeper levels
 * @param puzzle The Futoshiki puzzle
 * @param solution Current partial solution
 * @param start_row Start row for search
 * @param start_col Start column for search
 * @return true if a solution is found, false otherwise
 */
bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int start_row, int start_col) {
    // Find the next empty cell (most constrained one)
    Cell next_cell = find_most_constrained_cell(puzzle, solution, start_row, start_col);

    // If no empty cell found, the puzzle is solved
    if (next_cell.row == -1) {
        return true;
    }

    int row = next_cell.row;
    int col = next_cell.col;

    // Try each possible color for the selected cell
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (color_g_seq(puzzle, solution, row, col)) {
                return true;
            }
            solution[row][col] = EMPTY;  // Backtrack
        }
    }

    return false;
}

/**
 * Recursive parallel backtracking with multi-level parallelism
 * @param puzzle The Futoshiki puzzle
 * @param solution Current partial solution
 * @param depth Current recursion depth
 * @param row Current row
 * @param col Current column
 * @return true if a solution is found, false otherwise
 */
bool color_g_parallel(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int depth, int row, int col) {
    // Find the next empty cell (most constrained one)
    Cell next_cell = find_most_constrained_cell(puzzle, solution, row, col);

    // If no empty cell found, the puzzle is solved
    if (next_cell.row == -1) {
        return true;
    }

    row = next_cell.row;
    col = next_cell.col;

    // Determine if we should continue parallel or switch to sequential
    // Based on depth and available threads
    bool use_parallel = (depth < MAX_PARALLEL_DEPTH && omp_get_num_threads() > 1 &&
                         puzzle->pc_lengths[row][col] > 1);

    bool found_solution = false;

    if (use_parallel) {
// Create tasks for each possible color
#pragma omp parallel
        {
#pragma omp single
            {
                for (int i = 0; i < puzzle->pc_lengths[row][col] && !found_solution; i++) {
                    int color = puzzle->pc_list[row][col][i];
                    if (safe(puzzle, row, col, solution, color)) {
#pragma omp task firstprivate(color, depth) shared(found_solution, solution)
                        {
                            // Create local copy of solution
                            int local_solution[MAX_N][MAX_N];
                            memcpy(local_solution, solution, sizeof(local_solution));

                            // Set color for this branch
                            local_solution[row][col] = color;

                            // Continue with next level of parallelism or sequential
                            bool success;
                            if (depth + 1 < MAX_PARALLEL_DEPTH) {
                                success =
                                    color_g_parallel(puzzle, local_solution, depth + 1, row, col);
                            } else {
                                success = color_g_seq(puzzle, local_solution, row, col);
                            }

                            // Update shared solution if successful
                            if (success) {
#pragma omp critical
                                {
                                    if (!found_solution) {
                                        found_solution = true;
                                        memcpy(solution, local_solution, sizeof(local_solution));
                                    }
                                }
                            }
                        }
                    }
                }

#pragma omp taskwait
            }
        }
    } else {
        // Sequential backtracking
        for (int i = 0; i < puzzle->pc_lengths[row][col] && !found_solution; i++) {
            int color = puzzle->pc_list[row][col][i];
            if (safe(puzzle, row, col, solution, color)) {
                solution[row][col] = color;
                if (color_g_seq(puzzle, solution, row, col)) {
                    found_solution = true;
                } else {
                    solution[row][col] = EMPTY;  // Backtrack
                }
            }
        }
    }

    return found_solution;
}

/**
 * Main solver function
 * @param puzzle The Futoshiki puzzle
 * @param solution Output solution array
 * @param row Starting row
 * @param col Starting column
 * @return true if a solution is found, false otherwise
 */
bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    print_progress("Starting parallel backtracking with multi-level parallelism");

    // Initialize solution with pre-filled values
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            if (puzzle->board[r][c] != EMPTY) {
                solution[r][c] = puzzle->board[r][c];
            } else {
                solution[r][c] = EMPTY;
            }
        }
    }

    // Start recursive parallel solving from depth 0
    bool found_solution = color_g_parallel(puzzle, solution, 0, 0, 0);

    print_progress("Backtracking %s",
                   found_solution ? "found a solution" : "did not find a solution");
    return found_solution;
}

/**
 * Print the puzzle board with constraints
 * @param puzzle The Futoshiki puzzle
 * @param solution Solution array
 */
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

/**
 * Parse a Futoshiki puzzle from a string
 * @param input Input string containing the puzzle
 * @param puzzle Output puzzle structure
 * @return true if parsing was successful, false otherwise
 */
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

/**
 * Read a puzzle from a file
 * @param filename Name of the input file
 * @param puzzle Output puzzle structure
 * @return true if reading was successful, false otherwise
 */
bool read_puzzle_from_file(const char* filename, Futoshiki* puzzle) {
    print_progress("Reading puzzle file");

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return false;
    }

    char buffer[1024] = {0};
    char content[4096] = {0};  // Increased buffer size for larger puzzles
    int total_read = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        int read_len = strlen(buffer);
        if (total_read + read_len >= sizeof(content) - 1) {
            printf("Error: Puzzle file too large\n");
            fclose(file);
            return false;
        }
        strcat(content, buffer);
        total_read += read_len;
    }

    fclose(file);
    return parse_futoshiki(content, puzzle);
}

/**
 * Get current time in seconds
 * @return Current time in seconds
 */
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

/**
 * Solve a Futoshiki puzzle
 * @param filename Name of the input file
 * @param use_precoloring Whether to use precoloring optimization
 * @param print_solution Whether to print the solution
 * @return Statistics about the solving process
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