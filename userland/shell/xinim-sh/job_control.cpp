/**
 * @file job_control.cpp
 * @brief Job control implementation for xinim-sh
 *
 * Implements job management, foreground/background control,
 * and job status tracking.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include "shell.hpp"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

namespace xinim::shell {

// External shell state
extern ShellState g_shell;

// ============================================================================
// Job Management
// ============================================================================

/**
 * @brief Add a new job to the job table
 *
 * @param pid Process ID
 * @param pgid Process group ID
 * @param command Command string
 * @param foreground Is foreground job?
 * @return Job ID, or -1 on error
 */
int add_job(pid_t pid, pid_t pgid, const char* command, bool foreground) {
    // Find free slot
    int job_id = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_shell.jobs[i].state == JobState::DONE ||
            g_shell.jobs[i].pgid == 0) {
            job_id = i + 1;  // Job IDs are 1-indexed
            break;
        }
    }

    if (job_id == -1) {
        fprintf(stderr, "xinim-sh: job table full\n");
        return -1;
    }

    // Initialize job
    Job* job = &g_shell.jobs[job_id - 1];
    job->job_id = job_id;
    job->pgid = pgid;
    job->pid = pid;
    job->state = JobState::RUNNING;
    job->is_foreground = foreground;
    job->exit_status = 0;

    strncpy(job->command, command, MAX_COMMAND_LENGTH - 1);
    job->command[MAX_COMMAND_LENGTH - 1] = '\0';

    g_shell.job_count++;

    // Print job notification for background jobs
    if (!foreground) {
        printf("[%d] %d\n", job_id, pid);
    }

    return job_id;
}

/**
 * @brief Find job by job ID
 */
Job* find_job(int job_id) {
    if (job_id < 1 || job_id > MAX_JOBS) {
        return nullptr;
    }

    Job* job = &g_shell.jobs[job_id - 1];
    if (job->pgid == 0 || job->state == JobState::DONE) {
        return nullptr;
    }

    return job;
}

/**
 * @brief Find job by process group ID
 */
Job* find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_shell.jobs[i].pgid == pgid &&
            g_shell.jobs[i].state != JobState::DONE) {
            return &g_shell.jobs[i];
        }
    }
    return nullptr;
}

/**
 * @brief Remove job from job table
 */
void remove_job(int job_id) {
    if (job_id < 1 || job_id > MAX_JOBS) {
        return;
    }

    Job* job = &g_shell.jobs[job_id - 1];
    job->pgid = 0;
    job->pid = 0;
    job->state = JobState::DONE;
    job->is_foreground = false;

    if (g_shell.job_count > 0) {
        g_shell.job_count--;
    }
}

/**
 * @brief Update status of all jobs
 *
 * Checks for terminated/stopped jobs using waitpid.
 */
void update_job_status() {
    for (int i = 0; i < MAX_JOBS; i++) {
        Job* job = &g_shell.jobs[i];

        if (job->pgid == 0 || job->state == JobState::DONE) {
            continue;
        }

        int status;
        pid_t result = waitpid(-job->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (result == 0) {
            // No status change
            continue;
        } else if (result == -1) {
            // Error or no such process group
            continue;
        }

        // Process status changed
        if (WIFEXITED(status)) {
            job->state = JobState::DONE;
            job->exit_status = WEXITSTATUS(status);

            if (!job->is_foreground) {
                printf("[%d]  Done                    %s\n",
                       job->job_id, job->command);
            }
        } else if (WIFSIGNALED(status)) {
            job->state = JobState::TERMINATED;
            job->exit_status = WTERMSIG(status);

            if (!job->is_foreground) {
                printf("[%d]  Terminated              %s\n",
                       job->job_id, job->command);
            }
        } else if (WIFSTOPPED(status)) {
            job->state = JobState::STOPPED;

            if (!job->is_foreground) {
                printf("[%d]  Stopped                 %s\n",
                       job->job_id, job->command);
            } else {
                printf("[%d]+  Stopped                 %s\n",
                       job->job_id, job->command);
            }
        } else if (WIFCONTINUED(status)) {
            job->state = JobState::RUNNING;
        }
    }
}

/**
 * @brief List all jobs
 */
void list_jobs() {
    bool found = false;

    for (int i = 0; i < MAX_JOBS; i++) {
        Job* job = &g_shell.jobs[i];

        if (job->pgid == 0 || job->state == JobState::DONE) {
            continue;
        }

        found = true;

        const char* state_str;
        switch (job->state) {
            case JobState::RUNNING:
                state_str = "Running";
                break;
            case JobState::STOPPED:
                state_str = "Stopped";
                break;
            case JobState::DONE:
                state_str = "Done";
                break;
            case JobState::TERMINATED:
                state_str = "Terminated";
                break;
            default:
                state_str = "Unknown";
                break;
        }

        printf("[%d]  %-20s %s\n", job->job_id, state_str, job->command);
    }

    if (!found) {
        printf("No jobs\n");
    }
}

// ============================================================================
// Foreground/Background Control
// ============================================================================

/**
 * @brief Bring job to foreground
 *
 * @param job Job to bring to foreground
 * @return 0 on success, -1 on error
 */
int bring_to_foreground(Job* job) {
    if (!job) {
        return -1;
    }

    // Give terminal to job's process group
    if (g_shell.interactive) {
        tcsetpgrp(g_shell.shell_terminal, job->pgid);
    }

    // Continue job if stopped
    if (job->state == JobState::STOPPED) {
        kill(-job->pgid, SIGCONT);
    }

    job->is_foreground = true;
    job->state = JobState::RUNNING;

    // Wait for job
    wait_for_job(job);

    // Return terminal to shell
    if (g_shell.interactive) {
        tcsetpgrp(g_shell.shell_terminal, g_shell.shell_pgid);
    }

    return 0;
}

/**
 * @brief Send job to background
 *
 * @param job Job to send to background
 * @return 0 on success, -1 on error
 */
int send_to_background(Job* job) {
    if (!job) {
        return -1;
    }

    // Continue job if stopped
    if (job->state == JobState::STOPPED) {
        kill(-job->pgid, SIGCONT);
        printf("[%d]  %s &\n", job->job_id, job->command);
    }

    job->is_foreground = false;
    job->state = JobState::RUNNING;

    return 0;
}

/**
 * @brief Wait for foreground job to complete or stop
 *
 * @param job Job to wait for
 */
void wait_for_job(Job* job) {
    if (!job) {
        return;
    }

    int status;
    pid_t result;

    do {
        result = waitpid(-job->pgid, &status, WUNTRACED);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        return;
    }

    // Update job status
    if (WIFEXITED(status)) {
        job->state = JobState::DONE;
        job->exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        job->state = JobState::TERMINATED;
        job->exit_status = WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        job->state = JobState::STOPPED;
        printf("[%d]+  Stopped                 %s\n",
               job->job_id, job->command);
    }
}

} // namespace xinim::shell
