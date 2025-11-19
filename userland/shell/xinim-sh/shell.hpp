/**
 * @file shell.hpp
 * @brief XINIM Shell (xinim-sh) - Minimal shell with job control
 *
 * A simple POSIX-compliant shell for XINIM supporting:
 * - Command execution (foreground and background)
 * - Job control (fg, bg, jobs)
 * - Built-in commands (cd, exit, etc.)
 * - Signal handling (Ctrl+C, Ctrl+Z)
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_SHELL_HPP
#define XINIM_SHELL_HPP

#include <cstdint>
#include <sys/types.h>

namespace xinim::shell {

// ============================================================================
// Constants
// ============================================================================

constexpr size_t MAX_COMMAND_LENGTH = 1024;
constexpr size_t MAX_ARGS = 64;
constexpr size_t MAX_JOBS = 32;

// ============================================================================
// Job States
// ============================================================================

enum class JobState {
    RUNNING,     ///< Job is currently running
    STOPPED,     ///< Job was stopped (Ctrl+Z)
    DONE,        ///< Job has completed
    TERMINATED   ///< Job was terminated by signal
};

// ============================================================================
// Job Structure
// ============================================================================

/**
 * @brief Represents a job (process or pipeline)
 *
 * A job is a single command or pipeline that can be managed
 * as a unit (foreground, background, stopped, etc.)
 */
struct Job {
    int job_id;                      ///< Job number (1, 2, 3, ...)
    pid_t pgid;                      ///< Process group ID
    pid_t pid;                       ///< Process ID (for single commands)
    JobState state;                  ///< Current state
    char command[MAX_COMMAND_LENGTH]; ///< Command string (for display)
    bool is_foreground;              ///< Is this the foreground job?
    int exit_status;                 ///< Exit status (if done)
};

// ============================================================================
// Command Structure
// ============================================================================

/**
 * @brief Parsed command
 */
struct Command {
    char* args[MAX_ARGS];            ///< Null-terminated argument array
    int argc;                        ///< Argument count
    bool background;                 ///< Should run in background?
};

// ============================================================================
// Shell State
// ============================================================================

/**
 * @brief Global shell state
 */
struct ShellState {
    Job jobs[MAX_JOBS];              ///< Job table
    int job_count;                   ///< Number of active jobs
    pid_t shell_pgid;                ///< Shell's process group ID
    int shell_terminal;              ///< Shell's controlling terminal FD
    bool interactive;                ///< Is shell interactive?
    bool running;                    ///< Is shell running?
};

// ============================================================================
// Function Declarations
// ============================================================================

// Main shell loop
int shell_main();

// Command parsing
bool parse_command(const char* input, Command* cmd);
void free_command(Command* cmd);

// Job management
int add_job(pid_t pid, pid_t pgid, const char* command, bool foreground);
Job* find_job(int job_id);
Job* find_job_by_pgid(pid_t pgid);
void remove_job(int job_id);
void update_job_status();
void list_jobs();

// Command execution
int execute_command(Command* cmd);
int execute_builtin(Command* cmd);
bool is_builtin(const char* command);

// Built-in commands
int builtin_cd(Command* cmd);
int builtin_exit(Command* cmd);
int builtin_jobs(Command* cmd);
int builtin_fg(Command* cmd);
int builtin_bg(Command* cmd);
int builtin_help(Command* cmd);

// Job control
int bring_to_foreground(Job* job);
int send_to_background(Job* job);
void wait_for_job(Job* job);

// Signal handling
void setup_shell_signals();
void handle_sigchld(int sig);
void handle_sigint(int sig);
void handle_sigtstp(int sig);

// Initialization
void init_shell();
void cleanup_shell();

} // namespace xinim::shell

#endif // XINIM_SHELL_HPP
