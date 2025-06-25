#include "futoshiki_mpi.h"

#include <mpi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Strong definitions of MPI rank and size. These will override the 'weak'
// symbols in futoshiki_common.c when creating the MPI executable.
int g_mpi_rank = 0;
int g_mpi_size = 1;

// MPI message tags
#define TAG_WORK_REQUEST 1
#define TAG_COLOR_ASSIGNMENT 2
#define TAG_SOLUTION_FOUND 3
#define TAG_SOLUTION_DATA 4
#define TAG_TERMINATE 5

// --- Private Helper Functions for MPI Solver ---

// A local, sequential backtracking solver for workers to solve their assigned subproblems.
static bool solve_subproblem_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col) {
    if (row >= puzzle->size) return true;

    int next_row = (col == puzzle->size - 1) ? row + 1 : row;
    int next_col = (col == puzzle->size - 1) ? 0 : col + 1;

    if (solution[row][col] != EMPTY) {
        return solve_subproblem_seq(puzzle, solution, next_row, next_col);
    }

    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];
        if (safe(puzzle, row, col, solution, color)) {
            solution[row][col] = color;
            if (solve_subproblem_seq(puzzle, solution, next_row, next_col)) {
                return true;
            }
        }
    }
    solution[row][col] = EMPTY;  // Backtrack
    return false;
}

// --- MPI Master/Worker Logic ---

// Worker process function: requests work, solves subproblems, and reports back.
static void mpi_worker(Futoshiki* puzzle) {
    int solution[MAX_N][MAX_N];
    int color_assignment[3];  // [row, col, color]
    MPI_Status status;

    while (true) {
        MPI_Send(NULL, 0, MPI_INT, 0, TAG_WORK_REQUEST, MPI_COMM_WORLD);
        MPI_Recv(color_assignment, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_TERMINATE) {
            break;  // Master says we're done
        }

        memcpy(solution, puzzle->board, sizeof(solution));
        int row = color_assignment[0];
        int col = color_assignment[1];
        int color = color_assignment[2];
        solution[row][col] = color;

        if (solve_subproblem_seq(puzzle, solution, row, col + 1)) {
            // Found a solution! Report it to the master.
            MPI_Send(NULL, 0, MPI_INT, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);
        }
    }
}

// Master process function: distributes work and collects results.
static bool mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Master process starting with %d worker(s)", g_mpi_size - 1);

    int start_row = -1, start_col = -1;
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            if (puzzle->board[r][c] == EMPTY) {
                start_row = r;
                start_col = c;
                goto found_first_empty;
            }
        }
    }
found_first_empty:
    if (start_row == -1) return true;  // Puzzle already solved

    print_progress("Parallelizing on first empty cell (%d,%d) with %d choices", start_row,
                   start_col, puzzle->pc_lengths[start_row][start_col]);

    int num_tasks = puzzle->pc_lengths[start_row][start_col];
    int tasks_sent = 0;
    int active_workers = g_mpi_size - 1;
    bool solution_found = false;
    MPI_Status status;

    while (active_workers > 0) {
        MPI_Recv(NULL, 0, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int worker_rank = status.MPI_SOURCE;

        if (status.MPI_TAG == TAG_SOLUTION_FOUND) {
            solution_found = true;
            MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            print_progress("Solution received from worker %d", worker_rank);
            break;  // Exit loop to terminate other workers
        }

        // It was a work request, send a task if available
        if (tasks_sent < num_tasks) {
            int color = puzzle->pc_list[start_row][start_col][tasks_sent];
            int task[3] = {start_row, start_col, color};
            MPI_Send(task, 3, MPI_INT, worker_rank, TAG_COLOR_ASSIGNMENT, MPI_COMM_WORLD);
            tasks_sent++;
        } else {
            // No more tasks, terminate this worker
            MPI_Send(NULL, 0, MPI_INT, worker_rank, TAG_TERMINATE, MPI_COMM_WORLD);
            active_workers--;
        }
    }

    // Terminate any remaining workers
    for (int i = 1; i < g_mpi_size; i++) {
        MPI_Send(NULL, 0, MPI_INT, i, TAG_TERMINATE, MPI_COMM_WORLD);
    }

    return solution_found;
}

// Main solving dispatcher for MPI
static bool color_g(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    if (g_mpi_size == 1) {
        print_progress("Only 1 process, running sequentially...");
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        return solve_subproblem_seq(puzzle, solution, 0, 0);
    }

    if (g_mpi_rank == 0) {
        return mpi_master(puzzle, solution);
    } else {
        mpi_worker(puzzle);
        return false;  // Workers do not return a solution directly.
    }
}

// --- Public Interface Functions ---

SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution) {
    SolverStats stats = {0};
    Futoshiki puzzle;
    double global_start_time = MPI_Wtime();

    if (g_mpi_rank == 0) {
        if (!read_puzzle_from_file(filename, &puzzle)) {
            puzzle.size = -1;  // Signal error
        }
    }

    // Broadcast the puzzle struct to all processes.
    // Use size as a success/error signal.
    MPI_Bcast(&puzzle, sizeof(Futoshiki), MPI_BYTE, 0, MPI_COMM_WORLD);

    if (puzzle.size == -1) {
        MPI_Barrier(MPI_COMM_WORLD);
        return stats;  // All processes exit on file read error.
    }

    if (print_solution && g_mpi_rank == 0) {
        printf("Initial puzzle:\n");
        print_board(&puzzle, puzzle.board);
    }

    double start_precolor = MPI_Wtime();
    stats.colors_removed = compute_pc_lists(&puzzle, use_precoloring);
    stats.precolor_time = MPI_Wtime() - start_precolor;

    int solution_grid[MAX_N][MAX_N] = {{0}};
    MPI_Barrier(MPI_COMM_WORLD);
    double start_coloring = MPI_Wtime();

    stats.found_solution = color_g(&puzzle, solution_grid);

    MPI_Barrier(MPI_COMM_WORLD);
    stats.coloring_time = MPI_Wtime() - start_coloring;
    stats.total_time = MPI_Wtime() - global_start_time;

    if (g_mpi_rank == 0) {
        stats.remaining_colors = 0;
        for (int r = 0; r < puzzle.size; r++) {
            for (int c = 0; c < puzzle.size; c++) {
                stats.remaining_colors += puzzle.pc_lengths[r][c];
            }
        }
        if (print_solution) {
            if (stats.found_solution) {
                printf("\nSolution found:\n");
                print_board(&puzzle, solution_grid);
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