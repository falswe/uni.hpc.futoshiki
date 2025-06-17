#!/bin/bash

# Load necessary modules
module load openmpi-4.0.4

# Clean old files
rm -f *.o futoshiki_mpi

# Build with MPI and C99 standard
echo "Compiling comparison.c..."
mpicc -std=c99 -Wall -g -c comparison.c -o comparison.o

echo "Compiling futoshiki_mpi.c..."
mpicc -std=c99 -Wall -g -c futoshiki_mpi.c -o futoshiki_mpi.o

echo "Compiling main_mpi.c..."
mpicc -std=c99 -Wall -g -c main_mpi.c -o main_mpi.o

# Link with MPI
echo "Linking..."
mpicc comparison.o futoshiki_mpi.o main_mpi.o -o futoshiki_mpi

echo "Build complete!"
ls -la futoshiki_mpi