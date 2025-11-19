/**
 * @file builtins.cpp
 * @brief Built-in commands for xinim-sh
 *
 * Implements shell built-in commands:
 * - cd: Change directory
 * - exit: Exit shell
 * - jobs: List jobs
 * - fg: Bring job to foreground
 * - bg: Send job to background
 * - help: Show help
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include "shell.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace xinim::shell {

// External shell state
extern ShellState g_shell;

/**
 * @brief Check if command is a built-in
 */
bool is_builtin(const char* command) {
    if (!command) return false;

    return strcmp(command, "cd") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "jobs") == 0 ||
           strcmp(command, "fg") == 0 ||
           strcmp(command, "bg") == 0 ||
           strcmp(command, "help") == 0;
}

/**
 * @brief Execute built-in command
 *
 * @param cmd Command to execute
 * @return Exit status
 */
int execute_builtin(Command* cmd) {
    if (!cmd || !cmd->args[0]) {
        return -1;
    }

    const char* name = cmd->args[0];

    if (strcmp(name, "cd") == 0) {
        return builtin_cd(cmd);
    } else if (strcmp(name, "exit") == 0) {
        return builtin_exit(cmd);
    } else if (strcmp(name, "jobs") == 0) {
        return builtin_jobs(cmd);
    } else if (strcmp(name, "fg") == 0) {
        return builtin_fg(cmd);
    } else if (strcmp(name, "bg") == 0) {
        return builtin_bg(cmd);
    } else if (strcmp(name, "help") == 0) {
        return builtin_help(cmd);
    }

    return -1;
}

// ============================================================================
// Built-in Command Implementations
// ============================================================================

/**
 * @brief cd - Change directory
 */
int builtin_cd(Command* cmd) {
    const char* dir = cmd->argc > 1 ? cmd->args[1] : getenv("HOME");

    if (!dir) {
        fprintf(stderr, "cd: HOME not set\n");
        return 1;
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }

    return 0;
}

/**
 * @brief exit - Exit shell
 */
int builtin_exit(Command* cmd) {
    int status = 0;

    if (cmd->argc > 1) {
        status = atoi(cmd->args[1]);
    }

    // Clean up jobs
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_shell.jobs[i].pgid != 0 &&
            g_shell.jobs[i].state == JobState::RUNNING) {
            fprintf(stderr, "xinim-sh: There are running jobs\n");
            list_jobs();
            return 1;
        }
    }

    g_shell.running = false;
    exit(status);
}

/**
 * @brief jobs - List all jobs
 */
int builtin_jobs(Command* cmd) {
    (void)cmd;  // Unused

    update_job_status();
    list_jobs();

    return 0;
}

/**
 * @brief fg - Bring job to foreground
 */
int builtin_fg(Command* cmd) {
    int job_id;

    // Get job ID
    if (cmd->argc > 1) {
        // Parse %N or N
        const char* arg = cmd->args[1];
        if (arg[0] == '%') {
            job_id = atoi(arg + 1);
        } else {
            job_id = atoi(arg);
        }
    } else {
        // Find most recent job
        job_id = -1;
        for (int i = MAX_JOBS - 1; i >= 0; i--) {
            if (g_shell.jobs[i].pgid != 0 &&
                g_shell.jobs[i].state != JobState::DONE) {
                job_id = i + 1;
                break;
            }
        }

        if (job_id == -1) {
            fprintf(stderr, "fg: no current job\n");
            return 1;
        }
    }

    // Find job
    Job* job = find_job(job_id);
    if (!job) {
        fprintf(stderr, "fg: %%%d: no such job\n", job_id);
        return 1;
    }

    printf("%s\n", job->command);

    // Bring to foreground
    return bring_to_foreground(job);
}

/**
 * @brief bg - Send job to background
 */
int builtin_bg(Command* cmd) {
    int job_id;

    // Get job ID
    if (cmd->argc > 1) {
        // Parse %N or N
        const char* arg = cmd->args[1];
        if (arg[0] == '%') {
            job_id = atoi(arg + 1);
        } else {
            job_id = atoi(arg);
        }
    } else {
        // Find most recent stopped job
        job_id = -1;
        for (int i = MAX_JOBS - 1; i >= 0; i--) {
            if (g_shell.jobs[i].pgid != 0 &&
                g_shell.jobs[i].state == JobState::STOPPED) {
                job_id = i + 1;
                break;
            }
        }

        if (job_id == -1) {
            fprintf(stderr, "bg: no stopped jobs\n");
            return 1;
        }
    }

    // Find job
    Job* job = find_job(job_id);
    if (!job) {
        fprintf(stderr, "bg: %%%d: no such job\n", job_id);
        return 1;
    }

    // Send to background
    return send_to_background(job);
}

/**
 * @brief help - Show help
 */
int builtin_help(Command* cmd) {
    (void)cmd;  // Unused

    printf("XINIM Shell (xinim-sh) - Version 1.0\n");
    printf("\n");
    printf("Built-in commands:\n");
    printf("  cd [dir]         Change directory\n");
    printf("  exit [n]         Exit shell with status n\n");
    printf("  jobs             List active jobs\n");
    printf("  fg [%%n]          Bring job n to foreground\n");
    printf("  bg [%%n]          Send job n to background\n");
    printf("  help             Show this help\n");
    printf("\n");
    printf("Job control:\n");
    printf("  command &        Run command in background\n");
    printf("  Ctrl+C           Terminate foreground job (SIGINT)\n");
    printf("  Ctrl+Z           Stop foreground job (SIGTSTP)\n");
    printf("\n");

    return 0;
}

} // namespace xinim::shell
