#include "futoshiki_mpi.h"

#include <limits.h>
#include <mpi.h>
#include <string.h>

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

// Structure to represent a partial solution (work unit)
typedef struct {
    int depth;                   // How many cells have been filled
    int assignments[MAX_N * 3];  // [row1, col1, color1, row2, col2, color2, ...]
} WorkUnit;

/**
 * Calculate the appropriate depth for work distribution
 */
static int calculate_distribution_depth(Futoshiki* puzzle, int num_workers) {
    // If there are no workers, we don't need to generate sub-tasks for distribution.
    if (num_workers <= 0) {
        return 0;
    }

    // Set a reasonable max depth to prevent excessive work unit generation and memory usage.
    int max_depth = 5;
    if (puzzle->size > 9) max_depth = 4;
    if (puzzle->size > 15) max_depth = 3;

    // Find the sequence of empty cells to explore for job generation.
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

    if (num_empty == 0) {
        print_progress("Puzzle has no empty cells; no work to distribute.");
        return 0;
    }

    // Find the smallest depth 'd' where the estimated number of jobs > num_workers.
    long long estimated_jobs = 1;
    int chosen_depth = 0;

    for (int d = 0; d < num_empty && d < max_depth; ++d) {
        int row = empty_cells[d][0];
        int col = empty_cells[d][1];
        int branching_factor = puzzle->pc_lengths[row][col];

        if (branching_factor <= 0) {
            branching_factor = 1;  // Dead-end path won't increase sub-problems.
        }

        // Check for potential overflow before multiplication.
        if (branching_factor > 1 && estimated_jobs > LLONG_MAX / branching_factor) {
            estimated_jobs = LLONG_MAX;  // Mark as huge, will be > num_workers.
        } else {
            estimated_jobs *= branching_factor;
        }

        chosen_depth = d + 1;

        if (estimated_jobs > num_workers) {
            break;  // Found the minimal depth that exceeds worker count.
        }
    }

    print_progress("Job Distribution Strategy:");
    print_progress("  - Target: >%d jobs for %d workers.", num_workers, num_workers);
    print_progress("  - Chosen depth: %d (estimated to generate ~%lld work units)", chosen_depth,
                   estimated_jobs);

    return chosen_depth;
}

/**
 * Generate all valid partial solutions up to the specified depth
 */
static void generate_work_units_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N],
                                          WorkUnit** units, int* unit_count, int* capacity,
                                          int current_depth, int target_depth, int* assignments,
                                          int row, int col) {
    // Safety limit - don't generate too many work units
    if (*unit_count >= 10000) {
        print_progress("WARNING: Work unit limit reached (%d units)", *unit_count);
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

    // Check if we've reached the target depth or end of board
    if (current_depth >= target_depth || row >= puzzle->size) {
        // Create a work unit
        if (*unit_count >= *capacity) {
            int new_capacity = *capacity * 2;
            if (new_capacity > 10000) new_capacity = 10000;  // Hard limit
            if (new_capacity <= *capacity) return;           // Can't grow anymore

            WorkUnit* new_units = realloc(*units, new_capacity * sizeof(WorkUnit));
            if (!new_units) {
                print_progress("WARNING: Failed to expand work unit array");
                return;
            }
            *units = new_units;
            *capacity = new_capacity;
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
    // Limit initial capacity to prevent excessive memory allocation
    int capacity = (g_mpi_size - 1) * 4;   // Start with 4x workers
    if (capacity > 1000) capacity = 1000;  // Cap at 1000

    WorkUnit* units = malloc(capacity * sizeof(WorkUnit));
    if (!units) {
        print_progress("ERROR: Failed to allocate memory for work units");
        *num_units = 0;
        return NULL;
    }

    *num_units = 0;

    int solution[MAX_N][MAX_N];
    memcpy(solution, puzzle->board, sizeof(solution));

    int assignments[MAX_N * 3];  // row, col, color for each assignment

    generate_work_units_recursive(puzzle, solution, &units, num_units, &capacity, 0, depth,
                                  assignments, 0, 0);

    print_progress("Generated %d work units at depth %d", *num_units, depth);

    // Shrink to actual size
    if (*num_units > 0 && *num_units < capacity) {
        WorkUnit* shrunk = realloc(units, *num_units * sizeof(WorkUnit));
        if (shrunk) units = shrunk;
    }

    return units;
}

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
 * Master process function with multi-level distribution
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

                // TODO: remove these debug prints
                print_progress("Assigned work unit %d/%d to worker %d", next_unit + 1, num_units,
                               worker_rank);

                WorkUnit* unit = &work_units[next_unit];
                printf("[DEBUG] Work unit %d: depth=%d, assignments=", next_unit + 1, unit->depth);
                for (int i = 0; i < unit->depth; i++) {
                    printf(" (%d,%d,%d)", unit->assignments[i * 3], unit->assignments[i * 3 + 1],
                           unit->assignments[i * 3 + 2]);
                }
                printf("\n");
                fflush(stdout);

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