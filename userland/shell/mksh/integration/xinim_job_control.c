/**
 * @file xinim_job_control.c
 * @brief POSIX job control implementation for mksh on XINIM
 * 
 * Implements process groups, foreground/background job management,
 * and terminal ownership for job control in the shell.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <syscall.h>
#include <stdio.h>

/* Process group management */

/**
 * @brief Set process group ID
 */
int xinim_setpgid(pid_t pid, pid_t pgid) {
    /* XINIM syscall for setpgid */
    /* Syscall number would be defined in syscall table */
    return syscall(__NR_setpgid, pid, pgid);
}

/**
 * @brief Get process group ID
 */
pid_t xinim_getpgid(pid_t pid) {
    return syscall(__NR_getpgid, pid);
}

/**
 * @brief Get process group of current process
 */
pid_t xinim_getpgrp(void) {
    return xinim_getpgid(0);
}

/* Terminal ownership */

/**
 * @brief Set foreground process group of terminal
 */
int xinim_tcsetpgrp(int fd, pid_t pgrp) {
    return ioctl(fd, TIOCSPGRP, &pgrp);
}

/**
 * @brief Get foreground process group of terminal
 */
pid_t xinim_tcgetpgrp(int fd) {
    pid_t pgrp;
    if (ioctl(fd, TIOCGPGRP, &pgrp) < 0)
        return -1;
    return pgrp;
}

/* Session management */

/**
 * @brief Create a new session
 */
pid_t xinim_setsid(void) {
    return syscall(__NR_setsid);
}

/**
 * @brief Get session ID
 */
pid_t xinim_getsid(pid_t pid) {
    return syscall(__NR_getsid, pid);
}

/* Job control operations */

/**
 * @brief Put a job in foreground
 * 
 * @param pgrp Process group ID
 * @param terminal_fd Terminal file descriptor
 * @param cont Send SIGCONT if stopped
 * @return 0 on success, -1 on error
 */
int xinim_job_foreground(pid_t pgrp, int terminal_fd, int cont) {
    struct termios term_state;
    
    /* Save current terminal state */
    if (tcgetattr(terminal_fd, &term_state) < 0)
        return -1;
    
    /* Give terminal to the job */
    if (xinim_tcsetpgrp(terminal_fd, pgrp) < 0)
        return -1;
    
    /* Continue job if stopped */
    if (cont) {
        if (kill(-pgrp, SIGCONT) < 0)
            return -1;
    }
    
    return 0;
}

/**
 * @brief Put a job in background
 * 
 * @param pgrp Process group ID
 * @param terminal_fd Terminal file descriptor
 * @param cont Send SIGCONT if stopped
 * @return 0 on success, -1 on error
 */
int xinim_job_background(pid_t pgrp, int terminal_fd, int cont) {
    pid_t shell_pgrp;
    
    /* Get shell's process group */
    shell_pgrp = xinim_getpgrp();
    
    /* Give terminal back to shell */
    if (xinim_tcsetpgrp(terminal_fd, shell_pgrp) < 0)
        return -1;
    
    /* Continue job if stopped */
    if (cont) {
        if (kill(-pgrp, SIGCONT) < 0)
            return -1;
    }
    
    return 0;
}

/**
 * @brief Stop a job
 * 
 * @param pgrp Process group ID
 * @return 0 on success, -1 on error
 */
int xinim_job_stop(pid_t pgrp) {
    return kill(-pgrp, SIGSTOP);
}

/**
 * @brief Continue a stopped job
 * 
 * @param pgrp Process group ID
 * @return 0 on success, -1 on error
 */
int xinim_job_continue(pid_t pgrp) {
    return kill(-pgrp, SIGCONT);
}

/**
 * @brief Terminate a job
 * 
 * @param pgrp Process group ID
 * @return 0 on success, -1 on error
 */
int xinim_job_kill(pid_t pgrp) {
    return kill(-pgrp, SIGKILL);
}

/**
 * @brief Wait for job state change
 * 
 * @param pid Process ID (-1 for any child)
 * @param status Status pointer
 * @param options Wait options (WNOHANG, WUNTRACED, WCONTINUED)
 * @return PID of changed process, 0 if WNOHANG and no change, -1 on error
 */
pid_t xinim_job_wait(pid_t pid, int *status, int options) {
    return waitpid(pid, status, options);
}

/* Shell initialization for job control */

/**
 * @brief Initialize shell for job control
 * 
 * @param terminal_fd Terminal file descriptor
 * @return 0 on success, -1 on error
 */
int xinim_shell_init_job_control(int terminal_fd) {
    pid_t shell_pgid;
    
    /* Make sure we're in the foreground */
    while (xinim_tcgetpgrp(terminal_fd) != (shell_pgid = xinim_getpgrp())) {
        kill(-shell_pgid, SIGTTIN);
    }
    
    /* Ignore job control signals */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    
    /* Put shell in its own process group */
    shell_pgid = getpid();
    if (xinim_setpgid(shell_pgid, shell_pgid) < 0) {
        perror("setpgid");
        return -1;
    }
    
    /* Grab control of terminal */
    if (xinim_tcsetpgrp(terminal_fd, shell_pgid) < 0) {
        perror("tcsetpgrp");
        return -1;
    }
    
    return 0;
}

/* POSIX compatibility wrappers */

int setpgid(pid_t pid, pid_t pgid) {
    return xinim_setpgid(pid, pgid);
}

pid_t getpgid(pid_t pid) {
    return xinim_getpgid(pid);
}

pid_t getpgrp(void) {
    return xinim_getpgrp();
}

int tcsetpgrp(int fd, pid_t pgrp) {
    return xinim_tcsetpgrp(fd, pgrp);
}

pid_t tcgetpgrp(int fd) {
    return xinim_tcgetpgrp(fd);
}

pid_t setsid(void) {
    return xinim_setsid();
}

pid_t getsid(pid_t pid) {
    return xinim_getsid(pid);
}
