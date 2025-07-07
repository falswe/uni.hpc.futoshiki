#ifndef FUTOSHIKI_COMMON_H
#define FUTOSHIKI_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

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
bool safe(const Futoshiki* puzzle, int row, int col, int solution[MAX_N][MAX_N], int color);
bool has_valid_neighbor(const Futoshiki* puzzle, int row, int col, int color, bool need_greater);
bool satisfies_inequalities(const Futoshiki* puzzle, int row, int col, int color);

// === Pre-coloring optimization functions ===
void filter_possible_colors(Futoshiki* puzzle, int row, int col);
void process_uniqueness(Futoshiki* puzzle, int row, int col);
int compute_pc_lists(Futoshiki* puzzle, bool use_precoloring);

// === Solving functions ===
bool find_first_empty_cell(const Futoshiki* puzzle, int solution[MAX_N][MAX_N], int* row, int* col);
bool color_g_seq(Futoshiki* puzzle, int solution[MAX_N][MAX_N], int row, int col);

// === I/O functions ===
bool parse_futoshiki(const char* input, Futoshiki* puzzle);
bool read_puzzle_from_file(const char* filename, Futoshiki* puzzle);
void print_board(const Futoshiki* puzzle, int solution[MAX_N][MAX_N]);

// === Utility functions ===
double get_time(void);
int get_target_tasks(int num_workers, double factor, const char* impl_name);

// === Global variables ===
extern int g_mpi_rank;
extern int g_mpi_size;

// === Main interface - all implementations must provide this ===
SolverStats solve_puzzle(const char* filename, bool use_precoloring, bool print_solution);

#endif  // FUTOSHIKI_COMMON_H