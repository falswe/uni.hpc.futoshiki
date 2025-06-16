# Parallel Futoshiki Solver

## Overview

This project implements a parallel solver for the Futoshiki puzzle using OpenMP. The solution combines constraint propagation techniques with parallel backtracking to efficiently solve puzzles of various sizes and difficulty levels.

## What is Futoshiki?

Futoshiki (不等式 - "inequality" in Japanese) is a logic puzzle played on an N×N grid where:

1. Each row and column must contain the numbers 1 through N exactly once (Latin square property)
2. Inequality signs (< and >) between adjacent cells must be satisfied
3. Some cells may be pre-filled with numbers (givens)

Example of a 4×4 Futoshiki puzzle:
```
4   0 < 0   0
        ^
0   0   0   0

0   0   0   0

0   0 < 0   2
```

## Implementation Approach

The solver uses a two-phase approach:

### 1. Pre-coloring Phase (Constraint Propagation)

This phase reduces the search space by filtering possible values for each cell:
- For each cell, we maintain a list of possible values (colors)
- We apply constraint propagation to filter these lists
- Constraints include row/column uniqueness and inequality constraints
- This phase is parallelized using OpenMP

### 2. List Coloring Phase (Backtracking)

This phase uses a parallel backtracking approach to find a valid assignment:
- Uses an intelligent cell selection strategy (most constrained cell first)
- Implements multi-level parallelism with dynamic task creation
- Automatically adjusts between parallel and sequential solving based on depth and available threads
- Includes optimizations to minimize thread contention

## Key Features

- **Multi-level parallelism**: Creates parallel tasks at multiple levels of the search tree
- **Intelligent variable ordering**: Selects cells with fewest remaining values first
- **Dynamic task granularity**: Adjusts between parallel and sequential solving based on depth
- **Parallel pre-coloring**: Uses parallel constraint propagation to reduce search space

## Building and Running

### Prerequisites

- GCC compiler with OpenMP support
- Make (optional, for build automation)

### Build

```bash
# Build with provided script
./build.sh

# Or manually
gcc -fopenmp -std=c99 -Wall -g -c comparison.c futoshiki.c main.c
gcc -fopenmp comparison.o futoshiki.o main.o -o futoshiki
```

### Run

```bash
# Basic usage
./futoshiki examples/9x9_extreme1_initial.txt

# Without pre-coloring optimization
./futoshiki examples/9x9_extreme1_initial.txt -n

# With progress display
./futoshiki examples/9x9_extreme1_initial.txt -v

# Run comparison mode (with and without pre-coloring)
./futoshiki examples/9x9_extreme1_initial.txt -c
```

### Performance Testing

Run comprehensive performance tests with:

```bash
./performance_test.sh
```

This script will test the solver with different thread counts and puzzle sizes, and generate performance graphs.

## Performance Analysis

The solver shows significant speedup with parallelization, especially for larger puzzles:

1. **Pre-coloring phase**: Shows near-linear scaling with the number of threads
2. **Backtracking phase**: Scaling depends on puzzle difficulty and available parallelism
3. **Overall performance**: Typically 4-16x speedup on 16-core systems compared to sequential version

## Implementation Details

### Data Structures

- **Puzzle representation**: 2D arrays for board, horizontal and vertical constraints
- **Color lists**: Array of possible values for each cell with separate length tracking
- **Cell selection**: Uses a priority-based approach to select most constrained cells first

### Parallelization Strategy

- **Pre-coloring phase**: Uses parallel for loops with critical sections for updates
- **Backtracking phase**: Uses recursive task creation with controlled depth
- **Load balancing**: Dynamic task creation based on available parallelism

## Future Improvements

1. **Enhanced constraint propagation**: Implement more advanced techniques like arc consistency
2. **Adaptive parallelism**: Dynamically adjust parallelization strategy based on puzzle characteristics
3. **Memory optimization**: Use bit vectors for color lists to reduce memory usage
4. **Hybrid parallel model**: Combine OpenMP with MPI for cluster computing

## License

This project is open source and available under the MIT License.