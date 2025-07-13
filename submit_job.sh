#!/bin/bash
#
# Utility script to submit Futoshiki solver jobs
#

usage() {
    echo "Usage: $0 <job_type> <puzzle_file> [num_procs_or_threads]"
    echo ""
    echo "Job types:"
    echo "  seq           - Run sequential solver on a puzzle"
    echo "  mpi           - Run MPI solver with a specific process count"
    echo "  omp           - Run OpenMP solver with a specific thread count"
    echo "  hybrid        - Run Hybrid MPI+OpenMP solver with given MPI processes and OpenMP threads"
    echo "  scaling_mpi   - Run MPI performance scaling tests"
    echo "  scaling_omp   - Run OpenMP performance scaling tests"
    echo "  factor_mpi    - Run MPI task factor performance tests"
    echo "  factor_omp    - Run OpenMP task factor performance tests"
    echo "  factor_hybrid - Run Hybrid factor test for a puzzle"
    echo ""
    echo "Examples:"
    echo "  $0 seq puzzles/5x5_easy_1.txt"
    echo "  $0 mpi puzzles/11x11_hard_1.txt 16"
    echo "  $0 omp puzzles/9x9_hard_1.txt 8"
    echo "  $0 hybrid puzzles/11x11_hard_1.txt 2 4"
    echo "  $0 scaling_mpi puzzles/9x9_hard_1.txt"
    echo "  $0 scaling_omp puzzles/9x9_hard_1.txt"
    echo "  $0 factor_mpi puzzles/11x11_hard_1.txt"
    echo "  $0 factor_omp puzzles/9x9_hard_1.txt"
    echo "  $0 factor_hybrid puzzles/9x9_hard_1.txt"
    exit 1
}

# Check for at least job_type and puzzle_file
if [ $# -lt 2 ]; then
    usage
fi

JOB_TYPE=$1
PUZZLE_FILE=$2
NPROCS=$3

case $JOB_TYPE in
    seq)
            if [ -z "$PUZZLE_FILE" ]; then
                echo "Error: Puzzle file required for sequential job"
                usage
            fi
            echo "Submitting sequential job for $PUZZLE_FILE..."
            qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_seq.pbs
            ;;
    
    mpi)
        if [ -z "$PUZZLE_FILE" ] || [ -z "$NPROCS" ]; then
            echo "Error: Puzzle file and process count required for MPI job"
            usage
        fi
        
        # MPI needs N nodes, 1 core per node for this use case
        RESOURCES="select=${NPROCS}:ncpus=1:mem=8gb"
        
        echo "Submitting MPI job for $PUZZLE_FILE with $NPROCS processes..."
        echo "Requesting resources: $RESOURCES"
        
        qsub -l "$RESOURCES" -v PUZZLE_FILE="$PUZZLE_FILE",NPROCS="$NPROCS" jobs/futoshiki_mpi.pbs
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
    
    hybrid)
        if [ $# -ne 4 ]; then
            echo "Error: Puzzle file, MPI process count, and OpenMP thread count required for Hybrid job"
            usage
        fi
        MPI_PROCS=$3
        OMP_THREADS=$4

        # Hybrid needs MPI_PROCS nodes, with OMP_THREADS cores each
        RESOURCES="select=${MPI_PROCS}:ncpus=${OMP_THREADS}:mem=8gb"
        
        echo "Submitting Hybrid job for $PUZZLE_FILE with $MPI_PROCS MPI processes and $OMP_THREADS OpenMP threads..."
        echo "Requesting resources: $RESOURCES"

        qsub -l "$RESOURCES" -v PUZZLE_FILE="$PUZZLE_FILE",MPI_PROCS="$MPI_PROCS",OMP_THREADS="$OMP_THREADS" jobs/futoshiki_hybrid.pbs
        ;;

    scaling_mpi)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for MPI scaling test job"
            usage
        fi
        echo "Submitting MPI scaling test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_scaling_mpi.pbs
        ;;
    
    scaling_omp)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for OpenMP scaling test job"
            usage
        fi
        echo "Submitting OpenMP scaling test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_scaling_omp.pbs
        ;;

    factor_mpi)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for MPI factor test job"
            usage
        fi
        echo "Submitting MPI factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_factor_mpi.pbs
        ;;

    factor_omp)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for OpenMP factor test job"
            usage
        fi
        echo "Submitting OpenMP factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_factor_omp.pbs
        ;;

    factor_hybrid)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for Hybrid factor test job"
            usage
        fi
        echo "Submitting Hybrid factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_factor_hybrid.pbs
        ;;

    seq)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for sequential job"
            usage
        fi
        echo "Submitting sequential job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_seq.pbs
        ;;

    scaling_omp)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for OpenMP scaling test job"
            usage
        fi
        echo "Submitting OpenMP scaling test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_scaling_omp.pbs
        ;;

    scaling_mpi)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for MPI scaling test job"
            usage
        fi
        echo "Submitting MPI scaling test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_scaling_mpi.pbs
        ;;

    factor_omp)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for OpenMP factor test job"
            usage
        fi
        echo "Submitting OpenMP factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_factor_omp.pbs
        ;;

    factor_mpi)
        if [ -z "$PUZZLE_FILE" ]; then
            echo "Error: Puzzle file required for MPI factor test job"
            usage
        fi
        echo "Submitting MPI factor test job for $PUZZLE_FILE..."
        qsub -v PUZZLE_FILE="$PUZZLE_FILE" jobs/futoshiki_factor_mpi.pbs
        ;;
    
    *)
        echo "Error: Unknown job type '$JOB_TYPE'"
        usage
        ;;
esac

echo ""
echo "To check job status: qstat -u $USER"
