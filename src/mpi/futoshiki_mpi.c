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
    TAG_WORK_ASSIGNMENT = 3,
    TAG_SOLUTION_FOUND = 4,
    TAG_SOLUTION_DATA = 5,
    TAG_TERMINATE = 6
} MessageTag;

// Structure to represent a partial solution (work unit)
typedef struct {
    int depth;                   // How many cells have been filled
    int assignments[MAX_N * 2];  // [row1, col1, color1, row2, col2, color2, ...]
} WorkUnit;

/**
 * Calculate the appropriate depth for work distribution
 * We want at least num_workers * 2 work units for good load balancing
 */
static int calculate_distribution_depth(Futoshiki* puzzle, int num_workers) {
    int target_units = num_workers * 2;  // Aim for 2x oversubscription
    int current_units = 1;
    int depth = 0;

    // Find empty cells and estimate branching factor
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

    // Simulate depth exploration
    while (depth < num_empty && current_units < target_units) {
        int row = empty_cells[depth][0];
        int col = empty_cells[depth][1];
        int branching = puzzle->pc_lengths[row][col];

        current_units *= branching;
        depth++;

        // Don't go too deep - limit to reasonable depth
        if (depth > 5 || current_units > num_workers * 100) {
            break;
        }
    }

    print_progress("Distribution depth: %d (estimated %d work units for %d workers)", depth,
                   current_units, num_workers);

    return depth;
}

/**
 * Generate all valid partial solutions up to the specified depth
 * Uses recursive enumeration to build work units
 */
static void generate_work_units_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N],
                                          WorkUnit** units, int* unit_count, int* capacity,
                                          int current_depth, int target_depth, int* assignments,
                                          int row, int col) {
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

    // Check if we've reached the target depth or end of board
    if (current_depth >= target_depth || row >= puzzle->size) {
        // Create a work unit
        if (*unit_count >= *capacity) {
            *capacity *= 2;
            *units = realloc(*units, *capacity * sizeof(WorkUnit));
        }

        WorkUnit* unit = &(*units)[*unit_count];
        unit->depth = current_depth;
        memcpy(unit->assignments, assignments, current_depth * 3 * sizeof(int));
        (*unit_count)++;
        return;
    }

    // Try each possible color for this cell
    for (int i = 0; i < puzzle->pc_lengths[row][col]; i++) {
        int color = puzzle->pc_list[row][col][i];

        if (safe(puzzle, row, col, solution, color)) {
            // Make assignment
            solution[row][col] = color;
            assignments[current_depth * 3] = row;
            assignments[current_depth * 3 + 1] = col;
            assignments[current_depth * 3 + 2] = color;

            // Recurse
            generate_work_units_recursive(puzzle, solution, units, unit_count, capacity,
                                          current_depth + 1, target_depth, assignments, row,
                                          col + 1);

            // Backtrack
            solution[row][col] = EMPTY;
        }
    }
}

/**
 * Generate all work units up to the calculated depth
 */
static WorkUnit* generate_work_units(Futoshiki* puzzle, int depth, int* num_units) {
    int capacity = 100;
    WorkUnit* units = malloc(capacity * sizeof(WorkUnit));
    *num_units = 0;

    int solution[MAX_N][MAX_N];
    memcpy(solution, puzzle->board, sizeof(solution));

    int assignments[MAX_N * 2 * 3];  // row, col, color for each assignment

    generate_work_units_recursive(puzzle, solution, &units, num_units, &capacity, 0, depth,
                                  assignments, 0, 0);

    print_progress("Generated %d work units at depth %d", *num_units, depth);

    return units;
}

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

        // Initialize solution with puzzle board
        memcpy(solution, puzzle->board, sizeof(solution));

        // Apply the partial solution from work unit
        for (int i = 0; i < work_unit.depth; i++) {
            int row = work_unit.assignments[i * 3];
            int col = work_unit.assignments[i * 3 + 1];
            int color = work_unit.assignments[i * 3 + 2];
            solution[row][col] = color;
        }

        // Find where to continue from
        int start_row = 0, start_col = 0;
        if (work_unit.depth > 0) {
            start_row = work_unit.assignments[(work_unit.depth - 1) * 3];
            start_col = work_unit.assignments[(work_unit.depth - 1) * 3 + 1] + 1;
        }

        // Try to solve from this partial solution
        if (color_g_seq(puzzle, solution, start_row, start_col)) {
            found_solution = true;
            MPI_Send(&found_solution, 1, MPI_C_BOOL, 0, TAG_SOLUTION_FOUND, MPI_COMM_WORLD);
            MPI_Send(solution, MAX_N * MAX_N, MPI_INT, 0, TAG_SOLUTION_DATA, MPI_COMM_WORLD);

            // Wait for termination
            MPI_Recv(&work_unit, sizeof(WorkUnit), MPI_BYTE, 0, TAG_TERMINATE, MPI_COMM_WORLD,
                     &status);
            break;
        }
    }
}

// Master process function
static bool mpi_master(Futoshiki* puzzle, int solution[MAX_N][MAX_N]) {
    print_progress("Starting MPI parallel backtracking with %d workers", g_mpi_size - 1);

    // Calculate appropriate depth for distribution
    int depth = calculate_distribution_depth(puzzle, g_mpi_size - 1);

    // Generate work units
    int num_units;
    WorkUnit* work_units = generate_work_units(puzzle, depth, &num_units);

    if (num_units == 0) {
        print_progress("No work units generated - puzzle may already be solved");
        memcpy(solution, puzzle->board, sizeof(int) * MAX_N * MAX_N);
        free(work_units);
        return color_g_seq(puzzle, solution, 0, 0);
    }

    // Distribute work units
    int next_unit = 0;
    bool found_solution = false;
    int active_workers = g_mpi_size - 1;
    WorkUnit dummy_unit = {0};

    while (active_workers > 0) {
        MPI_Status status;
        bool worker_found_solution;

        // Receive from any worker
        MPI_Recv(&worker_found_solution, 1, MPI_C_BOOL, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 &status);
        int worker_rank = status.MPI_SOURCE;

        if (found_solution) {
            // We're in shutdown mode
            MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                     MPI_COMM_WORLD);
            active_workers--;
            continue;
        }

        if (status.MPI_TAG == TAG_SOLUTION_FOUND && worker_found_solution) {
            found_solution = true;
            MPI_Recv(solution, MAX_N * MAX_N, MPI_INT, worker_rank, TAG_SOLUTION_DATA,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            print_progress("Received solution from worker %d", worker_rank);

            // Terminate the successful worker
            MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                     MPI_COMM_WORLD);
            active_workers--;
        } else {
            // Worker is requesting work
            if (next_unit < num_units) {
                MPI_Send(&work_units[next_unit], sizeof(WorkUnit), MPI_BYTE, worker_rank,
                         TAG_WORK_ASSIGNMENT, MPI_COMM_WORLD);
                print_progress("Assigned work unit %d/%d to worker %d", next_unit + 1, num_units,
                               worker_rank);
                next_unit++;
            } else {
                // No more work
                MPI_Send(&dummy_unit, sizeof(WorkUnit), MPI_BYTE, worker_rank, TAG_TERMINATE,
                         MPI_COMM_WORLD);
                active_workers--;
            }
        }
    }

    free(work_units);
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