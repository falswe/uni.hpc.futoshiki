#include "hybrid.h"

#include <mpi.h>  // Referencing external library
#include <omp.h>  // Referencing external library

#include "../common/parallel.h"

static double g_mpi_task_factor = 1.0;

typedef enum {
    TAG_WORK_REQUEST = 1,
    TAG_SOLUTION_FOUND = 2,
    TAG_SOLUTION_DATA = 3,
    TAG_TERMINATE = 4,
    TAG_WORK_ASSIGNMENT = 5
} MessageTag;

void hybrid_set_mpi_task_factor(double factor) {
    if (factor > 0) {
        g_mpi_task_factor = factor;
    }
}

static void hybrid_worker(Futoshiki* puzzle) {
    WorkUnit work_unit;
    MPI_Status status;

    while (true) {
        int request = 1;
        MPI_Send(&request, 1, MPI_INT, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);
        MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            log_verbose("Worker %d received termination signal.", g_mpi_rank);
            break;
        }

        int local_solution[MAX_N][MAX_N];
        Futoshiki sub_puzzle;
        memcpy(&sub_puzzle, puzzle, sizeof(Futoshiki));
        apply_work_unit(&sub_puzzle, &work_unit, sub_puzzle.board);

        if (omp_solve(&sub_puzzle, local_solution)) {
            // Found a solution, notify master and send it.
            int found_flag = 1;
            MPI_Send(&found_flag, 1, MPI_INT, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(local_solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // Wait for final termination signal
            MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, TAG_TERMINATE, MPI_COMM_WORLD,
                     &status);
            break;
        }
    }
}

static bool hybrid_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    int num_workers = g_mpi_size > 1 ? g_mpi_size - 1 : 1;
    int target_tasks = get_target_tasks(num_workers, g_mpi_task_factor, "MPI (Master)");
    int depth = calculate_distribution_depth(puzzle, target_tasks);
    int num_units;
    WorkUnit* work_units = generate_work_units(puzzle, depth, &num_units);

    if (!work_units || num_units == 0) {
        log_info("No MPI work units generated - falling back to OpenMP.");
        if (work_units) free(work_units);
        return omp_solve(puzzle, solution);
    }

    log_verbose("Master distributing %d work units to %d workers.", num_units, num_workers);
    int next_unit = 0;
    bool found_solution = false;
    int active_workers = num_workers;
    WorkUnit dummy_unit = {0};

    while (active_workers > 0) {
        MPI_Status status;
        int flag;
        MPI_Recv(&flag, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int worker_rank = status.MPI_SOURCE;

        if (status.MPI_TAG == TAG_SOLUTION_FOUND) {
            if (!found_solution) {
                found_solution = true;
                MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                log_verbose("Master received solution from worker %d. Shutting down.", worker_rank);
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
            } else {  // Another worker found a solution, but we already have one. Just terminate
                      // it.
                int temp_sol[MAX_N * MAX_N];
                MPI_Recv(&temp_sol, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
            }
        } else if (status.MPI_TAG == TAG_WORK_REQUEST) {
            if (found_solution || next_unit >= num_units) {
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
                log_verbose("Terminating worker %d (%s). %d workers left.", worker_rank,
                            found_solution ? "solution found by other" : "no more work",
                            active_workers);
            } else {
                MPI_Send(&work_units[next_unit], sizeof(WorkUnit), MPI_BYTE, worker_rank,
                         TAG_WORK_ASSIGNMENT, MPI_COMM_WORLD);
                log_verbose("Assigned work unit %d/%d to worker %d", next_unit + 1, num_units,
                            worker_rank);
                print_work_unit(&work_units[next_unit], next_unit + 1);
                next_unit++;
            }
        }
    }

    free(work_units);
    return found_solution;
}

static bool hybrid_solve(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    if (g_mpi_size == 1) {
        log_info("Only 1 MPI process, solving with OpenMP.");
        return omp_solve(puzzle, solution);
    }

    if (g_mpi_rank == 0) {
        return hybrid_master(puzzle, solution);
    } else {
        hybrid_worker(puzzle);
        return false;
    }
}

SolverStats hybrid_solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    if (g_mpi_rank == 0) {
        bool success = read_puzzle_from_file(filename, &puzzle);
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) return stats;
    } else {
        bool success;
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) return stats;
    }

    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    double start_precolor = get_time();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = get_time() - start_precolor;

    if (g_mpi_rank == 0) {
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
    }

    int solution[MAX_N][MAX_N] = {{0}};
    double start_coloring = MPI_Wtime();
    bool found = hybrid_solve(&puzzle, solution);
    stats.coloring_time = MPI_Wtime() - start_coloring;

    if (g_mpi_rank == 0) {
        stats.found_solution = found;
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
    } else {
        stats.found_solution = false;  // Only master reports stats
    }

    return stats;
}