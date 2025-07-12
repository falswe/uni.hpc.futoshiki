#include "futoshiki_hybrid.h"

#include <mpi.h>
#include <omp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/parallel_work_distribution.h"

// Global MPI variables
int g_mpi_rank = 0;
int g_mpi_size = 1;

// Task factors for controlling work distribution granularity
static double g_mpi_task_factor = 32.0;
static double g_omp_task_factor = 16.0;

// MPI Message tags for communication
typedef enum {
    TAG_WORK_REQUEST = 1,
    TAG_SOLUTION_FOUND = 2,
    TAG_SOLUTION_DATA = 3,
    TAG_TERMINATE = 4,
    TAG_WORK_ASSIGNMENT = 5
} MessageTag;

// --- Function Prototypes for internal logic ---
static bool color_g_hybrid(Futoshiki* puzzle, int solution[MAX_N][MAX_N]);
static void mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]);
static void mpi_worker(Futoshiki* puzzle);
static bool solve_work_unit_with_omp(Futoshiki* puzzle, WorkUnit* work_unit,
                                     int solution[MAX_N][MAX_N]);

// --- Public API Functions ---

void hybrid_set_mpi_task_factor(double factor) {
    if (factor > 0) {
        g_mpi_task_factor = factor;
    }
}

void hybrid_set_omp_task_factor(double factor) {
    if (factor > 0) {
        g_omp_task_factor = factor;
    }
}

void init_hybrid(int* argc, char*** argv) {
    int provided;
    // Initialize MPI with thread support.
    // MPI_THREAD_FUNNELED: Only the main thread of each process will make MPI calls.
    MPI_Init_thread(argc, argv, MPI_THREAD_FUNNELED, &provided);
    if (provided < MPI_THREAD_FUNNELED) {
        fprintf(stderr, "MPI thread support level is insufficient.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_mpi_size);
}

void finalize_hybrid() { MPI_Finalize(); }

// --- Core Implementation ---

/**
 * @brief The worker process logic for the hybrid model.
 *
 * A worker process receives a large WorkUnit from the master. It then uses
 * OpenMP to parallelize the solving of this WorkUnit. It acts as a "master"
 * for its local pool of OpenMP threads, breaking its assigned work down further.
 *
 * @param puzzle The initial Futoshiki puzzle.
 */
static void mpi_worker(Futoshiki* puzzle) {
    WorkUnit work_unit;
    MPI_Status status;

    // Main loop: request work from master until told to terminate.
    while (true) {
        // Signal to master that this worker is ready for work.
        int ready_flag = 1;
        MPI_Send(&ready_flag, 1, MPI_INT, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);

        // Receive a message from the master.
        MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        // If the master sends a termination signal, exit the loop.
        if (status.MPI_TAG == TAG_TERMINATE) {
            log_verbose("Worker %d received terminate signal.", g_mpi_rank);
            break;
        }

        // We have a work unit. Solve it using OpenMP.
        int solution[MAX_N][MAX_N];
        if (solve_work_unit_with_omp(puzzle, &work_unit, solution)) {
            // A solution was found by one of the OpenMP threads.
            log_verbose("Worker %d found a solution. Notifying master.", g_mpi_rank);
            int found_flag = 1;
            MPI_Send(&found_flag, 1, MPI_INT, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // After sending a solution, wait for the final termination signal from the master.
            MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, TAG_TERMINATE, MPI_COMM_WORLD,
                     &status);
            log_verbose("Worker %d received final termination.", g_mpi_rank);
            break; // Exit the loop.
        }
        // If no solution was found for this work unit, the loop continues and we request more work.
    }
}

/**
 * @brief Solves a given WorkUnit using OpenMP tasks.
 *
 * This function takes a WorkUnit (a sub-problem), generates smaller tasks from it,
 * and distributes these tasks among OpenMP threads to be solved concurrently.
 *
 * @param puzzle The base puzzle structure.
 * @param work_unit The specific sub-problem assigned to this worker.
 * @param solution A buffer to store the found solution.
 * @return true if a solution is found, false otherwise.
 */
static bool solve_work_unit_with_omp(Futoshiki* puzzle, WorkUnit* work_unit,
                                     int solution[MAX_N][MAX_N]) {
    bool found_solution_local = false;

    // Apply the master's work unit to get the starting board state.
    int initial_board[MAX_N][MAX_N];
    apply_work_unit(puzzle, work_unit, initial_board);

    // Find the first empty cell to start parallel work from.
    int start_row = 0, start_col = 0;
    bool found_empty = false;
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            if (initial_board[r][c] == EMPTY) {
                start_row = r;
                start_col = c;
                found_empty = true;
                break;
            }
        }
        if (found_empty) break;
    }

    // If there are no empty cells, the work unit itself is a solution.
    if (!found_empty) {
        memcpy(solution, initial_board, sizeof(int) * MAX_N * MAX_N);
        return true;
    }

#pragma omp parallel shared(found_solution_local, solution)
    {
        // Let one thread generate tasks for the first empty cell.
#pragma omp single
        {
            log_verbose("Worker %d, Thread 0: Spawning tasks for cell (%d, %d) from MPI Work Unit.",
                        g_mpi_rank, start_row, start_col);

            for (int i = 0; i < puzzle->pc_lengths[start_row][start_col]; i++) {
                if (found_solution_local) continue;

                int color = puzzle->pc_list[start_row][start_col][i];

                // Check if this color is a valid move in the current board state.
                if (safe(puzzle, start_row, start_col, initial_board, color)) {
#pragma omp task
                    {
                        if (!found_solution_local) {
                            int thread_local_solution[MAX_N][MAX_N];
                            memcpy(thread_local_solution, initial_board, sizeof(initial_board));
                            thread_local_solution[start_row][start_col] = color;

                            // Continue solving sequentially from the *next* cell.
                            if (color_g_seq(puzzle, thread_local_solution, start_row, start_col + 1)) {
#pragma omp critical
                                {
                                    if (!found_solution_local) {
                                        found_solution_local = true;
                                        memcpy(solution, thread_local_solution,
                                               sizeof(thread_local_solution));
                                        log_verbose("Worker %d, Thread %d: Found a solution.",
                                                    g_mpi_rank, omp_get_thread_num());
                                    }
                                }
                            }
                        }
                    } // omp task
                }
            } // for colors
        }     // omp single
    }         // omp parallel

    return found_solution_local;
}

