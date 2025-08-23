#include <algorithm>
#include <cstring>
#include <string_view>

/**
 * @file strncat.cpp
 * @brief Modern C++23 implementation of the standard `strncat` function.
 *
 * @details This implementation provides a type-safe and efficient wrapper
 * for appending a substring to a C-style string, using modern C++ facilities
 * like `std::string_view` to safely handle the source string and `std::copy_n`
 * for the copy operation.
 *
 * @ingroup utility
 */

/**
 * @brief Append up to `count` characters from `src` to `dest`.
 *
 * The destination buffer must be large enough to hold the existing
 * characters, the appended substring, and a terminating null character.
 * This function handles the copy operation using modern C++ algorithms
 * for safety and clarity.
 *
 * @param dest  Destination character buffer.
 * @param src   Source C string.
 * @param count Maximum number of characters to append.
 * @return A pointer to `dest`.
 *
 * @sideeffects Modifies `dest` by appending data.
 * @thread_safety The caller must ensure that `dest` is not concurrently accessed.
 * @compat strncat(3)
 * @example
 * char buf[16] = "Hi";
 * strncat(buf, " there", 6); // buf becomes "Hi there"
 */
[[nodiscard]] char *strncat(char *dest, const char *src, std::size_t count) {
    const std::size_t dest_len = std::strlen(dest);
    const std::string_view src_view{src};
    const std::size_t copy_len = std::min(count, src_view.size());
    std::copy_n(src_view.data(), copy_len, dest + dest_len);
    dest[dest_len + copy_len] = '\0';
    return dest;
}