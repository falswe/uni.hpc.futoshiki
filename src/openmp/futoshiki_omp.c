#include "futoshiki_omp.h"

#include <omp.h>
#include <string.h>

#include "../common/parallel_work_distribution.h"

static double g_omp_task_factor = 1.0;

void omp_set_task_factor(double factor) {
    if (factor > 0) {
        g_omp_task_factor = factor;
    }
}

static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    bool found_solution = false;

    int num_threads = omp_get_max_threads();
    int target_tasks = get_target_tasks(num_threads, g_omp_task_factor, "OpenMP");
    int depth = calculate_distribution_depth(puzzle, target_tasks);

    if (depth == 0) {
        log_info("Falling back to sequential solver (no work units generated).");
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

    int num_work_units;
    WorkUnit* work_units = generate_work_units(puzzle, depth, &num_work_units);

    if (!work_units || num_work_units == 0) {
        log_info("Falling back to sequential solver (no work units generated).");
        if (work_units) free(work_units);
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

#pragma omp parallel
    {
#pragma omp single
        {
            log_verbose("Processing %d tasks with %d threads.", num_work_units,
                        omp_get_num_threads());

            for (int i = 0; i < num_work_units && !found_solution; i++) {
                WorkUnit* wu = &work_units[i];

                log_verbose("Before firstprivate: Thread %d processing work unit %d",
                            omp_get_thread_num(), i);

#pragma omp task firstprivate(i) shared(found_solution)
                {
                    log_verbose("After firstprivate: Thread %d processing work unit %d",
                                omp_get_thread_num(), i);

                    if (!found_solution) {
                        int local_solution[MAX_N][MAX_N];
                        apply_work_unit(puzzle, wu, local_solution);
                        int start_row, start_col;
                        get_continuation_point(wu, &start_row, &start_col);

                        if (color_g_seq(puzzle, local_solution, start_row, start_col)) {
#pragma omp critical
                            {
                                if (!found_solution) {
                                    found_solution = true;
                                    memcpy(solution, local_solution, sizeof(local_solution));
                                    log_verbose("Thread %d found solution from task %d.",
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

    free(work_units);
    return found_solution;
}

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
    stats.found_solution = color_g(&puzzle, solution);
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