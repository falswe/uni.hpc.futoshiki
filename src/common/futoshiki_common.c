#include "futoshiki_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <sys/time.h>

// For MPI compatibility - these will be defined by MPI implementation
__attribute__((weak)) int g_mpi_rank = 0;
__attribute__((weak)) int g_mpi_size = 1;

double get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int get_target_tasks(int num_workers, double factor, const char* impl_name) {
    if (num_workers <= 0) {
        // This can happen if g_mpi_size is 1, so num_workers is 0.
        // Or if omp_get_max_threads() returns 0 or 1.
        num_workers = 1;
    }

    int target_tasks = (int)(num_workers * factor);

    // Sanity check: ensure we generate at least one task per worker if the factor is >= 1.0,
    // or at least one total task if the factor is < 1.0.
    if (target_tasks < num_workers && factor >= 1.0) {
        target_tasks = num_workers;
    }
    if (target_tasks < 1) {
        target_tasks = 1;
    }

    log_info("%s task generation strategy: target = %d workers * %.2f factor = %d tasks", impl_name,
             num_workers, factor, target_tasks);

    return target_tasks;
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
    log_verbose("Starting pre-coloring phase.");
    int initial_colors = 0;

    // Initialize pc_lists
    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            puzzle->pc_lengths[row][col] = 0;

            if (puzzle->board[row][col] != EMPTY) {
                puzzle->pc_list[row][col][0] = puzzle->board[row][col];
                puzzle->pc_lengths[row][col] = 1;
                initial_colors += 1;
            } else {
                for (int color = 1; color <= puzzle->size; color++) {
                    puzzle->pc_list[row][col][puzzle->pc_lengths[row][col]++] = color;
                }
                initial_colors += puzzle->size;
            }
        }
    }

    if (use_precoloring) {
        bool changes;
        do {
            changes = false;
            int old_lengths[MAX_N][MAX_N];
            memcpy(old_lengths, puzzle->pc_lengths, sizeof(old_lengths));

            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    filter_possible_colors(puzzle, row, col);
                    process_uniqueness(puzzle, row, col);
                }
            }

            for (int row = 0; row < puzzle->size; row++) {
                for (int col = 0; col < puzzle->size; col++) {
                    if (puzzle->pc_lengths[row][col] != old_lengths[row][col]) {
                        changes = true;
                    }
                }
            }
        } while (changes);
    }

    int final_colors = 0;
    for (int row = 0; row < puzzle->size; row++) {
        for (int col = 0; col < puzzle->size; col++) {
            final_colors += puzzle->pc_lengths[row][col];
        }
    }

    log_verbose("Pre-coloring phase complete.");
    return initial_colors - final_colors;
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
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    if (solution != NULL && !found_empty) {
        for (int r = 0; r < puzzle->size; r++) {
            for (int c = 0; c < puzzle->size; c++) {
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    return found_empty;
}

bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    if (row >= puzzle->size) {
        return true;
    }

    if (col >= puzzle->size) {
        return color_g_seq(puzzle, solution, row + 1, 0);
    }

    if (puzzle->board[row][col] != EMPTY) {
        solution[row][col] = puzzle->board[row][col];
        return color_g_seq(puzzle, solution, row, col + 1);
    }

    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (color_g_seq(puzzle, solution, row, col + 1)) {
                return true;
            }
            solution[row][col] = EMPTY;
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
    log_verbose("Parsing puzzle input from string.");

    memset(puzzle, 0, sizeof(Futoshiki));
    int last_num_relative_positions[MAX_N] = {0};
    int size = 0;

    const char* cursor = input;
    int board_row = 0;

    while (*cursor != '\0' && board_row < MAX_N) {
        const char* line_start = cursor;
        const char* line_end = line_start;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        int line_len = line_end - line_start;

        cursor = line_end;
        if (*cursor == '\r') cursor++;
        if (*cursor == '\n') cursor++;

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
        log_error("Puzzle input appears to be empty or invalid.");
        return false;
    }
    return true;
}

bool read_puzzle_from_file(const char* filename, Futoshiki* puzzle) {
    log_verbose("Reading puzzle file: %s", filename);

    FILE* file = fopen(filename, "r");
    if (!file) {
        log_error("Could not open file '%s'", filename);
        return false;
    }
    char buffer[4096];
    char content[16384] = {0};
    size_t total_read = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        if (total_read + strlen(buffer) >= sizeof(content)) {
            log_error("Puzzle file '%s' is too large (max %zu bytes)", filename, sizeof(content));
            fclose(file);
            return false;
        }
        strcat(content, buffer);
        total_read += strlen(buffer);
    }
    fclose(file);
    return parse_futoshiki(content, puzzle);
}