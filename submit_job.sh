#!/bin/bash
#
# Utility script to submit Futoshiki solver jobs
#

usage() {
    echo "Usage: $0 <job_type> [puzzle_file]"
    echo ""
    echo "Job types:"
    echo "  perf    - Run performance scaling tests"
    echo "  omp     - Run OpenMP solver on a puzzle"
    echo "  mpi     - Run MPI solver on a puzzle"
    echo ""
    echo "Examples:"
    echo "  $0 perf                          # Run performance tests"
    echo "  $0 omp puzzles/9x9_extreme1.txt  # Run OpenMP solver"
    echo "  $0 mpi puzzles/11x11_hard_1.txt  # Run MPI solver"
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

JOB_TYPE=$1
PUZZLE_FILE=$2

case $JOB_TYPE in
    perf)
        echo "Submitting performance test job..."
        qsub jobs/futoshiki_performance.pbs
        ;;
    
    omp)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for OpenMP job"
            usage
        fi
        echo "Submitting OpenMP job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_omp.pbs
        ;;
    
    mpi)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for MPI job"
            usage
        fi
        echo "Submitting MPI job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_mpi.pbs
        ;;
    
    *)
        echo "Error: Unknown job type '$JOB_TYPE'"
        usage
        ;;
esac

echo ""
echo "To check job status: qstat -u $USER"