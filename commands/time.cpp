/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* time - time a command        Authors: Andy Tanenbaum & Michiel Huisjes */

/**
 * @file time.cpp
 * @brief Measure execution time of a command using POSIX primitives.
 * @details Modernized variant of the historical MINIX `time` utility.
 * Uses `std::time` for wall-clock measurement and `times` for process CPU usage.
 */

#include "../h/const.hpp"
#include "signal.hpp"
#include <ctime> ///< for std::time [C++23 §28.7]

/// Arguments for the command being timed.
char **args;
/// Pointer to the command name.
char *name;

/// Aggregate of user and system times.
struct time_buf {
    long user, sys;      ///< CPU time of current process.
    long childu, childs; ///< CPU time of child process.
};

/// Tracks whether a non-zero digit has been printed.
int digit_seen;
/// Buffer for formatted time output.
char a[12] = {"        . \n"};

/// Argument array used for `execv`.
char *newargs[MAX_ISTACK_BYTES >> 2] = {"/bin/sh"}; /* 256 args */
/// Buffer for building candidate pathnames.
char pathname[200];

/**
 * @brief Program entry point.
 * @param argc Number of command-line arguments.
 * @param argv Argument vector.
 * @return Exit status of the timed command.
 */
int main(int argc, char *argv[]) {

    struct time_buf pre_buf, post_buf;
    int status, pid;
    long start_time, end_time;

    if (argc == 1)
        return 0;

    args = &argv[1];
    name = argv[1];

    /* Get real time at start of run. */
    std::time(&start_time);

    /* Fork off child. */
    if ((pid = fork()) < 0) {
        std_err("Cannot fork\n");
        return 1;
    }

    if (pid == 0)
        execute();

    /* Parent is the time program.  Disable interrupts and wait. */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    do {
        times(&pre_buf);
    } while (wait(&status) != pid);
    std::time(&end_time);

    if ((status & 0377) != 0)
        std_err("Command terminated abnormally.\n");
    times(&post_buf);

    /* Print results. */
    print_time("real ", (end_time - start_time) * HZ);
    print_time("user ", post_buf.childu - pre_buf.childu);
    print_time("sys  ", post_buf.childs - pre_buf.childs);
    return status >> 8;
}

/**
 * @brief Pretty-print a time value.
 * @param mess Prefix label.
 * @param t Duration in system ticks.
 *
 * Converts ticks to `hh:mm:ss.t` format.
 */
static void print_time(char *mess, long t) {
    /* Print the time 't' in hours: minutes: seconds.  't' is in ticks. */
    int hours, minutes, seconds, tenths, i;

    digit_seen = 0;
    for (i = 0; i < 8; i++)
        a[i] = ' ';
    hours = (int)(t / (3600L * (long)HZ));
    t -= (long)hours * 3600L * (long)HZ;
    minutes = (int)(t / (60L * (long)HZ));
    t -= (long)minutes * 60L * (long)HZ;
    seconds = (int)(t / (long)HZ);
    t -= (long)seconds * (long)HZ;
    tenths = (int)(t / ((long)HZ / 10L));

    std_err(mess);

    if (hours) {
        twin(hours, &a[0]);
        a[2] = ':';
    }
    if (minutes || digit_seen) {
        twin(minutes, &a[3]);
        a[5] = ':';
    }
    if (seconds || digit_seen)
        twin(seconds, &a[6]);
    else
        a[7] = '0';
    a[9] = tenths + '0';
    std_err(a);
}

/**
 * @brief Print a two-digit number.
 * @param n Value to render.
 * @param p Buffer destination.
 */
static void twin(int n, char *p) {
    char c1, c2;
    c1 = (n / 10) + '0';
    c2 = (n % 10) + '0';
    if (digit_seen == 0 && c1 == '0')
        c1 = ' ';
    *p++ = c1;
    *p++ = c2;
    if (n > 0)
        digit_seen = 1;
}

/**
 * @brief Search for command and execute it.
 *
 * Tries common PATH prefixes and finally falls back to `/bin/sh`.
 */
static void execute() {
    register int i;

    try_path("");          /* try local directory */
    try_path("/bin/");     // search /bin
    try_path("/usr/bin/"); // search /usr/bin

    for (i = 0; newargs[i + 1] = args[i]; i++)
        ;
    execv("/bin/sh", newargs);
    std_err("Cannot execute /bin/sh\n");

    exit(-1);
}

/**
 * @brief Attempt to execute using the provided path prefix.
 * @param path Directory prefix to try.
 */
static void try_path(const char *path) {
    const char *p1 = path;
    char *p2 = pathname;

    while (*p1 != 0)
        *p2++ = *p1++;
    p1 = name;
    while (*p1 != 0)
        *p2++ = *p1++;
    *p2 = 0;
    execv(pathname, args);
}
