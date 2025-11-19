/**
 * @file exec_stack.cpp
 * @brief Initial user stack setup implementation
 *
 * Implements stack initialization for execve according to
 * System V AMD64 ABI specification.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "exec_stack.hpp"
#include "../early/serial_16550.hpp"
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

/**
 * @brief Count NULL-terminated string array
 */
int count_strings(char* const strings[]) {
    if (!strings) {
        return 0;
    }

    int count = 0;
    while (strings[count] != nullptr) {
        count++;
    }

    return count;
}

/**
 * @brief Calculate total size of string data
 */
size_t calculate_string_size(char* const strings[]) {
    if (!strings) {
        return 0;
    }

    size_t total = 0;
    for (int i = 0; strings[i] != nullptr; i++) {
        total += std::strlen(strings[i]) + 1;  // +1 for NULL terminator
    }

    return total;
}

/**
 * @brief Calculate total stack frame size
 */
size_t calculate_stack_size(int argc, int envc,
                            size_t argv_str_size,
                            size_t envp_str_size) {
    size_t total_size = 0;

    // argc (8 bytes)
    total_size += sizeof(uint64_t);

    // argv pointers (including NULL terminator)
    total_size += (argc + 1) * sizeof(char*);

    // envp pointers (including NULL terminator)
    total_size += (envc + 1) * sizeof(char*);

    // String data
    total_size += argv_str_size;
    total_size += envp_str_size;

    // Align to 16 bytes (required by x86_64 ABI)
    total_size = (total_size + 15) & ~15;

    return total_size;
}

/**
 * @brief Set up initial user stack
 *
 * Week 10 Phase 1: Simplified implementation
 * Creates stack frame in kernel buffer and returns pointer.
 * Week 11 will map this to actual user address space.
 */
uint64_t setup_exec_stack(uint64_t stack_top,
                          char* const argv[],
                          char* const envp[]) {
    char log_buf[256];

    // Count arguments and environment variables
    int argc = count_strings(argv);
    int envc = count_strings(envp);

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXEC_STACK] Setting up stack: argc=%d envc=%d\n",
                  argc, envc);
    early_serial.write(log_buf);

    // Calculate sizes
    size_t argv_str_size = calculate_string_size(argv);
    size_t envp_str_size = calculate_string_size(envp);
    size_t total_size = calculate_stack_size(argc, envc,
                                             argv_str_size, envp_str_size);

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXEC_STACK] Stack size: %zu bytes (aligned)\n", total_size);
    early_serial.write(log_buf);

    // Stack grows downward - allocate from top
    uint64_t stack_ptr = stack_top - total_size;

    // Ensure 16-byte alignment (x86_64 ABI requirement)
    stack_ptr &= ~15ULL;

    // Week 10 Phase 1: Allocate kernel buffer for stack
    // Week 11: This will be replaced with proper user page allocation
    char* stack_buffer = new char[total_size];
    if (!stack_buffer) {
        early_serial.write("[EXEC_STACK] Failed to allocate stack buffer\n");
        return 0;
    }

    std::memset(stack_buffer, 0, total_size);

    // Set up pointers into the buffer
    uint64_t current_pos = 0;

    // Calculate where string data starts
    uint64_t string_area_offset = sizeof(uint64_t) +
                                  (argc + 1) * sizeof(char*) +
                                  (envc + 1) * sizeof(char*);

    uint64_t string_pos = string_area_offset;

    // Write argc
    *(uint64_t*)(stack_buffer + current_pos) = (uint64_t)argc;
    current_pos += sizeof(uint64_t);

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXEC_STACK] argc written at offset 0: %d\n", argc);
    early_serial.write(log_buf);

    // Write argv pointers
    for (int i = 0; i < argc; i++) {
        // Pointer points to string in user address space
        uint64_t str_addr = stack_ptr + string_pos;
        *(char**)(stack_buffer + current_pos) = (char*)str_addr;
        current_pos += sizeof(char*);

        // Copy string data
        size_t str_len = std::strlen(argv[i]) + 1;
        std::memcpy(stack_buffer + string_pos, argv[i], str_len);

        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXEC_STACK] argv[%d] = \"%s\" at 0x%lx\n",
                      i, argv[i], str_addr);
        early_serial.write(log_buf);

        string_pos += str_len;
    }

    // NULL terminator for argv
    *(char**)(stack_buffer + current_pos) = nullptr;
    current_pos += sizeof(char*);

    // Write envp pointers
    for (int i = 0; i < envc; i++) {
        // Pointer points to string in user address space
        uint64_t str_addr = stack_ptr + string_pos;
        *(char**)(stack_buffer + current_pos) = (char*)str_addr;
        current_pos += sizeof(char*);

        // Copy string data
        size_t str_len = std::strlen(envp[i]) + 1;
        std::memcpy(stack_buffer + string_pos, envp[i], str_len);

        std::snprintf(log_buf, sizeof(log_buf),
                      "[EXEC_STACK] envp[%d] = \"%s\" at 0x%lx\n",
                      i, envp[i], str_addr);
        early_serial.write(log_buf);

        string_pos += str_len;
    }

    // NULL terminator for envp
    *(char**)(stack_buffer + current_pos) = nullptr;

    std::snprintf(log_buf, sizeof(log_buf),
                  "[EXEC_STACK] Stack setup complete: sp=0x%lx size=%zu\n",
                  stack_ptr, total_size);
    early_serial.write(log_buf);

    // TODO Week 10 Phase 1: Map stack_buffer to stack_ptr in user address space
    // TODO Week 11: Implement proper user stack page allocation

    // For now, keep the buffer allocated (will leak temporarily)
    // Week 11 will properly track and free this memory

    // Return stack pointer (points to argc)
    return stack_ptr;
}

} // namespace xinim::kernel
