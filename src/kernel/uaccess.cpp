/**
 * @file uaccess.cpp
 * @brief User space memory access validation implementation
 *
 * Implements safe copy functions for transferring data between user and kernel space.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 */

#include "uaccess.hpp"
#ifdef XINIM_ARCH_X86_64
#include "arch/x86_64/user_address_space.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#endif
#include <cerrno>
#include <cstring>

namespace xinim::kernel {

    // ============================================================================
    // User Pointer Validation
    // ============================================================================

    /**
     * @brief Check if address range is valid user space memory
     *
     * Performs comprehensive validation:
     * 1. Rejects null page (addr < 0x1000)
     * 2. Rejects kernel space (addr >= 0x0000800000000000)
     * 3. Checks for overflow (addr + size < addr)
     * 4. Ensures range doesn't cross into kernel space
     *
     * @param addr User space address to check
     * @param size Size of memory region in bytes
     * @return true if valid, false otherwise
     */
    bool is_user_address(uintptr_t addr, size_t size) {
        // Reject null page (security: prevent null pointer dereference exploits)
        if (addr < USER_SPACE_START) {
            return false;
        }

        // Reject kernel space addresses
        if (addr >= USER_SPACE_END) {
            return false;
        }

        // Check for overflow (addr + size wraps around)
        uintptr_t end = addr + size;
        if (end < addr) {
            return false; // Overflow detected
        }

        // Ensure end doesn't cross into kernel space
        if (end > USER_SPACE_END) {
            return false;
        }

#ifdef XINIM_ARCH_X86_64
        const ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->address_space_root == 0U) {
            return false;
        }
        const xinim::kernel::x86_64::UserAddressSpace address_space{process->address_space_root};
        return xinim::kernel::x86_64::is_user_range_mapped(address_space, addr, size, false);
#else
        return true;
#endif
    }

    // ============================================================================
    // Safe Copy Functions
    // ============================================================================

    /**
     * @brief Safely copy data from user space to kernel space
     *
     * Validates the canonical user range and current page-table mappings before
     * copying from the process address space.
     *
     * @param dest Kernel space destination buffer
     * @param src User space source address
     * @param size Number of bytes to copy
     * @return 0 on success, -EFAULT if user address is invalid
     */
    int copy_from_user(void *dest, uintptr_t src, size_t size) {
        // Validate user address
        if (!is_user_address(src, size)) {
            return -EFAULT;
        }

        // Mapping validation above requires every source page to be present.
        std::memcpy(dest, reinterpret_cast<const void *>(src), size);

        return 0;
    }

    /**
     * @brief Safely copy data from kernel space to user space
     *
     * Validates the canonical user range and writable page-table mappings before
     * copying into the process address space.
     *
     * @param dest User space destination address
     * @param src Kernel space source buffer
     * @param size Number of bytes to copy
     * @return 0 on success, -EFAULT if user address is invalid
     */
    int copy_to_user(uintptr_t dest, const void *src, size_t size) {
        // Validate user address
        if (!is_user_address(dest, size)) {
            return -EFAULT;
        }

#ifdef XINIM_ARCH_X86_64
        const ProcessControlBlock *process = get_current_process();
        const xinim::kernel::x86_64::UserAddressSpace address_space{
            process != nullptr ? process->address_space_root : 0U};
        if (!xinim::kernel::x86_64::is_user_range_mapped(address_space, dest, size, true)) {
            return -EFAULT;
        }
#endif

        // Mapping validation above requires every destination page to be writable.
        std::memcpy(reinterpret_cast<void *>(dest), src, size);

        return 0;
    }

    /**
     * @brief Safely copy null-terminated string from user space
     *
     * Reads user string byte-by-byte until null terminator or max_len reached.
     * Always null-terminates destination buffer.
     *
     * @param dest Kernel space destination buffer
     * @param src User space source address (null-terminated string)
     * @param max_len Maximum bytes to copy (including null terminator)
     * @return 0 on success, -EFAULT if invalid, -ENAMETOOLONG if too long
     */
    int copy_string_from_user(char *dest, uintptr_t src, size_t max_len) {
        // Edge case: max_len must be at least 1 (for null terminator)
        if (max_len == 0) {
            return -EINVAL;
        }

        // Validate initial address (we'll check incrementally)
        if (!is_user_address(src)) {
            // Ensure dest is null-terminated even on error
            dest[0] = '\0';
            return -EFAULT;
        }

        // Copy byte-by-byte, checking for null terminator
        const char *src_ptr = reinterpret_cast<const char *>(src);
        size_t copied = 0;

        for (copied = 0; copied < max_len; copied++) {
            // Validate current address
            if (!is_user_address(src + copied)) {
                dest[0] = '\0';
                return -EFAULT;
            }

            // Copy character
            char c = src_ptr[copied];
            dest[copied] = c;

            // Check for null terminator
            if (c == '\0') {
                return 0; // Success: null-terminated string copied
            }
        }

        // No null terminator found within max_len
        // Null-terminate what we have
        dest[max_len - 1] = '\0';
        return -ENAMETOOLONG;
    }

    /**
     * @brief Get length of user space string (like strnlen)
     *
     * Reads user string to determine length without copying.
     *
     * @param src User space string address
     * @param max_len Maximum length to check
     * @return String length (excluding null), or negative error code
     */
    ssize_t strnlen_user(uintptr_t src, size_t max_len) {
        // Validate initial address
        if (!is_user_address(src)) {
            return -EFAULT;
        }

        const char *src_ptr = reinterpret_cast<const char *>(src);
        size_t len = 0;

        for (len = 0; len < max_len; len++) {
            // Validate current address
            if (!is_user_address(src + len)) {
                return -EFAULT;
            }

            // Check for null terminator
            if (src_ptr[len] == '\0') {
                return (ssize_t) len;
            }
        }

        // No null terminator found within max_len
        // Return max_len to indicate string is at least this long
        return (ssize_t) max_len;
    }

} // namespace xinim::kernel
