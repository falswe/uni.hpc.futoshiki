# =============================================================================
# Makefile for the Futoshiki Parallel Solver
# Manages the build process for sequential, OpenMP, and MPI versions.
# =============================================================================

# --- Configuration ---
# Compilers and compiler flags
CC          := gcc
MPICC       := mpicc
CFLAGS      := -std=c99 -Wall -g -O2
OMPFLAGS    := -fopenmp
LDFLAGS     := -lm
MPI_LDFLAGS := $(LDFLAGS)

# --- Project Structure ---
BIN_DIR := bin
OBJ_DIR := obj
SRC_DIR := src

# --- Source File Discovery ---
# Automatically find all .c files in the source directories.
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.c)
SEQ_SRC    := $(wildcard $(SRC_DIR)/sequential/*.c)
OMP_SRC    := $(wildcard $(SRC_DIR)/openmp/*.c)
MPI_SRC    := $(wildcard $(SRC_DIR)/mpi/*.c)

# --- Object File Generation ---
# Generate object file names from source file names.
# e.g., src/common/utils.c -> obj/utils.o
COMMON_OBJ := $(patsubst $(SRC_DIR)/common/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
SEQ_OBJ    := $(patsubst $(SRC_DIR)/sequential/%.c,$(OBJ_DIR)/%.o,$(SEQ_SRC))
OMP_OBJ    := $(patsubst $(SRC_DIR)/openmp/%.c,$(OBJ_DIR)/%.o,$(OMP_SRC))
MPI_OBJ    := $(patsubst $(SRC_DIR)/mpi/%.c,$(OBJ_DIR)/%.o,$(MPI_SRC))

# --- Executable Targets ---
SEQ_TARGET := $(BIN_DIR)/futoshiki_seq
OMP_TARGET := $(BIN_DIR)/futoshiki_omp
MPI_TARGET := $(BIN_DIR)/futoshiki_mpi

# =============================================================================
# --- Main Targets ---
# =============================================================================
.PHONY: all sequential openmp mpi clean distclean help

# Default target: build all versions.
all: sequential openmp mpi

# Build the sequential version.
sequential: $(SEQ_TARGET)

# Build the OpenMP version.
openmp: $(OMP_TARGET)

# Build the MPI version.
mpi: $(MPI_TARGET)


# =============================================================================
# --- Build Rules (Linking) ---
# =============================================================================

# Rule to link the sequential executable.
$(SEQ_TARGET): $(SEQ_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Rule to link the OpenMP executable.
$(OMP_TARGET): $(OMP_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OMPFLAGS) $^ -o $@ $(LDFLAGS)

# Rule to link the MPI executable.
$(MPI_TARGET): $(MPI_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $^ -o $@ $(MPI_LDFLAGS)


# =============================================================================
# --- VPATH & Build Rules (Compilation) ---
# =============================================================================

# Use VPATH to tell make where to find the source files for the object files.
VPATH = $(SRC_DIR)/common:$(SRC_DIR)/sequential:$(SRC_DIR)/openmp:$(SRC_DIR)/mpi

# Generic rule to compile a .c file into a .o file in the obj/ directory.
# Make will use VPATH to find the corresponding .c file for each .o target.
# e.g., for 'obj/main_seq.o', it will find and compile 'src/sequential/main_seq.c'
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Add OpenMP flags only when compiling OpenMP source files.
# This target-specific variable assignment works perfectly with the generic rule above.
$(OMP_OBJ): CFLAGS += $(OMPFLAGS)


# Rule to create the build directories if they don't exist.
# The '|' makes it an "order-only" prerequisite.
$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@


# =============================================================================
# --- Utility Targets ---
# =============================================================================

# 'clean' removes only build artifacts (binaries and object files).
clean:
	@echo "Cleaning build artifacts (bin/ and obj/)..."
	@rm -rf $(BIN_DIR) $(OBJ_DIR)

# 'distclean' is more thorough, removing everything 'clean' does plus
# job outputs and editor backup files.
distclean: clean
	@echo "Cleaning job outputs and editor backups..."
	@find . -type f -name '*~' -delete
	@find . -type f \( -name '*.sh.e*' -o -name '*.sh.o*' \) -delete

# 'help' displays a helpful message about how to use the Makefile.
help:
	@echo "Futoshiki Parallel Solver Build System"
	@echo "======================================"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Primary Targets:"
	@echo "  all         Build all versions (default)"
	@echo "  sequential  Build the sequential version"
	@echo "  openmp      Build the OpenMP parallel version"
	@echo "  mpi         Build the MPI parallel version"
	@echo ""
	@echo "Utility Targets:"
	@echo "  clean       Remove all compiled binaries and object files"
	@echo "  distclean   Run 'clean' and remove job outputs/backups"
	@echo "  help        Show this help message"
	@echo ""
	@echo "Example: make -j4 all"