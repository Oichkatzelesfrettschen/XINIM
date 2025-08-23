/**
 * @file time.cpp
 * @brief Measure execution time of a command using POSIX primitives.
 * @details Modernized variant of the historical MINIX `time` utility.
 * Uses `std::time` for wall-clock measurement and `times` for process CPU usage.
 *
 * This utility measures the real, user, and system CPU time consumed by a
 * specified command. It forks a child process to execute the command, while
 * the parent process (this `time` utility) waits and records timing
 * information using `std::time` for wall-clock time and `times` for CPU
 * statistics. Signal handling is configured to ignore interrupts during the
 * waiting phase, ensuring accurate measurement.
 *
 * @ingroup utility
 */

#include "../h/const.hpp"
#include "signal.hpp"
#include <array>   // For std::array
#include <cstddef> // For std::size_t
#include <cstdio>  // For std::snprintf (used by std_err implicitly)
#include <cstdlib> // For exit
#include <cstring> // For strlen
#include <ctime>   // For std::time, std::time_t, std::difftime
#include <iostream> // For std::cerr
#include <sys/times.h> // For times
#include <sys/wait.h>  // For wait
#include <unistd.h>    // For fork, execv

// Global variables for command-line arguments and time formatting.
// These are part of the original MINIX design and are maintained for compatibility.
char **args;                       ///< Arguments for the command being timed.
char *name;                        ///< Pointer to the command name.
int digit_seen;                    ///< Tracks whether a non-zero digit has been printed.
char a[12] = {"        . \n"};    ///< Buffer for formatted time output (hh:mm:ss.t).

// Argument array used for `execv`. MAX_ISTACK_BYTES is assumed from a global header.
char *newargs[MAX_ISTACK_BYTES >> 2] = {"/bin/sh"}; /* 256 args */
char pathname[200];                                  ///< Buffer for building candidate pathnames.

// Forward declarations for static helper functions.
static void print_time(const char *mess, long t);
static void twin(int n, char *p);
static void execute();
static void try_path(const char *path);

// Assume std_err is a utility function to print to stderr.
extern void std_err(const char *s);

/**
 * @brief Entry point for the `time` utility.
 *
 * This function parses command-line arguments, forks a child process to execute
 * the specified command, and measures its execution time (real, user, system).
 * It then prints these times to standard error.
 *
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 * @ingroup utility
 */
int main(int argc, char *argv[]) {
    struct time_buf pre_buf, post_buf;
    int status, pid;
    std::time_t start_time_t, end_time_t; // Use std::time_t for wall-clock time
    struct tms pre_tms, post_tms; // For CPU times

    if (argc == 1) return 0;

    args = &argv[1];
    name = argv[1];

    /* Get real time at start of run. */
    std::time(&start_time_t);

    /* Fork off child. */
    if ((pid = fork()) < 0) {
        std_err("Cannot fork\n");
        return 1;
    }

    if (pid == 0) execute(); // Child process executes the command.

    /* Parent is the time program. Disable interrupts and wait. */
    // Signal handlers are set to ignore for robustness during timing.
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    // Record CPU times before waiting (parent's time).
    times(&pre_tms);
    // Wait for the child process to terminate.
    do {
        // Loop to ensure we wait for the correct child if multiple exist.
    } while (wait(&status) != pid);
    std::time(&end_time_t); // Get real time at end of run.

    if ((status & 0377) != 0) {
        std_err("Command terminated abnormally.\n");
    }
    times(&post_tms); // Record CPU times after waiting (parent's time + child's accumulated time).

    /* Print results. */
    // Real time: difference between start and end wall-clock time.
    print_time("real ", static_cast<long>(std::difftime(end_time_t, start_time_t) * HZ));
    // User time: child's user CPU time.
    print_time("user ", static_cast<long>(post_tms.tms_cutime - pre_tms.tms_cutime));
    // System time: child's system CPU time.
    print_time("sys  ", static_cast<long>(post_tms.tms_cstime - pre_tms.tms_cstime));
    
    return status >> 8; // Return child's exit status.
}

