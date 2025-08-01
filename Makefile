# Futoshiki Parallel Solver Makefile
# Builds sequential, OpenMP, MPI, and Hybrid versions

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

# === Selecting Clang and libomp for macOS ===
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    CC := /opt/homebrew/opt/llvm/bin/clang 
    OMPFLAGS := -fopenmp=libomp 
endif

# === Source Files ===
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.c)
SEQ_SRC    := $(wildcard $(SRC_DIR)/sequential/*.c)
OMP_SRC    := $(wildcard $(SRC_DIR)/openmp/*.c)
MPI_SRC    := $(wildcard $(SRC_DIR)/mpi/*.c)
HYBRID_SRC := $(wildcard $(SRC_DIR)/hybrid/*.c)

# === Object Files ===
COMMON_OBJ := $(patsubst $(SRC_DIR)/common/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
SEQ_OBJ    := $(patsubst $(SRC_DIR)/sequential/%.c,$(OBJ_DIR)/%.o,$(SEQ_SRC))
OMP_OBJ    := $(patsubst $(SRC_DIR)/openmp/%.c,$(OBJ_DIR)/%.o,$(OMP_SRC))
MPI_OBJ    := $(patsubst $(SRC_DIR)/mpi/%.c,$(OBJ_DIR)/%.o,$(MPI_SRC))
HYBRID_OBJ := $(patsubst $(SRC_DIR)/hybrid/%.c,$(OBJ_DIR)/%.o,$(HYBRID_SRC))

# === Executables ===
SEQ_TARGET    := $(BIN_DIR)/futoshiki_seq
OMP_TARGET    := $(BIN_DIR)/futoshiki_omp
MPI_TARGET    := $(BIN_DIR)/futoshiki_mpi
HYBRID_TARGET := $(BIN_DIR)/futoshiki_hybrid

# === Main Targets ===
.PHONY: all sequential openmp mpi hybrid clean help

all: sequential openmp mpi hybrid

sequential: $(SEQ_TARGET)

openmp: $(OMP_TARGET)

mpi: CC = $(MPICC)
mpi: $(MPI_TARGET)

hybrid: CC = $(MPICC)
hybrid: $(HYBRID_TARGET)

# === Build Rules ===
# Note: Parallel implementations now depend on sequential object for color_g_seq

$(SEQ_TARGET): $(SEQ_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# OpenMP needs sequential's color_g_seq function
$(OMP_TARGET): $(OMP_OBJ) $(COMMON_OBJ) $(OBJ_DIR)/futoshiki_seq.o
	$(CC) $(CFLAGS) $(OMPFLAGS) $^ -o $@ $(LDFLAGS)

# MPI needs sequential's color_g_seq function
$(MPI_TARGET): $(MPI_OBJ) $(COMMON_OBJ) $(OBJ_DIR)/futoshiki_seq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Hybrid needs both sequential's color_g_seq and OpenMP's color_g_omp
$(HYBRID_TARGET): $(HYBRID_OBJ) $(COMMON_OBJ) $(OBJ_DIR)/futoshiki_seq.o $(OBJ_DIR)/futoshiki_omp.o
	$(CC) $(CFLAGS) $(OMPFLAGS) $^ -o $@ $(LDFLAGS)

# === Compilation Rules ===
VPATH = $(SRC_DIR)/common:$(SRC_DIR)/sequential:$(SRC_DIR)/openmp:$(SRC_DIR)/mpi:$(SRC_DIR)/hybrid

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Add OpenMP flags for OpenMP and Hybrid objects
$(OMP_OBJ): CFLAGS += $(OMPFLAGS)
$(HYBRID_OBJ): CFLAGS += $(OMPFLAGS)

# === Directory Creation ===
# Ensure bin and obj directories exist before trying to use them
$(SEQ_TARGET) $(OMP_TARGET) $(MPI_TARGET) $(HYBRID_TARGET): | $(BIN_DIR) $(OBJ_DIR)

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
	@echo "  hybrid     - Build Hybrid MPI+OpenMP version"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this message"
	@echo ""
	@echo "Note: Parallel implementations depend on sequential for color_g_seq"
	@echo "      Hybrid additionally depends on OpenMP for color_g_omp"