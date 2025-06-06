#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#if __cpp_lib_span
#include <span> // std::span overloads (C++20)
#endif
#include <system_error>
#include <vector>

namespace minix::io {

// Basic result type using std::optional with error codes
template <typename T> struct Result {
    std::optional<T> value{};
    std::error_code error{};

    explicit operator bool() const { return value.has_value(); }
    T &operator*() { return *value; }
    const T &operator*() const { return *value; }
};

class Stream {
  public:
    virtual ~Stream() = default;

    // Read bytes into ``buffer`` of size ``length``.
    virtual Result<size_t> read(std::byte *buffer, size_t length) = 0;
#if __cpp_lib_span
    // Convenience overload when std::span is available.
    virtual Result<size_t> read(std::span<std::byte> buffer) {
        return read(buffer.data(), buffer.size());
    }
#endif

    // Write bytes from ``buffer`` of size ``length``.
    virtual Result<size_t> write(const std::byte *buffer, size_t length) = 0;
#if __cpp_lib_span
    // Convenience overload when std::span is available.
    virtual Result<size_t> write(std::span<const std::byte> buffer) {
        return write(buffer.data(), buffer.size());
    }
#endif

    // Flush buffered data if applicable
    virtual std::error_code flush() { return {}; }

    // Close the stream
    virtual std::error_code close() { return {}; }

    // Obtain underlying descriptor
    virtual int descriptor() const = 0;
};

using StreamPtr = std::unique_ptr<Stream>;

} // namespace minix::io
