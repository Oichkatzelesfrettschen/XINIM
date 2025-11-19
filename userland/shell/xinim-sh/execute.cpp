/**
 * @file execute.cpp
 * @brief Command execution for xinim-sh
 *
 * Handles command execution including:
 * - Fork and exec
 * - Process group management
 * - Foreground/background control
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
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

namespace xinim::shell {

// External shell state
extern ShellState g_shell;

/**
 * @brief Execute command
 *
 * Handles both built-in and external commands.
 * For external commands, forks and execs.
 *
 * @param cmd Command to execute
 * @return Exit status
 */
int execute_command(Command* cmd) {
    if (!cmd || cmd->argc == 0) {
        return 0;
    }

    // Check for built-in
    if (is_builtin(cmd->args[0])) {
        return execute_builtin(cmd);
    }

    // External command - fork and exec
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process

        // Put child in its own process group
        pid_t child_pid = getpid();
        setpgid(child_pid, child_pid);

        // If foreground job, give terminal to child
        if (!cmd->background && g_shell.interactive) {
            tcsetpgrp(g_shell.shell_terminal, child_pid);
        }

        // Reset signal handlers to default
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // Execute command
        execvp(cmd->args[0], cmd->args);

        // If we get here, exec failed
        fprintf(stderr, "xinim-sh: %s: command not found\n", cmd->args[0]);
        exit(127);
    } else {
        // Parent process

        // Put child in its own process group
        setpgid(pid, pid);

        // Build command string for job table
        char command_str[MAX_COMMAND_LENGTH];
        command_str[0] = '\0';
        for (int i = 0; i < cmd->argc && strlen(command_str) < MAX_COMMAND_LENGTH - 2; i++) {
            if (i > 0) strcat(command_str, " ");
            strncat(command_str, cmd->args[i],
                    MAX_COMMAND_LENGTH - strlen(command_str) - 1);
        }
        if (cmd->background) {
            strcat(command_str, " &");
        }

        // Add job to job table
        int job_id = add_job(pid, pid, command_str, !cmd->background);

        if (cmd->background) {
            // Background job - return immediately
            return 0;
        } else {
            // Foreground job - wait for completion
            Job* job = find_job(job_id);
            if (job) {
                // Give terminal to job
                if (g_shell.interactive) {
                    tcsetpgrp(g_shell.shell_terminal, pid);
                }

                // Wait for job
                wait_for_job(job);

                // Take back terminal
                if (g_shell.interactive) {
                    tcsetpgrp(g_shell.shell_terminal, g_shell.shell_pgid);
                }

                int status = job->exit_status;

                // Remove completed job
                if (job->state == JobState::DONE ||
                    job->state == JobState::TERMINATED) {
                    remove_job(job_id);
                }

                return status;
            }
        }
    }

    return 0;
}

} // namespace xinim::shell
