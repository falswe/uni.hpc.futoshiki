#include "futoshiki_mpi.h"

#include <mpi.h>
#include <string.h>

#include "../common/multilevel_tasks.h"

// Strong definitions of MPI rank and size
int g_mpi_rank = 0;
int g_mpi_size = 1;

// MPI message tags
typedef enum {
    TAG_WORK_REQUEST = 1,
    TAG_COLOR_ASSIGNMENT = 2,
    TAG_SOLUTION_DATA = 3,
    TAG_TERMINATE = 4,
    TAG_WORK_ASSIGNMENT = 5
} MessageTag;

/**
 * Worker process function with multi-level work units
 */
static void mpi_worker(Futoshiki* puzzle) {
    int solution[MAX_N][MAX_N];
    WorkUnit work_unit;
    bool found_solution = false;
    MPI_Status status;

    while (true) {
        // Request work from master
        MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);

        // Receive work unit or termination
        MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;
        }

        // Apply the work unit to get partial solution
        apply_work_unit(puzzle, &work_unit, solution);

        // Find where to continue from
        int start_row, start_col;
        get_continuation_point(&work_unit, &start_row, &start_col);

        // Try to solve from this partial solution
        if (color_g_seq(puzzle, solution, start_row, start_col)) {
            found_solution = true;
            // First send the boolean flag with work request tag
            MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);
            // Then send the solution data
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // Wait for termination
            MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, TAG_TERMINATE, MPI_COMM_WORLD,
                     &status);
            break;
        }
    }
}

/**
 * Master process function
 */
static bool mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting MPI parallel backtracking with %d workers", g_mpi_size - 1);

    // Calculate appropriate depth for distribution
    int depth = calculate_distribution_depth(puzzle, g_mpi_size - 1);

    // Generate work units
    int num_units;
    WorkUnit* work_units = generate_work_units(puzzle, depth, &num_units);

    if (!work_units || num_units == 0) {
        print_progress("No work units generated - falling back to sequential");
        if (work_units) free(work_units);
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

    print_progress("Starting distribution of %d work units to %d workers", num_units,
                   g_mpi_size - 1);

    // Distribute work units
    int next_unit = 0;
    bool found_solution = false;
    int active_workers = g_mpi_size - 1;
    WorkUnit dummy_unit = {0};

    print_progress("Waiting for %d workers to start...", active_workers);

    while (active_workers > 0) {
        MPI_Status status;
        bool worker_found_solution;

        // Receive work request from any worker (expecting TAG_WORK_REQUEST)
        MPI_Recv(&worker_found_solution, 1, MPI_C_BOOL, MPI_ANY_SOURCE, TAG_WORK_REQUEST,
                 MPI_COMM_WORLD, &status);
        int worker_rank = status.MPI_SOURCE;

        if (found_solution) {
            // We're in shutdown mode
            MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                     MPI_COMM_WORLD);
            active_workers--;
            continue;
        }

        if (worker_found_solution) {
            // Worker claims to have found a solution, receive it
            found_solution = true;
            MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            print_progress("Received solution from worker %d", worker_rank);

            // Terminate the successful worker
            MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                     MPI_COMM_WORLD);
            active_workers--;
            print_progress("Worker %d terminated (solution found), %d workers remaining",
                           worker_rank, active_workers);
        } else {
            // Worker is requesting work.
            if (next_unit < num_units) {
                MPI_Send(&work_units[next_unit], sizeof(WorkUnit), MPI_BYTE, worker_rank,
                         TAG_WORK_ASSIGNMENT, MPI_COMM_WORLD);

                print_progress("Assigned work unit %d/%d to worker %d", next_unit + 1, num_units,
                               worker_rank);
                print_work_unit(&work_units[next_unit], next_unit + 1);

                next_unit++;
            } else {
                // No more work
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
                print_progress("Worker %d terminated (no more work), %d workers remaining",
                               worker_rank, active_workers);
            }
        }
    }

    free(work_units);
    return found_solution;
}

/**
 * Main solving dispatcher
 */
static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    if (g_mpi_size == 1) {
        print_progress("Only 1 process available, using sequential algorithm");
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return color_g_seq(puzzle, solution, 0, 0);
    }

    if (g_mpi_rank == 0) {
        return mpi_master(puzzle, solution);
    } else {
        mpi_worker(puzzle);
        return false;
    }
}

// Main solving function
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    // Master reads puzzle and broadcasts result
    if (g_mpi_rank == 0) {
        bool success = read_puzzle_from_file(filename, &puzzle);
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) return stats;
    } else {
        bool success;
        MPI_Bcast(&success, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
        if (!success) return stats;
    }

    // Broadcast puzzle to all processes
    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    // All processes compute pre-coloring (ensures consistency)
    double start_precolor = get_time();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = get_time() - start_precolor;

    if (print_solution && g_show_progress && g_mpi_rank == 0) {
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

    // Master calculates statistics
    if (g_mpi_rank == 0) {
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
    }

    return stats;
}

void init_mpi(int* argc, char*** argv) {
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_mpi_size);
}

void finalize_mpi() { MPI_Finalize(); }