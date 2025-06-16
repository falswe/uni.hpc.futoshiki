#!/bin/bash

# Load necessary modules
module load openmpi-4.0.4

# Build with MPI and C99 standard
mpicc -std=c99 -Wall -g -c comparison.c -o comparison.o
mpicc -std=c99 -Wall -g -c futoshiki_mpi.c -o futoshiki_mpi.o
mpicc -std=c99 -Wall -g -c main_mpi.c -o main_mpi.o

# Link with MPI
mpicc comparison.o futoshiki_mpi.o main_mpi.o -o futoshiki_mpi