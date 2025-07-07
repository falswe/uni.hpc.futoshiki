#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

// Defines the verbosity of the log output.
// A given level will print messages at that level and all levels above it.
// e.g., LOG_INFO will print INFO, ESSENTIAL, WARN, and ERROR.
typedef enum {
    LOG_DEBUG = 0,  // Detailed messages for debugging the algorithm
    LOG_VERBOSE,    // Progress updates, task assignments
    LOG_INFO,       // Standard informational messages (default)
    LOG_ESSENTIAL,  // Final results, crucial summary output
    LOG_WARN,       // Warnings about potential issues
    LOG_ERROR,      // Critical errors that may halt execution
    LOG_NONE        // No logging output
} LogLevel;

/**
 * Initializes the logger with a specific minimum level.
 * @param level The minimum level of messages to display.
 */
void logger_init(LogLevel level);

/**
 * The core logging function.
 * It's recommended to use the macros below (log_info, log_error, etc.).
 *
 * @param level The level of this specific message.
 * @param format The printf-style format string.
 * @param ... Additional arguments for the format string.
 */
void log_message(LogLevel level, const char* format, ...);

// --- Convenience Macros ---
// These macros make it easy to log at a specific level.
#define log_debug(...) log_message(LOG_DEBUG, __VA_ARGS__)
#define log_verbose(...) log_message(LOG_VERBOSE, __VA_ARGS__)
#define log_info(...) log_message(LOG_INFO, __VA_ARGS__)
#define log_essential(...) log_message(LOG_ESSENTIAL, __VA_ARGS__)
#define log_warn(...) log_message(LOG_WARN, __VA_ARGS__)
#define log_error(...) log_message(LOG_ERROR, __VA_ARGS__)

#endif  // LOGGER_H