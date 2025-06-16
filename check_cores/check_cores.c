#include <stdio.h>
#include <omp.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

int main() {
    int nprocs = 0;
    int max_threads = 0;
    
    printf("===== System Core Information =====\n");
    
    // Get number of processors from sysconf
    nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    printf("Number of processors online (sysconf): %d\n", nprocs);
    
    // Try to get from sysinfo
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        printf("Number of processors (sysinfo): %ld\n", info.procs);
    }
    
    // Get OpenMP threads
    #pragma omp parallel
    {
        #pragma omp single
        {
            max_threads = omp_get_num_threads();
            printf("OpenMP threads available: %d\n", max_threads);
        }
        
        // Show thread IDs
        #pragma omp critical
        {
            printf("Thread %d/%d running\n", 
                   omp_get_thread_num(), omp_get_num_threads());
        }
    }
    
    printf("Max threads (omp_get_max_threads): %d\n", omp_get_max_threads());
    
    // Check if OMP_NUM_THREADS is set
    char* env_threads = getenv("OMP_NUM_THREADS");
    if (env_threads) {
        printf("OMP_NUM_THREADS environment variable: %s\n", env_threads);
    } else {
        printf("OMP_NUM_THREADS is not set\n");
    }
    
    printf("===================================\n");
    
    return 0;
}