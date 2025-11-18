/**
 * @file exec_stack.hpp
 * @brief Initial user stack setup for execve
 *
 * Sets up the initial user stack with argc, argv, and envp
 * according to the System V AMD64 ABI specification.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#pragma once
#include <cstdint>
#include <cstddef>

namespace xinim::kernel {

/**
 * @brief Set up initial user stack for new program
 *
 * Creates stack layout according to System V AMD64 ABI:
 *
 * High Address (stack grows downward)
 * ┌──────────────────────┐
 * │ NULL                 │ ← envp terminator
 * │ env strings          │
 * │ NULL                 │ ← argv terminator
 * │ arg strings          │
 * │ envp pointers        │
 * │ argv pointers        │
 * │ argc                 │
 * └──────────────────────┘ ← RSP points here (16-byte aligned)
 * Low Address
 *
 * @param stack_top Top of user stack (high address)
 * @param argv Argument vector (NULL-terminated array of strings)
 * @param envp Environment vector (NULL-terminated array of strings)
 * @return New stack pointer (RSP value), 16-byte aligned
 *
 * @note Week 10 Phase 1: Simplified implementation using kernel buffer
 * @note Week 11: Enhanced with proper user memory mapping
 */
uint64_t setup_exec_stack(uint64_t stack_top,
                          char* const argv[],
                          char* const envp[]);

/**
 * @brief Count NULL-terminated string array
 *
 * @param strings NULL-terminated array of string pointers
 * @return Number of strings (excluding NULL terminator)
 */
int count_strings(char* const strings[]);

/**
 * @brief Calculate total size of string data
 *
 * Sums the lengths of all strings (including NULL terminators).
 *
 * @param strings NULL-terminated array of string pointers
 * @return Total size in bytes
 */
size_t calculate_string_size(char* const strings[]);

/**
 * @brief Calculate total stack frame size
 *
 * Calculates the total size needed for argc, argv pointers,
 * envp pointers, and all string data.
 *
 * @param argc Number of arguments
 * @param envc Number of environment variables
 * @param argv_str_size Total size of argv strings
 * @param envp_str_size Total size of envp strings
 * @return Total stack frame size (16-byte aligned)
 */
size_t calculate_stack_size(int argc, int envc,
                            size_t argv_str_size,
                            size_t envp_str_size);

} // namespace xinim::kernel
