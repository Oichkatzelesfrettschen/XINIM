/**
 * @file uaccess.hpp
 * @brief User space memory access validation
 *
 * Provides safe functions for copying data between user space and kernel space.
 * All syscalls that accept user pointers MUST use these functions to prevent
 * security vulnerabilities.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_UACCESS_HPP
#define XINIM_KERNEL_UACCESS_HPP

#include <cstdint>
#include <cstddef>

namespace xinim::kernel {

// ============================================================================
// Memory Layout Constants (x86_64)
// ============================================================================

/**
 * @brief User space memory boundaries
 *
 * x86_64 canonical addresses:
 * - User space:   0x0000000000000000 - 0x00007FFFFFFFFFFF
 * - Hole (non-canonical): 0x0000800000000000 - 0xFFFF7FFFFFFFFFFF
 * - Kernel space: 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
 */
constexpr uintptr_t USER_SPACE_START = 0x0000000000001000UL;  ///< Start of user space (skip null page)
constexpr uintptr_t USER_SPACE_END   = 0x0000800000000000UL;  ///< End of user space (canonical limit)
constexpr uintptr_t KERNEL_SPACE_START = 0xFFFF800000000000UL; ///< Start of kernel space

/**
 * @brief Maximum path length (POSIX PATH_MAX)
 */
constexpr size_t PATH_MAX = 4096;

/**
 * @brief Maximum string length for copy_string_from_user
 */
constexpr size_t MAX_STRING_LEN = 4096;

// ============================================================================
// User Pointer Validation
// ============================================================================

/**
 * @brief Check if address range is valid user space memory
 *
 * Validates that:
 * 1. Address is not in null page (< 0x1000)
 * 2. Address is below kernel space (< 0x0000800000000000)
 * 3. Range doesn't overflow
 * 4. Range doesn't cross into kernel space
 *
 * @param addr User space address to check
 * @param size Size of memory region in bytes
 * @return true if valid user space address, false otherwise
 *
 * @note This does NOT check if pages are actually mapped in the page table.
 *       Week 9 Phase 1 limitation: Assumes all user addresses are mapped.
 *       Week 10: Will add page table validation.
 */
bool is_user_address(uintptr_t addr, size_t size);

/**
 * @brief Check if single address is in user space
 *
 * @param addr Address to check
 * @return true if in user space, false otherwise
 */
inline bool is_user_address(uintptr_t addr) {
    return is_user_address(addr, 1);
}

/**
 * @brief Check if pointer is valid user space pointer
 *
 * @tparam T Type of pointed-to object
 * @param ptr Pointer to check
 * @return true if valid user pointer, false otherwise
 */
template<typename T>
inline bool is_user_pointer(const T* ptr) {
    return is_user_address(reinterpret_cast<uintptr_t>(ptr), sizeof(T));
}

// ============================================================================
// Safe Copy Functions
// ============================================================================

/**
 * @brief Safely copy data from user space to kernel space
 *
 * Validates user address before copying. Returns error if:
 * - User address is invalid
 * - Range crosses into kernel space
 * - Page fault occurs during copy (Week 10)
 *
 * @param dest Kernel space destination buffer
 * @param src User space source address
 * @param size Number of bytes to copy
 * @return 0 on success, -EFAULT if user address is invalid
 *
 * @warning dest must be a valid kernel buffer of at least size bytes
 * @note Uses memcpy internally (Week 9). Week 10 will add page fault handling.
 */
int copy_from_user(void* dest, uintptr_t src, size_t size);

/**
 * @brief Safely copy data from kernel space to user space
 *
 * Validates user address before copying. Returns error if:
 * - User address is invalid
 * - Range crosses into kernel space
 * - Page fault occurs during copy (Week 10)
 *
 * @param dest User space destination address
 * @param src Kernel space source buffer
 * @param size Number of bytes to copy
 * @return 0 on success, -EFAULT if user address is invalid
 *
 * @warning src must be a valid kernel buffer of at least size bytes
 * @note Uses memcpy internally (Week 9). Week 10 will add page fault handling.
 */
int copy_to_user(uintptr_t dest, const void* src, size_t size);

/**
 * @brief Safely copy null-terminated string from user space
 *
 * Copies up to max_len bytes, ensuring null termination. Returns error if:
 * - User address is invalid
 * - String is longer than max_len
 * - No null terminator found within max_len bytes
 *
 * @param dest Kernel space destination buffer (must be at least max_len bytes)
 * @param src User space source address (null-terminated string)
 * @param max_len Maximum bytes to copy (including null terminator)
 * @return 0 on success, -EFAULT if invalid address, -ENAMETOOLONG if too long
 *
 * @note Always null-terminates dest, even on error
 */
int copy_string_from_user(char* dest, uintptr_t src, size_t max_len);

/**
 * @brief Get length of user space string (like strlen)
 *
 * Safely reads user space string to determine length. Returns error if:
 * - User address is invalid
 * - String exceeds max_len without null terminator
 *
 * @param src User space string address
 * @param max_len Maximum length to check
 * @return String length (excluding null), or negative error code
 */
ssize_t strnlen_user(uintptr_t src, size_t max_len);

// ============================================================================
// Typed Copy Functions (C++ convenience wrappers)
// ============================================================================

/**
 * @brief Copy single object from user space
 *
 * Type-safe wrapper for copy_from_user.
 *
 * @tparam T Type of object to copy
 * @param dest Kernel space destination
 * @param src User space source address
 * @return 0 on success, -EFAULT on error
 */
template<typename T>
inline int copy_from_user(T* dest, uintptr_t src) {
    return copy_from_user(dest, src, sizeof(T));
}

/**
 * @brief Copy single object to user space
 *
 * Type-safe wrapper for copy_to_user.
 *
 * @tparam T Type of object to copy
 * @param dest User space destination address
 * @param src Kernel space source
 * @return 0 on success, -EFAULT on error
 */
template<typename T>
inline int copy_to_user(uintptr_t dest, const T* src) {
    return copy_to_user(dest, src, sizeof(T));
}

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_UACCESS_HPP */
