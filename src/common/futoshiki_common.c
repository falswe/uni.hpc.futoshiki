#include "futoshiki_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <sys/time.h>

// Global variable for progress display
bool g_show_progress = false;

// For MPI compatibility - these will be defined by MPI implementation
__attribute__((weak)) int g_mpi_rank = 0;
__attribute__((weak)) int g_mpi_size = 1;

void set_progress_display(bool show) { g_show_progress = show; }

void print_progress(const char* format, ...) {
    if (!g_show_progress) return;

    // Only master process should print in MPI context
    if (g_mpi_rank != 0) return;

    va_list args;
    va_start(args, format);
    printf("[PROGRESS] ");
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}

void print_cell_colors(const Futoshiki* puzzle, int row, int col) {
    if (!g_show_progress || g_mpi_rank != 0) return;

    printf("[PROGRESS] Cell [%d][%d]: ", row, col);
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        printf("%d ", puzzle->pc_list[row][col][i]);
    }
    printf("\n");
}

double get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
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