#pragma once
/**
 * @file syscall.hpp
 * @brief MM helper wrappers for SYS_* and FS notifications.
 */

#include "sys/com.hpp"
#include "sys/type.hpp"
#include "lib.hpp"
#include <cstddef>
#include <cstdint>

/**
 * @brief Send a request to the file system task.
 * @param call FS call number.
 * @param p1   First integer argument.
 * @param p2   Second integer argument.
 * @param p3   Third integer argument.
 * @return Result of sendrec().
 */
[[nodiscard]] int tell_fs(int call, int p1, int p2, int p3) noexcept;

/**
 * @brief Notify the kernel about a fork.
 */
[[nodiscard]] int sys_fork(int parent, int child, int child_pid, std::uint64_t token) noexcept;

/**
 * @brief Install a new memory map for a process.
 */
[[nodiscard]] int sys_newmap(int proc_nr, const mem_map *map) noexcept;

/**
 * @brief Finalize an exec after MM has loaded the image.
 */
[[nodiscard]] int sys_exec(int proc_nr, std::size_t new_sp, std::uint64_t token) noexcept;

/**
 * @brief Notify the kernel about process exit.
 */
[[nodiscard]] int sys_xit(int parent, int proc) noexcept;

/**
 * @brief Query the kernel for the current stack pointer.
 */
[[nodiscard]] int sys_getsp(int proc_nr, std::size_t *new_sp) noexcept;

/**
 * @brief Deliver a signal to a process via the kernel.
 */
[[nodiscard]] int sys_sig(int proc_nr, int sig_nr, int (*handler)(), std::uint64_t token) noexcept;

/**
 * @brief Ask the kernel to copy memory between processes.
 */
[[nodiscard]] int sys_copy(message *m_ptr) noexcept;

/**
 * @brief Abort the system via the kernel.
 */
[[nodiscard]] int sys_abort() noexcept;
