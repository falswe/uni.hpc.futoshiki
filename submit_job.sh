#!/bin/bash
#
# Utility script to submit Futoshiki solver jobs
#

usage() {
    echo "Usage: $0 <job_type> <puzzle_file> [args...]"
    echo ""
    echo "Job types:"
    echo "  seq            - Run sequential solver on a puzzle"
    echo "  mpi            - Run MPI solver with a specific process count and optional task factor"
    echo "  omp            - Run OpenMP solver with a specific thread count and optional task factor"
    echo "  hybrid         - Run Hybrid solver with given processes/threads and optional task factors"
    echo "  scaling_mpi    - Run MPI performance scaling tests with an optional task factor"
    echo "  scaling_omp    - Run OpenMP performance scaling tests with an optional task factor"
    echo "  scaling_hybrid - Run Hybrid scaling tests with optional MPI and OMP task factors"
    echo "  factor_mpi     - Run MPI task factor performance tests"
    echo "  factor_omp     - Run OpenMP task factor performance tests"
    echo "  factor_hybrid  - Run Hybrid factor test for a puzzle"
    echo ""
    echo "Examples:"
    echo "  $0 seq puzzles/5x5_easy_1.txt"
    echo "  $0 mpi puzzles/11x11_hard_1.txt 16"
    echo "  $0 mpi puzzles/11x11_hard_1.txt 16 2.0         # With task factor"
    echo "  $0 omp puzzles/9x9_hard_1.txt 8"
    echo "  $0 omp puzzles/9x9_hard_1.txt 8 4.0           # With task factor"
    echo "  $0 hybrid puzzles/11x11_hard_1.txt 2 4"
    echo "  $0 hybrid puzzles/11x11_hard_1.txt 2 4 8 8    # With mf and of factors"
    echo "  $0 scaling_mpi puzzles/9x9_hard_1.txt"
    echo "  $0 scaling_mpi puzzles/9x9_hard_1.txt 2.5       # With task factor"
    echo "  $0 scaling_omp puzzles/9x9_hard_1.txt 4.0       # With task factor"
    echo "  $0 scaling_hybrid puzzles/9x9_hard_1.txt 8 8    # With custom factors"
    echo "  $0 factor_mpi puzzles/9x9_hard_3.txt"
    echo "  $0 factor_omp puzzles/9x9_hard_3.txt"
    echo "  $0 factor_hybrid puzzles/9x9_hard_3.txt"

    exit 1
}

