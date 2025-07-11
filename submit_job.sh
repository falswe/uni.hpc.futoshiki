#!/bin/bash
#
# Utility script to submit Futoshiki solver jobs
#

usage() {
    echo "Usage: $0 <job_type> <puzzle_file> <num_procs_or_threads>"
    echo ""
    echo "Job types:"
    echo "  perf    - Run performance scaling tests for a specific puzzle"
    echo "  omp     - Run OpenMP solver on a puzzle with a specific thread count"
    echo "  mpi     - Run MPI solver on a puzzle with a specific process count"
    echo ""
    echo "Examples:"
    echo "  $0 perf puzzles/9x9_hard_1.txt"
    echo "  $0 omp puzzles/9x9_hard_1.txt 8     # Run OpenMP with 8 threads"
    echo "  $0 mpi puzzles/11x11_hard_1.txt 16  # Run MPI with 16 processes"
    exit 1
}

if [ $# -lt 2 ]; then
    usage
fi

JOB_TYPE=$1
PUZZLE_FILE=$2
NPROCS=$3

case $JOB_TYPE in
    perf)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for performance test job"
            usage
        fi
        echo "Submitting performance test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_performance.pbs
        ;;
    
    omp)
        if [ -z "$PUZZLE_FILE" ] || [ -z "$NPROCS" ]; then
            echo "Error: Puzzle file and thread count required for OpenMP job"
            usage
        fi
        
        # OpenMP needs N cores on a single node
        RESOURCES="select=1:ncpus=${NPROCS}:mem=8gb"
        
        echo "Submitting OpenMP job for $PUZZLE_FILE with $NPROCS threads..."
        echo "Requesting resources: $RESOURCES"
        
        qsub -l "$RESOURCES" -v PUZZLE_FILE="$PUZZLE_FILE",NPROCS="$NPROCS" jobs/futoshiki_omp.pbs
        ;;
    
    mpi)
        if [ -z "$PUZZLE_FILE" ] || [ -z "$NPROCS" ]; then
            echo "Error: Puzzle file and process count required for MPI job"
            usage
        fi
        
        # MPI needs N nodes, 1 core per node (for this use case)
        # Note: for scaling test on one node, we still need to request all procs
        # The performance script will handle the node allocation for MPI tests
        RESOURCES="select=${NPROCS}:ncpus=1:mem=8gb"
        
        echo "Submitting MPI job for $PUZZLE_FILE with $NPROCS processes..."
        echo "Requesting resources: $RESOURCES"
        
        qsub -l "$RESOURCES" -v PUZZLE_FILE="$PUZZLE_FILE",NPROCS="$NPROCS" jobs/futoshiki_mpi.pbs
        ;;
    
    *)
        echo "Error: Unknown job type '$JOB_TYPE'"
        usage
        ;;
esac

echo ""
echo "To check job status: qstat -u $USER"