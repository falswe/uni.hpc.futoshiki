#!/bin/bash

# Load necessary modules
module load openmpi-4.0.4

# Build with OpenMP and C99 standard
gcc -fopenmp -std=c99 -Wall -g -c comparison.c -o comparison.o
gcc -fopenmp -std=c99 -Wall -g -c futoshiki.c -o futoshiki.o
gcc -fopenmp -std=c99 -Wall -g -c main.c -o main.o

# Link with OpenMP
gcc -fopenmp comparison.o futoshiki.o main.o -o futoshiki