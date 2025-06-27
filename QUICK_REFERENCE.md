# Futoshiki Solver - Quick Reference

## Building

```bash
make all        # Build all versions
make sequential # Build only sequential
make openmp     # Build only OpenMP  
make mpi        # Build only MPI
make clean      # Remove build files
```

## Running

### Sequential
```bash
./bin/futoshiki_seq puzzles/9x9_extreme1.txt
./bin/futoshiki_seq puzzles/9x9_extreme1.txt -v    # Verbose
./bin/futoshiki_seq puzzles/9x9_extreme1.txt -n    # No pre-coloring
./bin/futoshiki_seq puzzles/9x9_extreme1.txt -c    # Compare mode
```

### OpenMP
```bash
OMP_NUM_THREADS=8 ./bin/futoshiki_omp puzzles/9x9_extreme1.txt
./bin/futoshiki_omp puzzles/9x9_extreme1.txt -t 16  # Set threads
./bin/futoshiki_omp puzzles/9x9_extreme1.txt -t 4 -v # Verbose
```

### MPI
```bash
mpirun -np 4 ./bin/futoshiki_mpi puzzles/9x9_extreme1.txt
mpirun -np 8 ./bin/futoshiki_mpi puzzles/9x9_extreme1.txt -v
mpirun -np 16 ./bin/futoshiki_mpi puzzles/9x9_extreme1.txt -n
```

## HPC Job Submission

```bash
# Submit performance test
qsub jobs/futoshiki_performance.pbs

# Submit OpenMP job with specific puzzle
qsub -v PUZZLE_FILE="puzzles/9x9_extreme1.txt" jobs/futoshiki_omp.pbs

# Submit MPI job with specific puzzle
qsub -v PUZZLE_FILE="puzzles/11x11_hard_1.txt" jobs/futoshiki_mpi.pbs

# Check job status
qstat -u $USER

## Puzzle Format

```
0   0   0   0     # Numbers (0 = empty)
V       V         # Vertical constraints
0   0   0   0     
                  
0   0 < 0 < 0     # Horizontal constraints  
    V             
0   0   0   3     
```

- `<` : Left < Right
- `>` : Left > Right  
- `^` : Upper < Lower
- `v` or `V` : Upper > Lower

## Common Options

- `-v` : Verbose mode (show progress)
- `-n` : Disable pre-coloring optimization
- `-c` : Comparison mode (with vs without pre-coloring)
- `-t N` : Set thread count (OpenMP only)

## Performance Tips

1. **Pre-coloring**: Usually provides 2-10x speedup
2. **Thread count**: Best performance at 8-16 threads for most puzzles
3. **MPI**: Most efficient with 4-16 processes
4. **Large puzzles**: MPI scales better than OpenMP for 11x11+

## Algorithm Overview

1. **Pre-coloring Phase**:
   - Initialize possible values (1 to n) for each empty cell
   - Apply constraint propagation
   - Repeat until no more reductions

2. **Solving Phase**:
   - Find first empty cell
   - Distribute its possible values across threads/processes
   - Recursive backtracking with early termination

## File Structure

```
src/
├── common/        # Shared code
├── sequential/    # Sequential solver
├── openmp/        # OpenMP solver  
└── mpi/          # MPI solver

puzzles/          # Example puzzles
jobs/             # PBS scripts
bin/              # Executables (after build)
obj/              # Object files (after build)
```