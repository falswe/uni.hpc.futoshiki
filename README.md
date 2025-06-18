# Parallel Futoshiki Solver

This project provides sequential, OpenMP, and MPI-based parallel solvers for the Futoshiki puzzle.

Futoshiki is a logic puzzle on an n×n grid. The goal is to fill each cell with numbers from 1 to n such that:
1.  Each number appears exactly once in each row and column.
2.  All inequality constraints (`<`, `>`) between adjacent cells are satisfied.
3.  Any pre-filled values are maintained.

## Project Structure

The repository is organized as follows:
```
.
├── bin/ # Compiled executables
├── obj/ # Compiled object files
├── src/ # Source code
│ ├── common/ # Code shared by all versions
│ ├── sequential/ # Sequential solver implementation
│ ├── openmp/ # OpenMP-based parallel implementation
│ └── mpi/ # MPI-based parallel implementation
├── puzzles/ # Example puzzle files
├── Makefile # Build system configuration
└── README.md # This documentation file
```

## Dependencies

To build and run this project, you will need:
*   A C99 compatible compiler (e.g., `gcc`)
*   `make`
*   An OpenMP compatible compiler
*   An MPI implementation (e.g., OpenMPI, MPICH)

## Building the Project

The project uses a `Makefile` for easy compilation.

*   **Build all versions (sequential, OpenMP, and MPI):**
    ```bash
    make all
    ```
    *To build in parallel, use the `-j` flag (e.g., `make -j4`).*

*   **Build a specific version:**
    ```bash
    make sequential
    make openmp
    make mpi
    ```

*   **Cleaning up:**
    *   To remove compiled binaries and object files:
        ```bash
        make clean
        ```
    *   To perform a deep clean (removes build artifacts, job outputs, etc.):
        ```bash
        make distclean
        ```

## Running the Solvers

The compiled executables are located in the `bin/` directory.

*   **Sequential:**
    ```bash
    ./bin/futoshiki_seq <path/to/puzzle_file.txt>
    ```

*   **OpenMP:**
    ```bash
    # Set the number of threads before running
    export OMP_NUM_THREADS=4
    ./bin/futoshiki_omp <path/to/puzzle_file.txt>
    ```

*   **MPI:**
    ```bash
    # Run with 4 processes
    mpirun -np 4 ./bin/futoshiki_mpi <path/to/puzzle_file.txt>
    ```

## Algorithm Overview

The solver uses a combination of constraint propagation and backtracking search.

1.  **Constraint Propagation**: Before searching, the solver filters the possible values for each cell based on row/column uniqueness and inequality constraints. This significantly prunes the search space.

2.  **Backtracking Search**: A recursive algorithm explores the remaining possibilities to find a valid solution.

### Parallelization Strategy
*   **OpenMP**: The backtracking search space is divided among multiple threads, which explore their assigned sub-trees in parallel on shared memory.
*   **MPI**: The work is distributed among multiple processes. A master process can distribute initial board configurations to worker processes, who solve their assigned portion of the search space.