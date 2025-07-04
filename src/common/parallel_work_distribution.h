#ifndef PARALLEL_WORK_DISTRIBUTION_H
#define PARALLEL_WORK_DISTRIBUTION_H

#include "futoshiki_common.h"

/**
 * Multilevel task generation for parallel Futoshiki solvers
 *
 * This module provides common functionality for generating partial solutions
 * (work units) that can be distributed across parallel workers/threads.
 * Based on the more accurate MPI implementation.
 */

// Structure representing a partial solution (work unit)
typedef struct {
    int depth;                   // Number of assignments made
    int assignments[MAX_N * 3];  // [row1, col1, color1, row2, col2, color2, ...]
} WorkUnit;

/**
 * Count the number of valid partial solutions at a given depth.
 * This gives us the exact number of work units that will be generated.
 *
 * @param puzzle The Futoshiki puzzle
 * @param solution Current solution state
 * @param empty_cells Array of empty cell coordinates
 * @param num_empty_cells Total number of empty cells
 * @param current_cell_idx Current position in empty_cells array
 * @param target_depth Depth to count up to
 * @return Number of valid partial solutions
 */
long long count_valid_assignments_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N],
                                            int empty_cells[MAX_N * MAX_N][2], int num_empty_cells,
                                            int current_cell_idx, int target_depth);

/**
 * Calculate the appropriate depth for work distribution.
 * Finds the minimum depth that generates at least num_workers work units.
 *
 * @param puzzle The Futoshiki puzzle
 * @param num_workers Target number of workers/threads
 * @return Optimal depth for work generation
 */
int calculate_distribution_depth(Futoshiki* puzzle, int num_workers);

/**
 * Generate all valid partial solutions up to the specified depth.
 *
 * @param puzzle The Futoshiki puzzle
 * @param depth Target depth for partial solutions
 * @param num_units Output: number of work units generated
 * @return Array of WorkUnit structures (caller must free)
 */
WorkUnit* generate_work_units(Futoshiki* puzzle, int depth, int* num_units);

/**
 * Internal recursive function for generating work units.
 *
 * @param puzzle The Futoshiki puzzle
 * @param solution Current solution state
 * @param units Pointer to work units array
 * @param unit_count Current number of units generated
 * @param capacity Current capacity of units array
 * @param current_depth Current recursion depth
 * @param target_depth Target depth to stop at
 * @param assignments Current assignments array
 * @param row Current row being processed
 * @param col Current column being processed
 */
void generate_work_units_recursive(Futoshiki* puzzle, int solution[MAX_N][MAX_N], WorkUnit** units,
                                   int* unit_count, int* capacity, int current_depth,
                                   int target_depth, int* assignments, int row, int col);

/**
 * Find all empty cells in the puzzle.
 *
 * @param puzzle The Futoshiki puzzle
 * @param empty_cells Output array of [row, col] pairs
 * @return Number of empty cells found
 */
int find_empty_cells(const Futoshiki* puzzle, int empty_cells[MAX_N * MAX_N][2]);

/**
 * Apply a work unit's partial solution to a solution matrix.
 *
 * @param puzzle The Futoshiki puzzle
 * @param work_unit The work unit containing assignments
 * @param solution Solution matrix to apply assignments to
 */
void apply_work_unit(const Futoshiki* puzzle, const WorkUnit* work_unit,
                     int solution[MAX_N][MAX_N]);

/**
 * Get the starting position for continuing from a work unit.
 *
 * @param work_unit The work unit
 * @param start_row Output: starting row
 * @param start_col Output: starting column
 */
void get_continuation_point(const WorkUnit* work_unit, int* start_row, int* start_col);

/**
 * Debug function to print work unit details.
 *
 * @param work_unit The work unit to print
 * @param unit_number Unit identifier for display
 */
void print_work_unit(const WorkUnit* work_unit, int unit_number);

#endif  // PARALLEL_WORK_DISTRIBUTION_H