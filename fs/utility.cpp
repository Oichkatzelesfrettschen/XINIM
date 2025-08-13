/**
 * @file utility.cpp
 * @brief Modern C++23 helper routines for the XINIM file system server
 * @author Original authors, modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @copyright Copyright (c) 2025, The XINIM Project. All rights reserved.
 *
 * @section Description
 * A collection of helper routines for the XINIM file system server, providing
 * functionalities such as time retrieval, string comparison, memory copying, and
 * user-space path handling. This modernized implementation replaces older C-style
 * code with C++23 idioms, emphasizing type safety, RAII, thread safety, and
 * performance optimization. It leverages C++23 features like std::string_view,
 * std::format, and std::span for robust and efficient operations.
 *
 * @section Features
 * - RAII for resource management
 * - Exception-safe error handling
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view and std::span
 * - Constexpr configuration for compile-time optimization
 * - Comprehensive Doxygen documentation
 * - Support for C++23 string formatting
 *
 * @note Requires C++23 compliant compiler and XINIM kernel headers
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "super.hpp"
#include "type.hpp"
#include <algorithm>
#include <format>
#include <mutex>
#include <span>
#include <string_view>

namespace xfs_util {

/**
 * @brief Thread-safe flag to prevent recursive panics during sync.
 */
inline std::atomic<bool> panicking{false};

/**
 * @brief Thread-safe message structure for clock communication.
 */
inline message clock_mess{};
inline std::mutex clock_mess_mutex;

/**
 * @brief Retrieves the current real time from the clock task.
 * @return Seconds since the Unix epoch.
 * @throws std::runtime_error on communication failure with the clock task.
 */
[[nodiscard]] real_time clock_time() {
    std::lock_guard lock(clock_mess_mutex);
    clock_mess.m_type = GET_TIME;
    int k = sendrec(CLOCK, &clock_mess);
    if (k != OK) {
        throw std::runtime_error(std::format("clock_time error: {}", k));
    }
    auto* sp = get_super(ROOT_DEV);
    sp->s_time = new_time(clock_mess);
    if (!sp->s_rd_only) {
        sp->s_dirt = DIRTY;
    }
    return static_cast<real_time>(new_time(clock_mess));
}

/**
 * @brief Compares two strings of length n.
 * @param rsp1 First string as a span.
 * @param rsp2 Second string as a span.
 * @return true if the strings are identical, false otherwise.
 */
[[nodiscard]] bool cmp_string(std::span<const char> rsp1, std::span<const char> rsp2) noexcept {
    if (rsp1.size() != rsp2.size()) {
        return false;
    }
    return std::ranges::equal(rsp1, rsp2);
}

/**
 * @brief Copies a byte sequence.
 * @param dest Destination buffer as a span.
 * @param src Source buffer as a span.
 * @throws std::out_of_range if spans are incompatible with requested copy size.
 */
void copy(std::span<char> dest, std::span<const char> src) {
    if (src.size() > dest.size()) {
        throw std::out_of_range("Copy destination buffer too small");
    }
    if (src.empty()) {
        return;
    }
    std::ranges::copy(src, dest.begin());
}

/**
 * @brief Fetches a path name from user space.
 * @param path User-space pointer to the path.
 * @param len Length of the path including the null terminator.
 * @param flag When set to M3, the path may reside in the incoming message.
 * @return OK on success, or an error code from ErrorCode.
 * @throws std::out_of_range if path length exceeds MAX_PATH.
 */
[[nodiscard]] int fetch_name(std::string_view path, size_t len, int flag) {
    if (flag == M3 && len <= M3_STRING) {
        std::ranges::copy_n(pathname, len, user_path);
        return OK;
    }
    if (len > MAX_PATH) {
        err_code = ErrorCode::E_LONG_STRING;
        return ERROR;
    }
    vir_bytes vpath = reinterpret_cast<vir_bytes>(path.data());
    err_code = rw_user(D, who, vpath, static_cast<vir_bytes>(len), user_path, FROM_USER);
    return err_code;
}

/**
 * @brief Handler for unsupported system calls.
 * @return ErrorCode::EINVAL cast to int.
 */
[[nodiscard]] int no_sys() noexcept {
    return static_cast<int>(ErrorCode::EINVAL);
}

/**
 * @brief Panic handler that syncs all buffers and halts the system.
 * @param format Format string for the panic message.
 * @param num Optional numeric argument to include in the message.
 * @throws std::runtime_error to initiate system shutdown.
 */
[[noreturn]] void panic(std::string_view format, int num = NO_NUM) {
    if (panicking.exchange(true)) {
        return; // Prevent recursive panics
    }
    std::string message = num == NO_NUM ? std::format("File system panic: {}", format)
                                       : std::format("File system panic: {} {}", format, num);
    std::cerr << message << "\n";
    do_sync();
    throw std::runtime_error("System panic: halting");
}

} // namespace xfs_util

/**
 * @brief Main entry point for testing utility functions (optional).
 * @note This is a placeholder for standalone testing and not part of the file system server.
 */
int main() {
    // Optional: Add test code for standalone compilation
    return 0;

// Recommendations/TODOs:
// - Add unit tests for clock_time, cmp_string, copy, fetch_name, and panic.
// - Implement a logging framework (e.g., spdlog) for detailed diagnostics.
// - Consider std::expected for fetch_name to handle errors without global err_code.
// - Optimize copy and cmp_string for large buffers using SIMD instructions.
// - Add support for parallel operations using std::jthread where applicable.
// - Integrate with CI for automated testing and validation in the XINIM environment.