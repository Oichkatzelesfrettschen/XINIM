#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
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

    // Read bytes into buffer, returning number read or error
    virtual Result<size_t> read(std::span<std::byte> buffer) = 0;

    // Write bytes from buffer, returning number written or error
    virtual Result<size_t> write(std::span<const std::byte> buffer) = 0;

    // Flush buffered data if applicable
    virtual std::error_code flush() { return {}; }

    // Close the stream
    virtual std::error_code close() { return {}; }

    // Obtain underlying descriptor
    virtual int descriptor() const = 0;
};

using StreamPtr = std::unique_ptr<Stream>;

} // namespace minix::io
