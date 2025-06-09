#pragma once

/**
 * @file stream.hpp
 * @brief Core abstract stream interface and helper utilities.
 */

#include <cstddef>
#include <memory>
#include <optional>
#if __cpp_lib_span
#include <span> // std::span overloads (C++20)
#endif
#include <system_error>
#include <vector>

namespace minix::io {

/**
 * @brief Result type bundling a value with an error code.
 *
 * Provides a lightweight container for returning either a successful result or
 * an error. A missing value indicates failure and the error member will
 * describe the cause.
 */
template <typename T> struct Result {
    std::optional<T> value{}; ///< Successfully returned value
    std::error_code error{};  ///< Populated when @c value is empty

    /// Check whether the result contains a value.
    explicit operator bool() const { return value.has_value(); }
    /// Dereference access to the contained value.
    T &operator*() { return *value; }
    /// Const dereference access to the contained value.
    const T &operator*() const { return *value; }
};

/**
 * @brief Abstract byte stream interface.
 *
 * Provides a minimal set of operations for reading and writing data. Concrete
 * implementations may represent files, memory buffers or other I/O sources.
 */
class Stream {
  public:
    virtual ~Stream() = default;

    /// Read bytes into @p buffer up to @p length.
    /// @return Number of bytes read or an error code.
    virtual Result<size_t> read(std::byte *buffer, size_t length) = 0;
#if __cpp_lib_span
    /// Convenience overload using std::span.
    virtual Result<size_t> read(std::span<std::byte> buffer) {
        return read(buffer.data(), buffer.size());
    }
#endif

    /// Write bytes from @p buffer up to @p length.
    /// @return Number of bytes written or an error code.
    virtual Result<size_t> write(const std::byte *buffer, size_t length) = 0;
#if __cpp_lib_span
    /// Convenience overload using std::span.
    virtual Result<size_t> write(std::span<const std::byte> buffer) {
        return write(buffer.data(), buffer.size());
    }
#endif

    /// Flush any buffered output data.
    virtual std::error_code flush() { return {}; }

    /// Close the stream, releasing resources.
    virtual std::error_code close() { return {}; }

    /// Retrieve the underlying descriptor if applicable.
    virtual int descriptor() const = 0;
};

/// Convenient alias for owning Stream pointers.
using StreamPtr = std::unique_ptr<Stream>;

} // namespace minix::io
