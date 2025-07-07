#include "logger.h"

#include <stdio.h>

// Global variable to store the current logging level threshold.
static LogLevel g_log_level = LOG_INFO;  // Default level

// Weak symbols for MPI compatibility. They will be defined by the MPI implementation.
__attribute__((weak)) int g_mpi_rank = 0;
__attribute__((weak)) int g_mpi_size = 1;

void logger_init(LogLevel level) { g_log_level = level; }

void log_message(LogLevel level, const char* format, ...) {
    // Filter messages based on the global log level.
    if (level < g_log_level) {
        return;
    }

    // In MPI, only rank 0 should print non-critical messages to avoid spam.
    // Warnings and errors can be printed by any rank.
    if (g_mpi_size > 1 && g_mpi_rank != 0 && level < LOG_WARN) {
        return;
    }

    // Determine the output stream (stderr for warnings/errors, stdout for others).
    FILE* stream = (level >= LOG_WARN) ? stderr : stdout;

    // Create the log prefix string.
    const char* level_str;
    switch (level) {
        case LOG_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_VERBOSE:
            level_str = "VERBOSE";
            break;
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_ESSENTIAL:
            level_str = "RESULT";
            break;
        case LOG_WARN:
            level_str = "WARN";
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "LOG";
            break;
    }

    // Print the prefix, including MPI rank if applicable.
    if (g_mpi_size > 1) {
        fprintf(stream, "[%s][RANK %d] ", level_str, g_mpi_rank);
    } else {
        fprintf(stream, "[%s] ", level_str);
    }

    // Print the actual message using varargs.
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    // Print a newline and flush the buffer to ensure immediate output.
    fprintf(stream, "\n");
    fflush(stream);
}