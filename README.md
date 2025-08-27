# Parallel Futoshiki Solver

A high-performance parallel solver for Futoshiki puzzles with sequential, OpenMP, MPI implementations and also an hybrid one combining the two approaches together.

## Overview

Futoshiki is a logic puzzle on an n×n grid where:
- Each cell must contain a number from 1 to n
- Each number appears exactly once in each row and column
- Inequality constraints (`<`, `>`) between adjacent cells must be satisfied

This project implements a parallel solver using a **list-coloring based algorithm** with pre-coloring optimization for efficient constraint propagation.

## Features

- **Four implementations**: Sequential, OpenMP (shared memory), MPI (distributed), hybrid (MPI+OMP)
- **Pre-coloring optimization**: Reduces search space by filtering impossible values
- **Parallel backtracking**: Distributes search branches across threads/processes
- **Performance comparison**: Built-in benchmarking and analysis tools

## Building

### Prerequisites
- C99 compatible compiler (gcc)
- GNU Make
- OpenMP support
- MPI implementation (OpenMPI, MPICH)

### Compilation

```bash
# Build all versions
make all

# Build specific version
make sequential
make openmp
make mpi

# Clean build
make clean
```

## Usage

### Basic Usage

```bash
# Sequential version
./bin/futoshiki_seq puzzles/9x9_extreme1.txt

# OpenMP version (8 threads)
OMP_NUM_THREADS=8 ./bin/futoshiki_omp puzzles/9x9_extreme1.txt

# MPI version (4 processes)
mpirun -np 4 ./bin/futoshiki_mpi puzzles/9x9_extreme1.txt
```

### Command-line Options

- `-n` : Disable pre-coloring optimization
- `-v` : Verbose mode (show progress messages)
- `-c` : Comparison mode (with vs without pre-coloring)
- `-t N` : Set number of threads (OpenMP only)

### Examples

```bash
# Compare pre-coloring effectiveness
./bin/futoshiki_seq puzzles/9x9_extreme1.txt -c

# Verbose OpenMP run with 16 threads
./bin/futoshiki_omp puzzles/9x9_extreme1.txt -t 16 -v

# MPI without pre-coloring
mpirun -np 8 ./bin/futoshiki_mpi puzzles/9x9_extreme1.txt -n
```

## Puzzle File Format

Puzzles are text files with:
- Numbers (0 for empty cells) separated by spaces
- Horizontal constraints: `<` or `>` between numbers
- Vertical constraints: `^` (up smaller) or `v` (up greater) on separate lines

Example (4×4 puzzle):
```
0   0   0   0
V       V
0   0   0   0

0   0 < 0 < 0
    V
0   0   0   3
```

## Algorithm

The solver uses a **list-coloring based backtracking algorithm**:

1. **Pre-coloring Phase**: 
   - Initialize each empty cell with all possible values (1 to n)
   - Apply constraint propagation to filter invalid values
   - Repeat until no more reductions possible

2. **Solving Phase**:
   - Find first empty cell
   - Distribute its possible values across threads/processes
   - Each thread/process solves its subproblem recursively
   - First solution found terminates other searches

## Project Structure

```
.
├── jobs/                # HPC job scripts
├── puzzles/             # Example puzzles
├── src/
│   ├── common/          # Shared code
│   ├── hybrid/          # Hybrid parallel solver
│   ├── mpi/             # MPI parallel solver
│   ├── omp/             # OpenMP parallel solver
│   └── seq/             # Sequential solver
├── puzzles/             # Utility scripts
└── Makefile
```

## License

This project is part of an academic assignment for parallel computing.
