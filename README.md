# Parallel Futoshiki Solver

Futoshiki is a Japanese puzzle game played on an n×n grid. The name "Futoshiki" means "inequality" in Japanese, highlighting the puzzle's defining feature. The objective is to fill each cell with numbers from 1 to n such that:

1. Each number appears exactly once in each row and column (Latin square property)
2. All inequality constraints between adjacent cells are satisfied (< or >)
3. Pre-filled values (givens) must be maintained

The Futoshiki problem is NP-complete, making it computationally challenging for larger board sizes. This project implements a parallel solver for Futoshiki puzzles using OpenMP, with a focus on optimizing search space and computation time.

## Sequential Algorithm Analysis

The sequential approach to solving Futoshiki combines two main techniques:

1. **Pre-coloring (Constraint Propagation)**: This phase filters possible values for each cell based on:
   - Latin square constraints (row/column uniqueness)
   - Inequality constraints between adjacent cells
   - Pre-filled values

2. **List Coloring (Backtracking)**: This phase uses backtracking to find a valid assignment:
   - Try each possible value from the filtered list for the current cell
   - If a valid assignment is found, move to the next cell
   - If no valid assignment exists, backtrack to the previous cell

The sequential backtracking algorithm has a worst-case time complexity of O(n^(n²)), where n is the board size, as each of the n² cells could potentially try all n possible values.
