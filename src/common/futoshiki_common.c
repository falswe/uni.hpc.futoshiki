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

bool find_first_empty_cell(const Futoshiki* puzzle, int solution[MAX_N][MAX_N], int* row,
                           int* col) {
    bool found_empty = false;

    for (int r = 0; r < puzzle->size && !found_empty; r++) {
        for (int c = 0; c < puzzle->size && !found_empty; c++) {
            if (puzzle->board[r][c] == EMPTY) {
                *row = r;
                *col = c;
                found_empty = true;
            } else if (solution != NULL) {
                // Initialize solution with board values as we go
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    // If solution matrix provided, copy remaining board values
    if (solution != NULL && !found_empty) {
        // Complete the copy if no empty cell was found
        for (int r = 0; r < puzzle->size; r++) {
            for (int c = 0; c < puzzle->size; c++) {
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    return found_empty;
}

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

void print_board(const Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            printf("%2d", solution[row][col]);

            if (col < puzzle->size - 1) {
                switch (puzzle->h_cons[row][col]) {
                    case GREATER:
                        printf(" > ");
                        break;
                    case SMALLER:
                        printf(" < ");
                        break;
                    default:
                        printf("   ");
                        break;
                }
            }
        }
        printf("\n");

        if (row < puzzle->size - 1) {
            for (int col = 0; col < puzzle->size; col++) {
                printf(" ");

                switch (puzzle->v_cons[row][col]) {
                    case GREATER:
                        printf("v");
                        break;
                    case SMALLER:
                        printf("^");
                        break;
                    default:
                        printf(" ");
                        break;
                }

                if (col < puzzle->size - 1) {
                    printf("   ");
                }
            }
            printf("\n");
        }
    }
    printf("\n");
}

bool parse_futoshiki(const char* input, Futoshiki* puzzle) {
    print_progress("Parsing puzzle input");

    memset(puzzle, 0, sizeof(Futoshiki));
    int last_num_relative_positions[MAX_N] = {0};
    int size = 0;

    const char* cursor = input;
    int board_row = 0;

    while (*cursor != '\0' && board_row < MAX_N) {
        // 1. Mark the beginning of the current line, including any whitespace.
        const char* line_start = cursor;

        // 2. Find the end of the line's content (before newline characters).
        const char* line_end = line_start;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        int line_len = line_end - line_start;

        // 3. Advance the main cursor past this line and its newline characters for the next
        // iteration.
        cursor = line_end;
        if (*cursor == '\r') cursor++;
        if (*cursor == '\n') cursor++;

        // 4. Check if the line we just identified is entirely blank. If so, skip it.
        bool is_blank = true;
        for (int i = 0; i < line_len; i++) {
            if (!isspace((unsigned char)line_start[i])) {
                is_blank = false;
                break;
            }
        }
        if (is_blank) {
            continue;  // Go to the next line.
        }

        bool has_digits = false;
        bool has_v_constraints = false;

        for (int i = 0; i < line_len; i++) {
            if (isdigit((unsigned char)line_start[i])) {
                has_digits = true;
            } else if (line_start[i] == 'v' || line_start[i] == 'V' || line_start[i] == '^') {
                has_v_constraints = true;
            }
        }

        if (has_digits) {
            // --- This is a NUMBER row ---
            if (size == 0) {
                const char* scan_p = line_start;
                int temp_size = 0;
                while ((scan_p - line_start) < line_len && temp_size < MAX_N) {
                    if (isdigit((unsigned char)*scan_p)) {
                        temp_size++;
                        while ((scan_p - line_start) < line_len && isdigit((unsigned char)*scan_p))
                            scan_p++;
                    } else {
                        scan_p++;
                    }
                }
                size = temp_size;
                puzzle->size = size;
                if (size == 0) return false;
            }

            int board_col = 0;
            const char* p = line_start;

            while ((p - line_start) < line_len && board_col < size) {
                while ((p - line_start) < line_len && !isdigit((unsigned char)*p)) p++;
                if ((p - line_start) >= line_len) break;

                last_num_relative_positions[board_col] = p - line_start;

                char* next_p = NULL;
                puzzle->board[board_row][board_col] = strtol(p, &next_p, 10);
                p = next_p;
                if (board_col < size - 1) {
                    const char* constraint_p = p;
                    while ((constraint_p - line_start) < line_len &&
                           isspace((unsigned char)*constraint_p))
                        constraint_p++;

                    if ((constraint_p - line_start) < line_len) {
                        if (*constraint_p == '>') {
                            puzzle->h_cons[board_row][board_col] = GREATER;
                        } else if (*constraint_p == '<') {
                            puzzle->h_cons[board_row][board_col] = SMALLER;
                        }
                    }
                }
                board_col++;
            }
            board_row++;

        } else if (has_v_constraints && board_row > 0) {
            // --- This is a VERTICAL constraint row ---
            for (int i = 0; i < line_len; i++) {
                char v_con_char = line_start[i];
                if (v_con_char != 'v' && v_con_char != 'V' && v_con_char != '^') {
                    continue;
                }

                int constraint_relative_pos = i;

                int best_col = 0;
                int min_dist = abs(constraint_relative_pos - last_num_relative_positions[0]);

                for (int c = 1; c < size; c++) {
                    int dist = abs(constraint_relative_pos - last_num_relative_positions[c]);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_col = c;
                    }
                }

                if (best_col >= 0) {
                    if (v_con_char == 'v' || v_con_char == 'V') {
                        puzzle->v_cons[board_row - 1][best_col] = GREATER;
                    } else if (v_con_char == '^') {
                        puzzle->v_cons[board_row - 1][best_col] = SMALLER;
                    }
                }
            }
        }
    }

    if (size == 0) {
        printf("Error: Puzzle input appears to be empty or invalid.\n");
        return false;
    }

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