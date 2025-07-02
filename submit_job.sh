#!/bin/bash
#
# Utility script to submit Futoshiki solver jobs
#

usage() {
    echo "Usage: $0 <job_type> <puzzle_file> [num_procs_or_threads]"
    echo ""
    echo "Job types:"
    echo "  perf    - Run performance scaling tests (no extra args)"
    echo "  omp     - Run OpenMP solver on a puzzle with a specific thread count"
    echo "  mpi     - Run MPI solver on a puzzle with a specific process count"
    echo ""
    echo "Examples:"
    echo "  $0 perf"
    echo "  $0 omp puzzles/9x9_extreme1.txt 8   # Run OpenMP with 8 threads"
    echo "  $0 mpi puzzles/11x11_hard_1.txt 16  # Run MPI with 16 processes"
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

JOB_TYPE=$1
PUZZLE_FILE=$2
NPROCS=$3

case $JOB_TYPE in
    perf)
        echo "Submitting performance test job..."
        qsub jobs/futoshiki_performance.pbs
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
        RESOURCES="select=${NPROCS}:ncpus=1:mem=2gb"
        
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