# Check for at least job_type and puzzle_file
if [ $# -lt 2 ]; then
    usage
fi

JOB_TYPE=$1
PUZZLE_FILE=$2

case $JOB_TYPE in
    seq)
        if [ $# -ne 2 ]; then
            echo "Error: Sequential job takes no extra arguments."
            usage
        fi
        echo "Submitting sequential job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/seq.pbs
        ;;
    
    mpi)
        if [ $# -lt 3 ] || [ $# -gt 4 ]; then
            echo "Error: Puzzle file and process count required. Factor is optional."
            usage
        fi
        NPROCS=$3
        FACTOR=$4
        RESOURCES="select=${NPROCS}:ncpus=1:mem=3gb"
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE,NPROCS=$NPROCS"

        if [ -n "$FACTOR" ]; then
            echo "Submitting MPI job for $PUZZLE_FILE with $NPROCS processes and factor $FACTOR..."
            QSUB_VARS="$QSUB_VARS,FACTOR=$FACTOR"
        else
            echo "Submitting MPI job for $PUZZLE_FILE with $NPROCS processes..."
        fi
        
        echo "Requesting resources: $RESOURCES"
        qsub -l "$RESOURCES" -v "$QSUB_VARS" jobs/mpi.pbs
        ;;

    omp)
        if [ $# -lt 3 ] || [ $# -gt 4 ]; then
            echo "Error: Puzzle file and thread count required. Factor is optional."
            usage
        fi
        NPROCS=$3
        FACTOR=$4
        RESOURCES="select=1:ncpus=${NPROCS}:mem=3gb"
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE,NPROCS=$NPROCS"

        if [ -n "$FACTOR" ]; then
            echo "Submitting OpenMP job for $PUZZLE_FILE with $NPROCS threads and factor $FACTOR..."
            QSUB_VARS="$QSUB_VARS,FACTOR=$FACTOR"
        else
            echo "Submitting OpenMP job for $PUZZLE_FILE with $NPROCS threads..."
        fi

        echo "Requesting resources: $RESOURCES"
        qsub -l "$RESOURCES" -v "$QSUB_VARS" jobs/omp.pbs
        ;;
    
    hybrid)
        if [ $# -ne 4 ] && [ $# -ne 6 ]; then
            echo "Error: Puzzle file, MPI procs, and OMP threads required. Factors are optional."
            usage
        fi
        MPI_PROCS=$3
        OMP_THREADS=$4
        MF=$5
        OF=$6
        RESOURCES="select=${MPI_PROCS}:ncpus=${OMP_THREADS}:mem=3gb"
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE,MPI_PROCS=$MPI_PROCS,OMP_THREADS=$OMP_THREADS"

        if [ -n "$MF" ] && [ -n "$OF" ]; then
            echo "Submitting Hybrid job for $PUZZLE_FILE with $MPI_PROCS MPI, $OMP_THREADS OMP, mf=$MF, of=$OF..."
            QSUB_VARS="$QSUB_VARS,MF=$MF,OF=$OF"
        else
            echo "Submitting Hybrid job for $PUZZLE_FILE with $MPI_PROCS MPI processes and $OMP_THREADS OpenMP threads..."
        fi
        
        echo "Requesting resources: $RESOURCES"
        qsub -l "$RESOURCES" -v "$QSUB_VARS" jobs/hybrid.pbs
        ;;

    scaling_mpi)
        FACTOR=$3
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE"
        if [ -n "$FACTOR" ]; then
            echo "Submitting MPI scaling test job for $PUZZLE_FILE with factor $FACTOR..."
            QSUB_VARS="$QSUB_VARS,FACTOR=$FACTOR"
        else
            echo "Submitting MPI scaling test job for $PUZZLE_FILE..."
        fi
        qsub -v "$QSUB_VARS" jobs/scaling_mpi.pbs
        ;;
    
    scaling_omp)
        FACTOR=$3
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE"
        if [ -n "$FACTOR" ]; then
            echo "Submitting OpenMP scaling test job for $PUZZLE_FILE with factor $FACTOR..."
            QSUB_VARS="$QSUB_VARS,FACTOR=$FACTOR"
        else
            echo "Submitting OpenMP scaling test job for $PUZZLE_FILE..."
        fi
        qsub -v "$QSUB_VARS" jobs/scaling_omp.pbs
        ;;
    
    scaling_hybrid)
        MF=$3
        OF=$4
        QSUB_VARS="PUZZLE_FILE=$PUZZLE_FILE"
        if [ -n "$MF" ] && [ -n "$OF" ]; then
            echo "Submitting Hybrid scaling test job for $PUZZLE_FILE with mf=$MF, of=$OF..."
            QSUB_VARS="$QSUB_VARS,MF=$MF,OF=$OF"
        else
            echo "Submitting Hybrid scaling test job for $PUZZLE_FILE with default factors..."
        fi
        qsub -v "$QSUB_VARS" jobs/scaling_hybrid.pbs
        ;;

    factor_mpi)
        echo "Submitting MPI factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/factor_mpi.pbs
        ;;

    factor_omp)
        echo "Submitting OpenMP factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/factor_omp.pbs
        ;;

    factor_hybrid)
        echo "Submitting Hybrid factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/factor_hybrid.pbs
        ;;
    
    *)
        echo "Error: Unknown job type '$JOB_TYPE'"
        usage
        ;;
esac

echo ""
echo "To check job status: qstat -u $USER"