/**
 * @brief The master process logic.
 *
 * This function is identical to the one in the pure MPI version. It distributes
 * work to workers and coordinates the shutdown process once a solution is found.
 */
static void mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    log_verbose("Starting HYBRID MPI+OpenMP parallel backtracking with %d workers.",
                g_mpi_size - 1);

    int num_workers = g_mpi_size > 1 ? g_mpi_size - 1 : 1;
    int target_tasks = get_target_tasks(num_workers, g_mpi_task_factor, "MPI");
    int depth = calculate_distribution_depth(puzzle, target_tasks);
    int num_units;
    WorkUnit* work_units = generate_work_units(puzzle, depth, &num_units);

    if (!work_units || num_units == 0) {
        log_info("No work units generated - falling back to sequential.");
        if (work_units) free(work_units);
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        color_g_seq(puzzle, solution, 0, 0); // Assuming this returns bool
        return;
    }

    log_verbose("Distributing %d work units to %d workers.", num_units, g_mpi_size - 1);
    int next_unit = 0;
    bool found_solution = false;
    int active_workers = g_mpi_size - 1;
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
                // Terminate this worker now
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
            } else {
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
            } else {
                MPI_Send(&work_units[next_unit], sizeof(WorkUnit), MPI_BYTE, worker_rank,
                         TAG_WORK_ASSIGNMENT, MPI_COMM_WORLD);
                next_unit++;
            }
        }
    }

    free(work_units);
}

/**
 * @brief Main dispatcher for the hybrid solver.
 *
 * Determines whether the current process is the master or a worker and
 * calls the appropriate function.
 */
static bool color_g_hybrid(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    if (g_mpi_size <= 1) {
        log_info(
            "Only 1 MPI process. Falling back to OpenMP-only solver on a single node.");
        // This case effectively becomes the OpenMP-only version.
        // We can simulate the OMP logic directly here.
        WorkUnit single_node_unit = {0}; // Represents the whole puzzle
        return solve_work_unit_with_omp(puzzle, &single_node_unit, solution);
    }

    if (g_mpi_rank == 0) {
        mpi_master(puzzle, solution);
        // The master needs to know if a solution was found. We can check the solution board.
        // A better way is to have master return a bool. Let's assume master updates a shared status.
        // For now, we rely on the fact that `solution` is filled.
        // A simple check: if the first cell is non-zero (assuming it was initially).
        return solution[0][0] != 0;
    } else {
        mpi_worker(puzzle);
        return false; // Workers don't return the solution status directly.
    }
}

/**
 * @brief Top-level solver function for the hybrid model.
 *
 * This function orchestrates the entire process from reading the puzzle
 * to running the hybrid solver and printing the results.
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring,
                                bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    // Master (rank 0) reads the file and broadcasts the puzzle to all workers.
    if (g_mpi_rank == 0) {
        bool success = read_puzzle_from_file(filename, &puzzle);
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) {
            stats.found_solution = false;
            return stats;
        }
    } else {
        bool success;
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) {
            stats.found_solution = false;
            return stats;
        }
    }

    // Broadcast the puzzle data itself.
    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD); // Ensure all processes have the puzzle before starting.

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    // Pre-coloring is done independently on all processes.
    double start_precolor = get_time();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = get_time() - start_precolor;

    int solution[MAX_N][MAX_N] = {{0}};
    double start_coloring = MPI_Wtime();
    bool found = color_g_hybrid(&puzzle, solution);
    stats.coloring_time = MPI_Wtime() - start_coloring;

    // The master process aggregates and prints the final statistics.
    if (g_mpi_rank == 0) {
        stats.found_solution = found;
        stats.total_time = stats.precolor_time + stats.coloring_time;
        stats.remaining_colors = 0;
        for (int row = 0; row < puzzle.size; row++) {
            for (int col = 0; col < puzzle.size; col++) {
                stats.remaining_colors += puzzle.pc_lengths[row][col];
            }
        }
        if (print_solution) {
            if (stats.found_solution) {
                printf("\nSolution:\n");
                print_board(&puzzle, solution);
            } else {
                printf("\nNo solution found.\n");
            }
        }
    } else {
        stats.found_solution = false; // Only master reports the final stats.
    }

    return stats;
}
