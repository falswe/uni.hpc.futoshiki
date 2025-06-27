#include "futoshiki_mpi.h"

#include <mpi.h>
#include <string.h>

// Strong definitions of MPI rank and size
int g_mpi_rank = 0;
int g_mpi_size = 1;

// MPI message tags
typedef enum {
    TAG_WORK_REQUEST = 1,
    TAG_COLOR_ASSIGNMENT = 2,
    TAG_SOLUTION_FOUND = 3,
    TAG_SOLUTION_DATA = 4,
    TAG_TERMINATE = 5
} MessageTag;

/**
 * MPI parallel implementation of the Futoshiki solver
 *
 * Parallelization strategy:
 * - Master process distributes color choices to workers
 * - Workers solve their assigned subproblems
 * - First solution found terminates all workers
 */

// Worker process function
static void mpi_worker(Futoshiki* puzzle) {
    int solution[MAX_N][MAX_N];
    int color_assignment[3];  // [row, col, color]
    bool found_solution = false;
    MPI_Status status;

    while (true) {
        // Request work from master
        MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);

        // Receive assignment or termination
        MPI_Recv(color_assignment, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;
        }

        // Initialize solution and set assigned color
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        int row = color_assignment[0];
        int col = color_assignment[1];
        int color = color_assignment[2];
        solution[row][col] = color;

        // Try to solve
        if (color_g_seq(puzzle, solution, row, col + 1)) {
            found_solution = true;
            MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // Wait for termination
            MPI_Recv(color_assignment, 3, MPI_INT, 0, TAG_TERMINATE, MPI_COMM_WORLD, &status);
            break;
        }
    }
}

// Master process function
static bool mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting MPI parallel backtracking with %d workers", g_mpi_size - 1);

    // Find first empty cell
    int start_row, start_col;
    if (!find_first_empty_cell(puzzle, solution, &start_row, &start_col)) {
        return true;  // No empty cells, puzzle solved.
    }

    print_progress("First empty cell at (%d,%d) with %d possible colors", start_row, start_col,
                   puzzle->pc_lengths[start_row][start_col]);

    int num_colors = puzzle->pc_lengths[start_row][start_col];
    int next_color_idx = 0;
    bool found_solution = false;
    int active_workers = g_mpi_size - 1;

    while (active_workers > 0) {
        MPI_Status status;
        bool worker_found_solution;

        // Receive from any worker
        MPI_Recv(&worker_found_solution, 1, MPI_C_BOOL, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 &status);
        int worker_rank = status.MPI_SOURCE;

        // If a solution has already been found, we are in "shutdown mode"
        if (found_solution) {
            int terminate_msg[3] = {-1, -1, -1};
            MPI_Send(terminate_msg, 3, MPI_INT, worker_rank, TAG_TERMINATE, MPI_COMM_WORLD);
            active_workers--;
            continue;  // Go back to waiting for the next worker to check in
        }

        if (status.MPI_TAG == TAG_SOLUTION_FOUND && worker_found_solution) {
            found_solution = true;
            MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            print_progress("Received solution from worker %d", worker_rank);

            // Terminate the successful worker.
            int terminate_msg[3] = {-1, -1, -1};
            MPI_Send(terminate_msg, 3, MPI_INT, worker_rank, TAG_TERMINATE, MPI_COMM_WORLD);
            active_workers--;
        }
        // Otherwise, the worker is requesting work
        else {
            bool assigned_work = false;
            // Try to assign the next available, safe color
            while (next_color_idx < num_colors) {
                int color = puzzle->pc_list[start_row][start_col][next_color_idx++];
                if (safe(puzzle, start_row, start_col, solution, color)) {
                    int assignment[3] = {start_row, start_col, color};
                    MPI_Send(assignment, 3, MPI_INT, worker_rank, TAG_COLOR_ASSIGNMENT,
                             MPI_COMM_WORLD);
                    print_progress("Assigned color %d to worker %d", color, worker_rank);
                    assigned_work = true;
                    break;
                }
            }

            // If no work could be assigned (no more colors), terminate this worker
            if (!assigned_work) {
                int terminate_msg[3] = {-1, -1, -1};
                MPI_Send(terminate_msg, 3, MPI_INT, worker_rank, TAG_TERMINATE, MPI_COMM_WORLD);
                active_workers--;
            }
        }
    }

    return found_solution;
}

// Main solving dispatcher
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