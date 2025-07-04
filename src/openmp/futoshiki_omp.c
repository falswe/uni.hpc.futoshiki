#include "futoshiki_omp.h"

#include <omp.h>
#include <string.h>

// Structure to hold a partial solution
typedef struct {
    int depth;
    int assignments[MAX_N * 3];  // [row1, col1, color1, row2, col2, color2, ...]
} PartialSolution;

// Calculate appropriate depth for task generation
static int calculate_task_depth(Futoshiki* puzzle, int target_tasks) {
    print_progress("Calculating optimal task generation depth");

    // Find empty cells
    int empty_cells[MAX_N * MAX_N][2];
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

    if (num_empty == 0) return 0;

    // Set reasonable max depth based on puzzle size
    int max_depth = 5;
    if (puzzle->size > 9) max_depth = 4;
    if (puzzle->size > 15) max_depth = 3;

    // Estimate task count at each depth
    int depth = 1;
    long long estimated_tasks = 1;

    for (depth = 1; depth <= num_empty && depth <= max_depth; depth++) {
        // Rough estimate: assume average branching factor
        int avg_colors = 0;
        for (int i = 0; i < depth && i < num_empty; i++) {
            int r = empty_cells[i][0];
            int c = empty_cells[i][1];
            avg_colors += puzzle->pc_lengths[r][c];
        }
        if (depth > 0) avg_colors /= depth;

        estimated_tasks = 1;
        for (int i = 0; i < depth; i++) {
            estimated_tasks *= avg_colors;
            if (estimated_tasks > target_tasks * 10) break;  // Avoid overflow
        }

        print_progress("Depth %d: estimated %lld tasks", depth, estimated_tasks);

        if (estimated_tasks >= target_tasks) {
            break;
        }
    }

    print_progress("Selected depth: %d (targeting %d+ tasks)", depth, target_tasks);
    return depth;
}

// Generate partial solutions recursively
static void generate_partial_solutions(Futoshiki* puzzle, int solution[MAX_N][MAX_N],
                                       PartialSolution** solutions, int* count, int* capacity,
                                       int current_depth, int target_depth, int* assignments,
                                       int row, int col) {
    // Safety limit
    if (*count >= 10000) {
        print_progress("WARNING: Partial solution limit reached");
        return;
    }

    // Find next empty cell
    while (row < puzzle->size) {
        if (col >= puzzle->size) {
            row++;
            col = 0;
            continue;
        }
        if (puzzle->board[row][col] == EMPTY && solution[row][col] == EMPTY) {
            break;
        }
        col++;
    }

    // Check if we've reached target depth or end
    if (current_depth >= target_depth || row >= puzzle->size) {
        // Store this partial solution
        if (*count >= *capacity) {
            int new_capacity = *capacity * 2;
            if (new_capacity > 10000) new_capacity = 10000;

            PartialSolution* new_solutions =
                realloc(*solutions, new_capacity * sizeof(PartialSolution));
            if (!new_solutions) return;

            *solutions = new_solutions;
            *capacity = new_capacity;
        }

        PartialSolution* ps = &(*solutions)[*count];
        ps->depth = current_depth;
        memcpy(ps->assignments, assignments, current_depth * 3 * sizeof(int));
        (*count)++;
        return;
    }

    // Try each possible color
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];

        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            assignments[current_depth * 3] = row;
            assignments[current_depth * 3 + 1] = col;
            assignments[current_depth * 3 + 2] = color;

            generate_partial_solutions(puzzle, solution, solutions, count, capacity,
                                       current_depth + 1, target_depth, assignments, row, col + 1);

            solution[row][col] = EMPTY;
        }
    }
}

// Parallel solving with OpenMP tasks
static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting OpenMP parallel backtracking");

    bool found_solution = false;

    // Calculate optimal depth for task generation
    int num_threads = omp_get_max_threads();
    int target_tasks = num_threads * 4;  // Generate more tasks than threads for load balancing
    int depth = calculate_task_depth(puzzle, target_tasks);

    if (depth == 0) {
        // No empty cells or calculation failed
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

    // Generate partial solutions
    int capacity = target_tasks * 2;
    PartialSolution* partial_solutions = malloc(capacity * sizeof(PartialSolution));
    if (!partial_solutions) {
        print_progress("Failed to allocate memory for partial solutions");
        return false;
    }

    int num_solutions = 0;
    int temp_solution[MAX_N][MAX_N];
    memcpy(temp_solution, puzzle->board, sizeof(temp_solution));
    int assignments[MAX_N * 3];

    generate_partial_solutions(puzzle, temp_solution, &partial_solutions, &num_solutions, &capacity,
                               0, depth, assignments, 0, 0);

    print_progress("Generated %d partial solutions at depth %d", num_solutions, depth);

    if (num_solutions == 0) {
        free(partial_solutions);
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

// Process partial solutions in parallel
#pragma omp parallel
    {
#pragma omp single
        {
            print_progress("Processing %d tasks with %d threads", num_solutions,
                           omp_get_num_threads());

            for (int i = 0; i < num_solutions && !found_solution; i++) {
                PartialSolution* ps = &partial_solutions[i];

#pragma omp task firstprivate(i) shared(found_solution)
                {
                    if (!found_solution) {  // Early exit check
                        // Create local solution
                        int local_solution[MAX_N][MAX_N];
                        memcpy(local_solution, puzzle->board, sizeof(local_solution));

                        // Apply partial solution
                        for (int j = 0; j < ps->depth; j++) {
                            int r = ps->assignments[j * 3];
                            int c = ps->assignments[j * 3 + 1];
                            int color = ps->assignments[j * 3 + 2];
                            local_solution[r][c] = color;
                        }

                        // Find where to continue
                        int start_row = 0, start_col = 0;
                        if (ps->depth > 0) {
                            start_row = ps->assignments[(ps->depth - 1) * 3];
                            start_col = ps->assignments[(ps->depth - 1) * 3 + 1] + 1;
                        }

                        // Try to complete the solution
                        if (color_g_seq(puzzle, local_solution, start_row, start_col)) {
#pragma omp critical
                            {
                                if (!found_solution) {
                                    found_solution = true;
                                    memcpy(solution, local_solution, sizeof(local_solution));
                                    print_progress("Thread %d found solution from task %d",
                                                   omp_get_thread_num(), i);
                                }
                            }
                        }
                    }
                }
            }

#pragma omp taskwait
        }
    }

    free(partial_solutions);
    return found_solution;
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