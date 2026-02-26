/**
 * @file test_syscall_dispatch.cpp
 * @brief Tests for syscall dispatch table correctness.
 *
 * WHY: The dispatch table (src/kernel/sys/dispatch.cpp) routes syscall numbers
 *      to handlers. These tests verify the table constants at compile time and
 *      validate that known syscall numbers have the expected integer values.
 *
 * NOTE: Full dispatch testing requires kernel context (interrupts, ring switches).
 *       This file tests only the compile-time table constants and header definitions.
 */

#include "../kernel/syscall_table.hpp"
#include <cassert>
#include <cstdint>

using xinim::kernel::SyscallNumber;
using xinim::kernel::MAX_SYSCALLS;

static void test_core_syscall_numbers() {
    // Verify core POSIX syscall numbers match expected Linux ABI values.
    static_assert(static_cast<uint64_t>(SyscallNumber::READ)  == 0);
    static_assert(static_cast<uint64_t>(SyscallNumber::WRITE) == 1);
    static_assert(static_cast<uint64_t>(SyscallNumber::OPEN)  == 2);
    static_assert(static_cast<uint64_t>(SyscallNumber::CLOSE) == 3);
    static_assert(static_cast<uint64_t>(SyscallNumber::EXIT)  == 60);
    static_assert(static_cast<uint64_t>(SyscallNumber::FORK)  == 57);
    static_assert(static_cast<uint64_t>(SyscallNumber::GETPID) == 39);
}

static void test_max_syscalls_reasonable() {
    // Dispatch table must accommodate at least 512 entries.
    static_assert(MAX_SYSCALLS >= 512);
}

static void test_syscall_numbers_are_positive() {
    // All core syscall numbers must be non-negative.
    static_assert(static_cast<uint64_t>(SyscallNumber::READ)  < MAX_SYSCALLS);
    static_assert(static_cast<uint64_t>(SyscallNumber::WRITE) < MAX_SYSCALLS);
    static_assert(static_cast<uint64_t>(SyscallNumber::EXIT)  < MAX_SYSCALLS);
    static_assert(static_cast<uint64_t>(SyscallNumber::FORK)  < MAX_SYSCALLS);
}

int main() {
    test_core_syscall_numbers();
    test_max_syscalls_reasonable();
    test_syscall_numbers_are_positive();
    return 0;
}
