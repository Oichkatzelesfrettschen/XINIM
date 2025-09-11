/**
 * @file
 * @brief Safe wrapper around the C library's environment lookup.
 *
 * This translation unit defines a modern interface for querying process
 * environment variables.  Instead of exposing raw C strings, the function
 * returns a `std::optional<std::string>` which conveys the absence of a value
 * without resorting to null pointers.  The implementation delegates to
 * `std::getenv` provided by the standard library and is therefore entirely
 * portable.
 */

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace xinim {

/**
 * @brief Retrieve the value of an environment variable.
 *
 * This function wraps `std::getenv` and converts the result into an
 * `std::optional`.  If the environment variable identified by @p name exists,
 * its value is returned as a `std::string`; otherwise `std::nullopt` is
 * returned.  The wrapper avoids the pitfalls associated with direct use of
 * C-string pointers and is safe against accidental modification.
 *
 * @param name Name of the environment variable to query.
 * @return `std::optional<std::string>` containing the variable's value when
 *         present; `std::nullopt` otherwise.
 */
[[nodiscard]] std::optional<std::string> getenv(std::string_view name) {
    if (const char *value = std::getenv(name.data())) {
        return std::string{value};
    }
    return std::nullopt;
}

} // namespace xinim
