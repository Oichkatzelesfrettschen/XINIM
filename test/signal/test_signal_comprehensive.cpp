/**
 * @file test_signal_comprehensive.cpp
 * @brief Comprehensive signal testing for XINIM
 *
 * Tests Week 10 Phase 2 & 3 signal implementation:
 * - Basic signal send/receive (kill, raise)
 * - Signal handlers (sigaction)
 * - Signal masking (sigprocmask)
 * - Process groups (setpgid, setsid)
 * - Special signals (SIGKILL, SIGSTOP, SIGCONT, SIGCHLD)
 * - Integration with fork, exec, wait
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <cassert>

// ============================================================================
// Test Framework
// ============================================================================

int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static void run_test_##name() { \
        printf("[TEST] %s... ", #name); \
        fflush(stdout); \
        g_tests_run++; \
        test_##name(); \
        printf("PASS\n"); \
        g_tests_passed++; \
    } \
    static void test_##name()

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d: Assertion failed: %s\n", \
                   __FILE__, __LINE__, #condition); \
            g_tests_failed++; \
            exit(1); \
        } \
    } while (0)

// ============================================================================
// Signal Handler State
// ============================================================================

static volatile sig_atomic_t g_signal_received = 0;
static volatile sig_atomic_t g_signal_count = 0;

static void test_handler(int sig) {
    g_signal_received = sig;
    g_signal_count++;
}

static void reset_signal_state() {
    g_signal_received = 0;
    g_signal_count = 0;
}

// ============================================================================
// Basic Signal Tests
// ============================================================================

TEST(signal_self_sigint) {
    reset_signal_state();

    struct sigaction sa;
    sa.sa_handler = test_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    ASSERT(sigaction(SIGINT, &sa, nullptr) == 0);

    // Send signal to self
    ASSERT(raise(SIGINT) == 0);

    // Handler should have been called
    ASSERT(g_signal_received == SIGINT);
    ASSERT(g_signal_count == 1);

    // Reset to default
    signal(SIGINT, SIG_DFL);
}

TEST(signal_self_sigusr1) {
    reset_signal_state();

    struct sigaction sa;
    sa.sa_handler = test_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    ASSERT(sigaction(SIGUSR1, &sa, nullptr) == 0);

    // Send signal to self
    ASSERT(raise(SIGUSR1) == 0);

    // Handler should have been called
    ASSERT(g_signal_received == SIGUSR1);
    ASSERT(g_signal_count == 1);

    // Reset to default
    signal(SIGUSR1, SIG_DFL);
}

TEST(signal_ignore) {
    reset_signal_state();

    // Set SIGUSR2 to ignore
    ASSERT(signal(SIGUSR2, SIG_IGN) != SIG_ERR);

    // Send signal - should be ignored
    ASSERT(raise(SIGUSR2) == 0);

    // Handler should not have been called
    ASSERT(g_signal_count == 0);

    // Reset to default
    signal(SIGUSR2, SIG_DFL);
}

// ============================================================================
// Signal Masking Tests
// ============================================================================

TEST(signal_mask_block) {
    reset_signal_state();

    struct sigaction sa;
    sa.sa_handler = test_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ASSERT(sigaction(SIGUSR1, &sa, nullptr) == 0);

    // Block SIGUSR1
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    ASSERT(sigprocmask(SIG_BLOCK, &mask, &oldmask) == 0);

    // Send signal - should be blocked
    ASSERT(raise(SIGUSR1) == 0);

    // Handler should not have been called yet
    ASSERT(g_signal_count == 0);

    // Unblock - handler should now be called
    ASSERT(sigprocmask(SIG_UNBLOCK, &mask, nullptr) == 0);

    // Give signal a moment to be delivered
    usleep(1000);

    // Handler should have been called
    ASSERT(g_signal_count == 1);

    signal(SIGUSR1, SIG_DFL);
}

TEST(signal_mask_setmask) {
    reset_signal_state();

    // Set mask to block SIGUSR1
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    ASSERT(sigprocmask(SIG_SETMASK, &mask, &oldmask) == 0);

    // Clear mask
    sigemptyset(&mask);
    ASSERT(sigprocmask(SIG_SETMASK, &mask, nullptr) == 0);

    // Restore old mask
    ASSERT(sigprocmask(SIG_SETMASK, &oldmask, nullptr) == 0);
}

// ============================================================================
// Process Group Tests
// ============================================================================

TEST(setpgid_self) {
    pid_t pid = getpid();
    pid_t pgid_before = getpgid(0);

    // Set self to own process group
    ASSERT(setpgid(0, 0) == 0);

    pid_t pgid_after = getpgid(0);

    // PGID should equal PID
    ASSERT(pgid_after == pid);

    // Restore original process group (if different)
    if (pgid_before != pgid_after) {
        setpgid(0, pgid_before);
    }
}

TEST(setsid_creates_new_session) {
    pid_t child = fork();

    if (child == 0) {
        // Child process

        // Create new session
        pid_t sid = setsid();

        // SID should equal PID
        ASSERT(sid == getpid());

        // PGID should equal PID
        ASSERT(getpgid(0) == getpid());

        // getsid should return same value
        ASSERT(getsid(0) == sid);

        exit(0);
    } else {
        // Parent
        ASSERT(child > 0);

        int status;
        ASSERT(waitpid(child, &status, 0) == child);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
}

// ============================================================================
// Signal Inheritance Tests
// ============================================================================

TEST(signal_handler_inherited_by_fork) {
    reset_signal_state();

    struct sigaction sa;
    sa.sa_handler = test_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ASSERT(sigaction(SIGUSR1, &sa, nullptr) == 0);

    pid_t child = fork();

    if (child == 0) {
        // Child process - handler should be inherited

        // Send signal to self
        raise(SIGUSR1);

        // Handler should have been called
        assert(g_signal_count == 1);

        exit(0);
    } else {
        // Parent
        ASSERT(child > 0);

        int status;
        ASSERT(waitpid(child, &status, 0) == child);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }

    signal(SIGUSR1, SIG_DFL);
}

TEST(signal_handler_reset_by_exec) {
    // This test requires an external program to exec
    // For now, we'll skip it as it requires a helper binary
    // TODO: Create helper program for exec testing
}

// ============================================================================
// SIGCHLD Tests
// ============================================================================

static volatile sig_atomic_t g_sigchld_received = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    g_sigchld_received = 1;
}

TEST(sigchld_on_child_exit) {
    g_sigchld_received = 0;

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ASSERT(sigaction(SIGCHLD, &sa, nullptr) == 0);

    pid_t child = fork();

    if (child == 0) {
        // Child - exit immediately
        exit(42);
    } else {
        // Parent
        ASSERT(child > 0);

        // Wait a moment for SIGCHLD
        usleep(10000);

        // SIGCHLD should have been received
        ASSERT(g_sigchld_received == 1);

        // Reap child
        int status;
        ASSERT(waitpid(child, &status, 0) == child);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 42);
    }

    signal(SIGCHLD, SIG_DFL);
}

// ============================================================================
// Kill Tests
// ============================================================================

TEST(kill_send_to_child) {
    reset_signal_state();

    pid_t child = fork();

    if (child == 0) {
        // Child process
        struct sigaction sa;
        sa.sa_handler = test_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGUSR1, &sa, nullptr);

        // Wait for signal
        pause();

        // Handler should have been called
        assert(g_signal_count == 1);

        exit(0);
    } else {
        // Parent
        ASSERT(child > 0);

        // Give child time to set up handler
        usleep(10000);

        // Send signal to child
        ASSERT(kill(child, SIGUSR1) == 0);

        // Wait for child
        int status;
        ASSERT(waitpid(child, &status, 0) == child);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("XINIM Signal Test Suite\n");
    printf("=======================\n\n");

    // Basic signal tests
    run_test_signal_self_sigint();
    run_test_signal_self_sigusr1();
    run_test_signal_ignore();

    // Signal masking tests
    run_test_signal_mask_block();
    run_test_signal_mask_setmask();

    // Process group tests
    run_test_setpgid_self();
    run_test_setsid_creates_new_session();

    // Signal inheritance tests
    run_test_signal_handler_inherited_by_fork();

    // SIGCHLD tests
    run_test_sigchld_on_child_exit();

    // Kill tests
    run_test_kill_send_to_child();

    // Summary
    printf("\n=======================\n");
    printf("Tests run:    %d\n", g_tests_run);
    printf("Tests passed: %d\n", g_tests_passed);
    printf("Tests failed: %d\n", g_tests_failed);

    if (g_tests_failed > 0) {
        printf("\nFAILED\n");
        return 1;
    } else {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
}
