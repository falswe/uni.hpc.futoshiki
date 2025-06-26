#include "futoshiki_mpi.h"

#include <mpi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

// Sequential backtracking algorithm
static bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
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

// Worker process function
static void mpi_worker(Futoshiki* puzzle) {
    int solution[MAX_N][MAX_N];
    int color_assignment[3];  // [row, col, color]
    MPI_Status status;

    while (true) {
        // Request work from master
        MPI_Send(NULL, 0, MPI_INT, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);

        // Receive assignment or termination
        MPI_Recv(color_assignment, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;  // Master says we're done
        }

        // Initialize solution with board values
        memcpy(solution, puzzle->board, sizeof(solution));

        int row = color_assignment[0];
        int col = color_assignment[1];
        int color = color_assignment[2];

        print_progress("Worker %d trying color %d at (%d,%d)", g_mpi_rank, color, row, col);

        solution[row][col] = color;

        // Try to solve using sequential algorithm
        if (color_g_seq(puzzle, solution, row, col + 1)) {
            // Found a solution! Report it to the master
            MPI_Send(NULL, 0, MPI_INT, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);
            print_progress("Worker %d found solution with color %d", g_mpi_rank, color);
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
    double global_start_time = MPI_Wtime();

    if (g_mpi_rank == 0) {
        if (!read_puzzle_from_file(filename, &puzzle)) {
            puzzle.size = -1;  // Signal error
        }
    }

    // Broadcast the puzzle struct to all processes
    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);

    if (puzzle.size == -1) {
        MPI_Barrier(MPI_COMM_WORLD);
        return stats;  // All processes exit on file read error
    }

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    // Time the pre-coloring phase
    double start_precolor = MPI_Wtime();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = MPI_Wtime() - start_precolor;

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
    MPI_Barrier(MPI_COMM_WORLD);
    double start_coloring = MPI_Wtime();

    stats.found_solution = color_g(&puzzle, solution);

    MPI_Barrier(MPI_COMM_WORLD);
    stats.coloring_time = MPI_Wtime() - start_coloring;
    stats.total_time = MPI_Wtime() - global_start_time;

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