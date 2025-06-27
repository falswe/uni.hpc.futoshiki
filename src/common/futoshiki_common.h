#ifndef FUTOSHIKI_COMMON_H
#define FUTOSHIKI_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define MAX_N 50
#define EMPTY 0

// Constraint types for inequality signs
typedef enum {
    NO_CONS = 0,
    GREATER = 1,  // Left/Upper > Right/Lower
    SMALLER = 2   // Left/Upper < Right/Lower
} Constraint;

// Futoshiki puzzle structure
typedef struct {
    int size;                             // Size of the puzzle (N x N)
    int board[MAX_N][MAX_N];              // Initial board state (0 = empty)
    Constraint h_cons[MAX_N][MAX_N - 1];  // Horizontal constraints
    Constraint v_cons[MAX_N - 1][MAX_N];  // Vertical constraints
    int pc_list[MAX_N][MAX_N][MAX_N];     // Possible colors for each cell [row][col][values]
    int pc_lengths[MAX_N][MAX_N];         // Length of possible color lists
} Futoshiki;

// Solver statistics returned by all implementations
typedef struct {
    double precolor_time;  // Time spent in pre-coloring phase
    double coloring_time;  // Time spent in solving/coloring phase
    double total_time;     // Total solving time
    int colors_removed;    // Number of colors removed by pre-coloring
    int remaining_colors;  // Colors remaining after pre-coloring
    int total_processed;   // Total colors processed (n^3 for nxn puzzle)
    bool found_solution;   // Whether a solution was found
} SolverStats;

// === Core constraint checking functions ===

/**
 * Check if a color assignment is safe (satisfies all constraints)
 */
bool safe(const Futoshiki* puzzle, int row, int col, int solution[MAX_N][MAX_N], int color);

/**
 * Check if there exists a valid neighbor for inequality constraints
 */
bool has_valid_neighbor(const Futoshiki* puzzle, int row, int col, int color, bool need_greater);

/**
 * Check if a color satisfies all inequality constraints
 */
bool satisfies_inequalities(const Futoshiki* puzzle, int row, int col, int color);

// === Pre-coloring optimization functions ===

/**
 * Filter possible colors for a cell based on constraints
 */
void filter_possible_colors(Futoshiki* puzzle, int row, int col);

/**
 * Process uniqueness constraints when a cell has only one possible color
 */
void process_uniqueness(Futoshiki* puzzle, int row, int col);

/**
 * Compute possible color lists for all cells (pre-coloring phase)
 */
int compute_pc_lists(Futoshiki* puzzle, bool use_precoloring);

// === Solving functions ===

/**
 * Find the first empty cell in the puzzle
 */
bool find_first_empty_cell(const Futoshiki* puzzle, int solution[MAX_N][MAX_N], int* row, int* col);

/**
 * Sequential backtracking algorithm to solve the Futoshiki puzzle
 */
bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col);

// === I/O functions ===

/**
 * Parse a Futoshiki puzzle from a string
 */
bool parse_futoshiki(const char* input, Futoshiki* puzzle);

/**
 * Read a puzzle from a file
 */
bool read_puzzle_from_file(const char* filename, Futoshiki* puzzle);

/**
 * Print the puzzle board with constraints
 */
void print_board(const Futoshiki* puzzle, int solution[MAX_N][MAX_N]);

// === Utility functions ===

/**
 * Get current time in seconds (for timing)
 */
double get_time(void);

/**
 * Control progress message display
 */
void set_progress_display(bool show);

/**
 * Print progress message (only if enabled and on master process)
 */
void print_progress(const char* format, ...);

/**
 * Print possible colors for a specific cell (for debugging)
 */
void print_cell_colors(const Futoshiki* puzzle, int row, int col);

// === Global variables ===

// For MPI compatibility - these are weak symbols that can be overridden
extern int g_mpi_rank;
extern int g_mpi_size;

// Global progress display flag
extern bool g_show_progress;

// === Main interface - all implementations must provide this ===

/**
 * Main solving interface
 * @param filename Path to the puzzle file
 * @param use_precoloring Whether to use pre-coloring optimization
 * @param print_solution Whether to print the solution
 * @return SolverStats structure with timing and solution information
 */
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif  // FUTOSHIKI_COMMON_H