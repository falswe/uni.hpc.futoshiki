# Futoshiki Parallel Solver Makefile
# Builds sequential, OpenMP, and MPI versions

# === Configuration ===
CC      := gcc
MPICC   := mpicc
CFLAGS  := -std=c99 -Wall -g -O2
OMPFLAGS:= -fopenmp
LDFLAGS := -lm

# === Project Structure ===
BIN_DIR := bin
OBJ_DIR := obj
SRC_DIR := src

# === Source Files ===
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.c)
SEQ_SRC    := $(wildcard $(SRC_DIR)/sequential/*.c)
OMP_SRC    := $(wildcard $(SRC_DIR)/openmp/*.c)
MPI_SRC    := $(wildcard $(SRC_DIR)/mpi/*.c)

# === Object Files ===
COMMON_OBJ := $(patsubst $(SRC_DIR)/common/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
SEQ_OBJ    := $(patsubst $(SRC_DIR)/sequential/%.c,$(OBJ_DIR)/%.o,$(SEQ_SRC))
OMP_OBJ    := $(patsubst $(SRC_DIR)/openmp/%.c,$(OBJ_DIR)/%.o,$(OMP_SRC))
MPI_OBJ    := $(patsubst $(SRC_DIR)/mpi/%.c,$(OBJ_DIR)/%.o,$(MPI_SRC))

# === Executables ===
SEQ_TARGET := $(BIN_DIR)/futoshiki_seq
OMP_TARGET := $(BIN_DIR)/futoshiki_omp
MPI_TARGET := $(BIN_DIR)/futoshiki_mpi

# === Main Targets ===
.PHONY: all sequential openmp mpi clean help

all: sequential openmp mpi

sequential: $(SEQ_TARGET)

openmp: $(OMP_TARGET)

mpi: $(MPI_TARGET)

# === Build Rules ===

$(SEQ_TARGET): $(SEQ_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OMP_TARGET): $(OMP_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OMPFLAGS) $^ -o $@ $(LDFLAGS)

$(MPI_TARGET): $(MPI_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# === Compilation Rules ===
VPATH = $(SRC_DIR)/common:$(SRC_DIR)/sequential:$(SRC_DIR)/openmp:$(SRC_DIR)/mpi

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Add OpenMP flags for OpenMP objects
$(OMP_OBJ): CFLAGS += $(OMPFLAGS)

# Use MPI compiler for MPI objects
$(MPI_OBJ): CC = $(MPICC)

# === Directory Creation ===
$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

# === Utility Targets ===
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BIN_DIR) $(OBJ_DIR)

help:
	@echo "Futoshiki Parallel Solver"
	@echo "========================"
	@echo "Targets:"
	@echo "  all        - Build all versions (default)"
	@echo "  sequential - Build sequential version"
	@echo "  openmp     - Build OpenMP version"
	@echo "  mpi        - Build MPI version"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this message"