/**
 * @file main.cpp
 * @brief Main entry point and loop for xinim-sh
 *
 * Implements:
 * - Shell initialization
 * - Main read-eval-print loop
 * - Signal handling
 * - Terminal control
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include "shell.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>

namespace xinim::shell {

// ============================================================================
// Global Shell State
// ============================================================================

ShellState g_shell;

// ============================================================================
// Signal Handlers
// ============================================================================

/**
 * @brief SIGCHLD handler - Update job status when child changes state
 */
void handle_sigchld(int sig) {
    (void)sig;  // Unused

    // Save errno
    int saved_errno = errno;

    // Update job status
    update_job_status();

    // Restore errno
    errno = saved_errno;
}

/**
 * @brief SIGINT handler - Ignore in shell (let foreground job handle it)
 */
void handle_sigint(int sig) {
    (void)sig;  // Unused

    // Shell ignores SIGINT - it goes to foreground process group
    // Just print newline for clean prompt
    write(STDOUT_FILENO, "\n", 1);
}

/**
 * @brief SIGTSTP handler - Ignore in shell (let foreground job handle it)
 */
void handle_sigtstp(int sig) {
    (void)sig;  // Unused

    // Shell ignores SIGTSTP - it goes to foreground process group
}

/**
 * @brief Set up shell signal handlers
 */
void setup_shell_signals() {
    struct sigaction sa;

    // SIGCHLD - reap children and update job status
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("sigaction(SIGCHLD)");
    }

    // SIGINT - ignore (foreground job gets it)
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        perror("sigaction(SIGINT)");
    }

    // SIGTSTP - ignore (foreground job gets it)
    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa, nullptr) == -1) {
        perror("sigaction(SIGTSTP)");
    }

    // SIGQUIT - ignore
    signal(SIGQUIT, SIG_IGN);

    // SIGTTIN - ignore (we're foreground)
    signal(SIGTTIN, SIG_IGN);

    // SIGTTOU - ignore (we're foreground)
    signal(SIGTTOU, SIG_IGN);
}

// ============================================================================
// Shell Initialization
// ============================================================================

/**
 * @brief Initialize shell state
 */
void init_shell() {
    // Clear shell state
    memset(&g_shell, 0, sizeof(g_shell));

    // Initialize job table
    for (int i = 0; i < MAX_JOBS; i++) {
        g_shell.jobs[i].pgid = 0;
        g_shell.jobs[i].state = JobState::DONE;
    }

    g_shell.job_count = 0;
    g_shell.running = true;

    // Check if interactive
    g_shell.shell_terminal = STDIN_FILENO;
    g_shell.interactive = isatty(g_shell.shell_terminal);

    if (g_shell.interactive) {
        // Wait until we're in the foreground
        while (tcgetpgrp(g_shell.shell_terminal) != (pid_t)getpgrp()) {
            kill(-getpgrp(), SIGTTIN);
        }

        // Put shell in its own process group
        g_shell.shell_pgid = getpid();
        if (setpgid(g_shell.shell_pgid, g_shell.shell_pgid) < 0) {
            perror("setpgid");
            exit(1);
        }

        // Grab control of the terminal
        tcsetpgrp(g_shell.shell_terminal, g_shell.shell_pgid);

        // Save terminal attributes
        // (We're not implementing full terminal control for now)
    }

    // Set up signal handlers
    setup_shell_signals();
}

/**
 * @brief Cleanup shell state
 */
void cleanup_shell() {
    // Kill all running jobs
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_shell.jobs[i].pgid != 0 &&
            (g_shell.jobs[i].state == JobState::RUNNING ||
             g_shell.jobs[i].state == JobState::STOPPED)) {
            kill(-g_shell.jobs[i].pgid, SIGHUP);
        }
    }
}

// ============================================================================
// Main Shell Loop
// ============================================================================

/**
 * @brief Main shell loop
 *
 * @return Exit status
 */
int shell_main() {
    // Initialize shell
    init_shell();

    // Print welcome message
    if (g_shell.interactive) {
        printf("XINIM Shell (xinim-sh) version 1.0\n");
        printf("Type 'help' for help, 'exit' to exit\n\n");
    }

    char line[MAX_COMMAND_LENGTH];
    Command cmd;

    // Main loop
    while (g_shell.running) {
        // Update job status
        update_job_status();

        // Print prompt
        if (g_shell.interactive) {
            printf("xinim-sh$ ");
            fflush(stdout);
        }

        // Read command
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                // EOF - exit
                if (g_shell.interactive) {
                    printf("\n");
                }
                break;
            } else {
                // Error
                perror("fgets");
                continue;
            }
        }

        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Skip empty lines
        if (line[0] == '\0') {
            continue;
        }

        // Parse command
        if (!parse_command(line, &cmd)) {
            continue;
        }

        // Execute command
        execute_command(&cmd);

        // Free command
        free_command(&cmd);
    }

    // Cleanup
    cleanup_shell();

    if (g_shell.interactive) {
        printf("Goodbye!\n");
    }

    return 0;
}

} // namespace xinim::shell

// ============================================================================
// Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;  // Unused
    (void)argv;  // Unused

    return xinim::shell::shell_main();
}
