#include <algorithm>
#include <cstring>
#include <string_view>

/**
 * @file strncat.cpp
 * @brief Implementation of \c strncat using modern C++23 facilities.
 */

/**
 * @brief Append up to @p count characters from @p src to @p dest.
 *
 * The destination buffer must be large enough to hold the existing
 * characters, the appended substring, and a terminating null character.
 *
 * @param dest  Destination character buffer.
 * @param src   Source C string.
 * @param count Maximum number of characters to append.
 * @return Pointer to @p dest.
 */
[[nodiscard]] char *strncat(char *dest, const char *src, std::size_t count) {
    const std::size_t dest_len = std::strlen(dest);
    const std::string_view src_view{src};
    const std::size_t copy_len = std::min(count, src_view.size());
    std::copy_n(src_view.data(), copy_len, dest + dest_len);
    dest[dest_len + copy_len] = '\0';
    return dest;
}
