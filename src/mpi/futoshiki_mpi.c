#include "futoshiki_mpi.h"

#include <mpi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Strong definitions of MPI rank and size
int g_mpi_rank = 0;
int g_mpi_size = 1;

// MPI message tags
#define TAG_WORK_REQUEST 1
#define TAG_COLOR_ASSIGNMENT 2
#define TAG_SOLUTION_FOUND 3
#define TAG_SOLUTION_DATA 4
#define TAG_TERMINATE 5

/**
 * Parallel solving strategy (shared by OpenMP and MPI):
 * 1. Find the first empty cell in the puzzle
 * 2. Distribute its possible color choices among threads/processes
 * 3. Each thread/process solves its assigned subproblem sequentially
 * 4. First solution found is returned; others are terminated
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

        // Receive color assignment or termination signal
        MPI_Recv(color_assignment, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;
        }

        // Initialize solution with board values
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);

        int row = color_assignment[0];
        int col = color_assignment[1];
        int color = color_assignment[2];

        // Set the assigned color
        solution[row][col] = color;

        // Try to solve using sequential algorithm
        if (color_g_seq(puzzle, solution, row, col + 1)) {
            found_solution = true;

            // Send solution to master
            MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // Wait for termination after finding solution
            MPI_Recv(color_assignment, 3, MPI_INT, 0, TAG_TERMINATE, MPI_COMM_WORLD, &status);
            break;
        }
    }
}

// Master process function
static bool mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting parallel backtracking with %d workers", g_mpi_size - 1);

    // Initialize solution with board values
    memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);

    // Find first empty cell
    int start_row = 0, start_col = 0;
    bool found_empty = false;

    for (int r = 0; r < puzzle->size && !found_empty; r++) {
        for (int c = 0; c < puzzle->size && !found_empty; c++) {
            if (puzzle->board[r][c] == EMPTY) {
                start_row = r;
                start_col = c;
                found_empty = true;
            } else {
                solution[r][c] = puzzle->board[r][c];
            }
        }
    }

    if (!found_empty) {
        // No empty cells, puzzle is already solved
        return true;
    }

    print_progress("First empty cell at (%d,%d) with %d possible colors", start_row, start_col,
                   puzzle->pc_lengths[start_row][start_col]);

    int num_colors = puzzle->pc_lengths[start_row][start_col];
    int next_color_idx = 0;
    bool found_solution = false;
    int active_workers = g_mpi_size - 1;

    while (active_workers > 0 && !found_solution) {
        MPI_Status status;
        bool worker_found_solution;

        // Receive work request or solution from any worker
        MPI_Recv(&worker_found_solution, 1, MPI_C_BOOL, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 &status);

        if (status.MPI_TAG == TAG_SOLUTION_FOUND && worker_found_solution) {
            // Worker found a solution
            found_solution = true;
            MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, status.MPI_SOURCE, TAG_SOLUTION_DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            print_progress("Master received solution from worker %d", status.MPI_SOURCE);

            // Send termination signal to all workers
            int terminate_msg[3] = {-1, -1, -1};
            for (int w = 1; w < g_mpi_size; w++) {
                MPI_Send(terminate_msg, 3, MPI_INT, w, TAG_TERMINATE, MPI_COMM_WORLD);
            }
            break;
        }

        // Worker is requesting work
        if (next_color_idx < num_colors && !found_solution) {
            bool assigned_work = false;

            // Try to find a safe color to assign
            while (next_color_idx < num_colors && !assigned_work) {
                int color = puzzle->pc_list[start_row][start_col][next_color_idx];

                if (safe(puzzle, start_row, start_col, solution, color)) {
                    int color_assignment[3] = {start_row, start_col, color};
                    MPI_Send(color_assignment, 3, MPI_INT, status.MPI_SOURCE, TAG_COLOR_ASSIGNMENT,
                             MPI_COMM_WORLD);

                    print_progress("Master assigned color %d to worker %d", color,
                                   status.MPI_SOURCE);
                    next_color_idx++;
                    assigned_work = true;
                } else {
                    // Color is not safe, skip it
                    print_progress("Color %d is not safe at (%d,%d), skipping", color, start_row,
                                   start_col);
                    next_color_idx++;
                }
            }

            // If no safe color found, terminate this worker
            if (!assigned_work) {
                int terminate_msg[3] = {-1, -1, -1};
                MPI_Send(terminate_msg, 3, MPI_INT, status.MPI_SOURCE, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
                print_progress("No safe colors left for worker %d, %d workers remaining",
                               status.MPI_SOURCE, active_workers);
            }
        } else {
            // No more work, send termination signal
            int terminate_msg[3] = {-1, -1, -1};
            MPI_Send(terminate_msg, 3, MPI_INT, status.MPI_SOURCE, TAG_TERMINATE, MPI_COMM_WORLD);
            active_workers--;
            print_progress("No more work for worker %d, %d workers remaining", status.MPI_SOURCE,
                           active_workers);
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
        return false;  // Workers don't return solutions directly
    }
}

SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;

    // Only master reads the puzzle file
    if (g_mpi_rank == 0) {
        if (!read_puzzle_from_file(filename, &puzzle)) {
            // Broadcast failure to all processes
            int failure = 0;
            MPI_Bcast(&failure, 1, MPI_INT, 0, MPI_COMM_WORLD);
            return stats;
        }
        // Broadcast success
        int success = 1;
        MPI_Bcast(&success, 1, MPI_INT, 0, MPI_COMM_WORLD);
    } else {
        // Workers wait for success/failure signal
        int status;
        MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (!status) {
            return stats;
        }
    }

    // Broadcast puzzle to all processes
    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);

    // Ensure all processes have received the puzzle
    MPI_Barrier(MPI_COMM_WORLD);

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        int initial_board[MAX_N][MAX_N];
        memcpy(initial_board, puzzle.board, sizeof(initial_board));
        print_board(&puzzle, initial_board);
    }

    // All processes compute pre-coloring (redundant but ensures consistency)
    double start_precolor = get_time();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    double end_precolor = get_time();
    stats.precolor_time = end_precolor - start_precolor;

    if (print_solution && g_show_progress && g_mpi_rank == 0) {
        print_progress("Possible colors for each cell:");
        for (int row = 0; row < puzzle.size; row++) {
            for (int col = 0; col < puzzle.size; col++) {
                print_cell_colors(&puzzle, row, col);
            }
        }
    }

    // Time the list-coloring phase
    int solution[MAX_N][MAX_N] = {{0}};
    double start_coloring = get_time();

    stats.found_solution = color_g(&puzzle, solution);

    double end_coloring = get_time();
    stats.coloring_time = end_coloring - start_coloring;
    stats.total_time = stats.precolor_time + stats.coloring_time;

    // Calculate remaining colors and total processed
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
                printf("Solution:\n");
                print_board(&puzzle, solution);
            } else {
                printf("No solution found.\n");
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