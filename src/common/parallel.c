#include "parallel_work_distribution.h"

#include <stdio.h>
#include <string.h>

int find_empty_cells(const Futoshiki* puzzle, int empty_cells[MAX_N * MAX_N][2]) {
    int num_empty = 0;
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            if (puzzle->board[r][c] == EMPTY) {
                empty_cells[num_empty][0] = r;
                empty_cells[num_empty][1] = c;
                num_empty++;
            }
        }
    }
    return num_empty;
}

long long count_valid_assignments_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N],
                                            int empty_cells[MAX_N * MAX_N][2], int num_empty_cells,
                                            int current_cell_idx, int target_depth) {
    if (current_cell_idx >= target_depth) {
        return 1;
    }
    if (current_cell_idx >= num_empty_cells) {
        return 1;
    }

    long long count = 0;
    int row = empty_cells[current_cell_idx][0];
    int col = empty_cells[current_cell_idx][1];

    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            count += count_valid_assignments_recursive(
                puzzle, solution, empty_cells, num_empty_cells, current_cell_idx + 1, target_depth);
            solution[row][col] = EMPTY;
        }
    }
    return count;
}

int calculate_distribution_depth(Futoshiki* puzzle, int num_workers) {
    double start_time = get_time();
    if (num_workers <= 0) {
        return 0;
    }

    int empty_cells[MAX_N * MAX_N][2];
    int num_empty = find_empty_cells(puzzle, empty_cells);

    if (num_empty == 0) {
        log_verbose("Puzzle has no empty cells; no work to distribute.");
        return 0;
    }

    log_verbose("Work Distribution Strategy:");
    log_verbose("  - Target: >%d work units.", num_workers);

    int chosen_depth = 0;
    long long job_count = 0;

    int temp_solution[MAX_N][MAX_N];
    memcpy(temp_solution, puzzle->board, sizeof(temp_solution));

    for (int d = 1; d <= num_empty; d++) {
        memcpy(temp_solution, puzzle->board, sizeof(temp_solution));
        job_count =
            count_valid_assignments_recursive(puzzle, temp_solution, empty_cells, num_empty, 0, d);
        log_verbose("  - Depth %d: %lld valid work units.", d, job_count);
        chosen_depth = d;

        if (job_count > num_workers) {
            log_verbose("  - Depth %d is sufficient.", chosen_depth);
            break;
        }
        if (d == num_empty && job_count <= num_workers) {
            log_verbose("  - Reached max possible depth (%d), using all %lld work units.", d,
                        job_count);
        }
    }

    if (job_count == 0 && num_empty > 0) {
        log_warn("No valid work units could be generated. Puzzle might be unsolvable.");
    }

    double end_time = get_time();
    log_verbose("Depth calculation took %.6f seconds.", end_time - start_time);
    log_info("Chosen depth: %d (will generate %lld work units)", chosen_depth, job_count);

    return chosen_depth;
}

void generate_work_units_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N], WorkUnit** units,
                                   int* unit_count, int* capacity, int current_depth,
                                   int target_depth, int* assignments, int row, int col) {
    if (*unit_count >= 100000) {
        if (*unit_count == 100000) log_warn("Work unit limit reached (%d units)", *unit_count);
        return;
    }

    while (row < puzzle->size) {
        if (col >= puzzle->size) {
            row++;
            col = 0;
            continue;
        }
        if (puzzle->board[row][col] == EMPTY && solution[row][col] == EMPTY) break;
        col++;
    }

    if (current_depth >= target_depth || row >= puzzle->size) {
        if (*unit_count >= *capacity) {
            int new_capacity = *capacity * 2;
            if (new_capacity > 100000) new_capacity = 100000;
            if (new_capacity <= *capacity) return;

            WorkUnit* new_units = realloc(*units, new_capacity * sizeof(WorkUnit));
            if (!new_units) {
                log_warn("Failed to expand work unit array");
                return;
            }
            *units = new_units;
            *capacity = new_capacity;
        }
        WorkUnit* unit = &(*units)[*unit_count];
        unit->depth = current_depth;
        memcpy(unit->assignments, assignments, current_depth * 3 * sizeof(int));
        (*unit_count)++;
        return;
    }

    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            assignments[current_depth * 3] = row;
            assignments[current_depth * 3 + 1] = col;
            assignments[current_depth * 3 + 2] = color;
            generate_work_units_recursive(puzzle, solution, units, unit_count, capacity,
                                          current_depth + 1, target_depth, assignments, row,
                                          col + 1);
            solution[row][col] = EMPTY;
        }
    }
}

WorkUnit* generate_work_units(Futoshiki* puzzle, int depth, int* num_units) {
    int capacity = 100;
    if (g_mpi_size > 1)
        capacity = (g_mpi_size - 1) * 4;
    else
        capacity = 64;
    if (capacity > 1000) capacity = 1000;

    WorkUnit* units = malloc(capacity * sizeof(WorkUnit));
    if (!units) {
        log_error("Failed to allocate memory for work units");
        *num_units = 0;
        return NULL;
    }
    *num_units = 0;
    int solution[MAX_N][MAX_N];
    memcpy(solution, puzzle->board, sizeof(solution));
    int assignments[MAX_N * 3];
    generate_work_units_recursive(puzzle, solution, &units, num_units, &capacity, 0, depth,
                                  assignments, 0, 0);
    log_info("Generated %d work units at depth %d", *num_units, depth);
    if (*num_units > 0 && *num_units < capacity) {
        WorkUnit* shrunk = realloc(units, *num_units * sizeof(WorkUnit));
        if (shrunk) units = shrunk;
    }
    return units;
}

void apply_work_unit(const Futoshiki* puzzle, const WorkUnit* work_unit,
                     int solution[MAX_N][MAX_N]) {
    memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
    for (int i = 0; i < work_unit->depth; i++) {
        int row = work_unit->assignments[i * 3];
        int col = work_unit->assignments[i * 3 + 1];
        int color = work_unit->assignments[i * 3 + 2];
        solution[row][col] = color;
    }
}

void get_continuation_point(const WorkUnit* work_unit, int* start_row, int* start_col) {
    if (work_unit->depth > 0) {
        *start_row = work_unit->assignments[(work_unit->depth - 1) * 3];
        *start_col = work_unit->assignments[(work_unit->depth - 1) * 3 + 1] + 1;
    } else {
        *start_row = 0;
        *start_col = 0;
    }
}

void print_work_unit(const WorkUnit* work_unit, int unit_number) {
    const int max_assignments_str_len = 128;
    char assignments_str[max_assignments_str_len];
    int offset = 0;
    assignments_str[0] = '\0';
    for (int i = 0; i < work_unit->depth; i++) {
        int remaining_space = max_assignments_str_len - offset;
        int chars_written = snprintf(
            assignments_str + offset, remaining_space, " (%d,%d,%d)", work_unit->assignments[i * 3],
            work_unit->assignments[i * 3 + 1], work_unit->assignments[i * 3 + 2]);
        if (chars_written <= 0 || chars_written >= remaining_space) {
            strncat(assignments_str, "...", 3);
            break;
        }
        offset += chars_written;
    }
    log_debug("Work unit %d: depth=%d, assignments=%s", unit_number, work_unit->depth,
              assignments_str);
}