/**
 * @brief Pretty-print a time value.
 * @param mess Prefix label (e.g., "real ", "user ", "sys  ").
 * @param t Duration in system ticks (HZ).
 *
 * Converts ticks to `hh:mm:ss.t` format and prints it to standard error.
 */
static void print_time(const char *mess, long t) {
    int hours, minutes, seconds, tenths, i;

    digit_seen = 0;
    for (i = 0; i < 8; i++) a[i] = ' '; // Clear buffer for formatting.

    hours = static_cast<int>(t / (3600L * HZ));
    t -= static_cast<long>(hours) * 3600L * HZ;
    minutes = static_cast<int>(t / (60L * HZ));
    t -= static_cast<long>(minutes) * 60L * HZ;
    seconds = static_cast<int>(t / HZ);
    t -= static_cast<long>(seconds) * HZ;
    tenths = static_cast<int>(t / (HZ / 10L)); // Tenths of a second.

    std_err(mess); // Print the label.

    // Format and print hours, minutes, seconds, and tenths.
    if (hours) { twin(hours, &a[0]); a[2] = ':'; }
    if (minutes || digit_seen) { twin(minutes, &a[3]); a[5] = ':'; }
    if (seconds || digit_seen) twin(seconds, &a[6]);
    else a[7] = '0'; // Default to '0' if seconds is zero and no higher digits seen.
    a[9] = static_cast<char>(tenths + '0'); // Print tenths.
    std_err(a); // Output the formatted time.
}

/**
 * @brief Print a two-digit number into a character buffer.
 * @param n Value to render (0-99).
 * @param p Buffer destination (must have space for 2 chars).
 *
 * This helper function formats an integer as two digits, handling leading
 * spaces if no higher-order digits have been printed yet (`digit_seen`).
 */
static void twin(int n, char *p) {
    char c1, c2;
    c1 = static_cast<char>((n / 10) + '0');
    c2 = static_cast<char>((n % 10) + '0');
    if (digit_seen == 0 && c1 == '0') c1 = ' '; // Suppress leading zero if not needed.
    *p++ = c1;
    *p++ = c2;
    if (n > 0) digit_seen = 1; // Mark that a non-zero digit has been seen.
}

/**
 * @brief Search for a command in standard paths and execute it.
 *
 * This function attempts to execute the command specified by `name` (from `argv[1]`)
 * by trying various common UNIX paths: current directory, `/bin/`, and `/usr/bin/`.
 * If the command is not found or fails to execute, it falls back to `/bin/sh`.
 * If all execution attempts fail, it prints an error and exits.
 */
static void execute() {
    // Try executing the command directly or from common paths.
    try_path("");         // Try local directory.
    try_path("/bin/");    // Search /bin.
    try_path("/usr/bin/"); // Search /usr/bin.

    // If all attempts to execute the command fail, fall back to /bin/sh.
    // The `newargs` array is prepared to pass the original arguments to /bin/sh.
    for (int i = 0; (newargs[i + 1] = args[i]); ++i); // Copy args, newargs[0] remains "/bin/sh"
    execv("/bin/sh", newargs); // Execute /bin/sh with the original command as argument.
    
    // If we reach here, execv failed for /bin/sh as well.
    std_err("Cannot execute /bin/sh\n");
    exit(-1); // Indicate critical failure.
}

/**
 * @brief Attempt to execute a command using a provided path prefix.
 * @param path Directory prefix to try (e.g., "/bin/", "/usr/bin/").
 *
 * This function constructs a full pathname by concatenating the prefix `path`
 * with the command `name` and then attempts to execute it using `execv`.
 */
static void try_path(const char *path) {
    const char *p1 = path;
    char *p2 = pathname;

    // Copy path prefix.
    while (*p1 != 0) *p2++ = *p1++;
    // Copy command name.
    p1 = name;
    while (*p1 != 0) *p2++ = *p1++;
    *p2 = 0; // Null-terminate the full pathname.

    execv(pathname, args); // Attempt to execute the command.
    // If execv returns, it means execution failed.
}