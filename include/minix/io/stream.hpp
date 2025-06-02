#pragma once

#include <cstddef>      // For size_t, ptrdiff_t, byte
#include <cstdint>      // For uint8_t
#include <span>         // For std::span
#include <expected>     // For std::expected (or placeholder)
#include <system_error> // For std::error_code, std::error_category
#include <string>       // For std::string in IOErrorCategory::message
#include <memory>       // For std::unique_ptr
#include <algorithm>    // For std::min, std::copy_n etc. (might be used in derived classes)
#include <utility>      // For std::move, std::forward

// Placeholder for std::expected if not available (e.g. pre-C++23)
// This is a very basic placeholder. A real version is more complex.
#if __cplusplus < 202302L && (!defined(_LIBCPP_STD_VER) || _LIBCPP_STD_VER < 23) && (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 12) && !defined(__cpp_lib_expected)
// A more robust check might be needed depending on actual compiler/stdlib versions.
// __cpp_lib_expected is the feature test macro.
namespace std {
    template<class E_Placeholder> // Renamed to avoid conflict if std::E exists
    class unexpected {
    public:
        explicit unexpected(const E_Placeholder& e) : val_(e) {}
        explicit unexpected(E_Placeholder&& e) : val_(std::move(e)) {}
        const E_Placeholder& value() const& noexcept { return val_; }
        E_Placeholder& value() & noexcept { return val_; }
        E_Placeholder&& value() && noexcept { return std::move(val_); }
        const E_Placeholder&& value() const&& noexcept { return std::move(val_); }
    private:
        E_Placeholder val_;
    };

    template<class T_Placeholder, class E_Placeholder> // Renamed to avoid conflict
    class expected {
    public:
        using value_type = T_Placeholder;
        using error_type = E_Placeholder;
        using unexpected_type = unexpected<E_Placeholder>;

        constexpr expected() : has_val_(true), val_() {} // Default constructor for T
        constexpr expected(const T_Placeholder& val) : has_val_(true), val_(val) {}
        constexpr expected(T_Placeholder&& val) : has_val_(true), val_(std::move(val)) {}
        constexpr expected(const unexpected_type& unex) : has_val_(false), err_(unex.value()) {}
        constexpr expected(unexpected_type&& unex) : has_val_(false), err_(std::move(unex.value())) {}

        // Omitting other constructors like in_place, copy/move constructors for brevity in placeholder

        constexpr bool has_value() const noexcept { return has_val_; }
        constexpr explicit operator bool() const noexcept { return has_val_; }

        constexpr T_Placeholder& value() & { if (!has_val_) throw "bad_expected_access"; return val_; }
        constexpr const T_Placeholder& value() const& { if (!has_val_) throw "bad_expected_access"; return val_; }
        constexpr T_Placeholder&& value() && { if (!has_val_) throw "bad_expected_access"; return std::move(val_); }
        constexpr const T_Placeholder&& value() const&& { if (!has_val_) throw "bad_expected_access"; return std::move(val_); }

        constexpr E_Placeholder& error() & { if (has_val_) throw "bad_expected_access"; return err_; }
        constexpr const E_Placeholder& error() const& { if (has_val_) throw "bad_expected_access"; return err_; }
        constexpr E_Placeholder&& error() && { if (has_val_) throw "bad_expected_access"; return std::move(err_); }
        constexpr const E_Placeholder&& error() const&& { if (has_val_) throw "bad_expected_access"; return std::move(err_); }

        template<class U_Placeholder> // Renamed
        constexpr T_Placeholder value_or(U_Placeholder&& default_val) const& {
            return has_val_ ? val_ : static_cast<T_Placeholder>(std::forward<U_Placeholder>(default_val));
        }
        template<class U_Placeholder> // Renamed
        constexpr T_Placeholder value_or(U_Placeholder&& default_val) && {
            return has_val_ ? std::move(val_) : static_cast<T_Placeholder>(std::forward<U_Placeholder>(default_val));
        }

    private:
        bool has_val_;
        union {
            T_Placeholder val_;
            E_Placeholder err_;
        };
    };
} // namespace std
#endif // C++ version check and __cpp_lib_expected

namespace minix::io {

// Forward declarations
class Stream;
using StreamPtr = std::unique_ptr<Stream>;

// Error handling enum
enum class IOError : int {
    success = 0,
    would_block = 1,        // Operation would block in non-blocking mode
    invalid_argument = 2,   // Invalid argument provided to a function
    not_open = 3,           // Stream is not open
    already_open = 4,       // Stream is already open
    read_only = 5,          // Operation requires write access, stream is read-only
    write_only = 6,         // Operation requires read access, stream is write-only
    buffer_full = 7,        // Internal buffer is full (should typically be handled internally)
    end_of_file = 8,        // End of file reached
    io_error = 9,           // Generic I/O error from underlying system
    permission_denied = 10,
    resource_exhausted = 11,// e.g., no more file descriptors, out of memory
    not_supported = 12,     // Operation not supported by this stream type
    timed_out = 13,
    interrupted = 14,       // e.g. EINTR
    bad_file_descriptor = 15 // Invalid fd
};

// Custom error category for IOError
class IOErrorCategoryImpl : public std::error_category {
public:
    const char* name() const noexcept override { return "minix::io"; }
    std::string message(int ev) const override {
        switch (static_cast<IOError>(ev)) {
            case IOError::success: return "Success";
            case IOError::would_block: return "Operation would block";
            case IOError::invalid_argument: return "Invalid argument";
            case IOError::not_open: return "Stream not open";
            case IOError::already_open: return "Stream already open";
            case IOError::read_only: return "Stream is read-only";
            case IOError::write_only: return "Stream is write-only";
            case IOError::buffer_full: return "Buffer full";
            case IOError::end_of_file: return "End of file";
            case IOError::io_error: return "I/O error";
            case IOError::permission_denied: return "Permission denied";
            case IOError::resource_exhausted: return "Resource exhausted";
            case IOError::not_supported: return "Operation not supported";
            case IOError::timed_out: return "Operation timed out";
            case IOError::interrupted: return "Operation interrupted";
            case IOError::bad_file_descriptor: return "Bad file descriptor";
            default: return "Unknown I/O error";
        }
    }
};

// Singleton instance of the category
inline const std::error_category& IOErrorCategory() {
    static IOErrorCategoryImpl category_instance;
    return category_instance;
}

// Make std::error_code from IOError
inline std::error_code make_error_code(IOError e) {
    return {static_cast<int>(e), IOErrorCategory()};
}

// Result type alias for I/O operations
template<typename T>
using Result = std::expected<T, std::error_code>;

// Stream base class
class Stream {
public:
    // Stream state
    enum class State : std::uint8_t {
        closed,
        open, // General open state, actual mode (r/w) depends on derived class or flags
        error,
        eof
    };

    // Seek direction (matches std::ios_base::seekdir values for convenience)
    enum class SeekDir : std::uint8_t {
        beg = 0, // Corresponds to SEEK_SET
        cur = 1, // Corresponds to SEEK_CUR
        end = 2  // Corresponds to SEEK_END
    };

    // Buffer mode
    enum class BufferMode : std::uint8_t {
        none,
        line,
        full
    };

    // Constructors and destructor
    Stream() = default;
    virtual ~Stream() = default;

    // Move-only semantics
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&&) = default;
    Stream& operator=(Stream&&) = default;

    // Core I/O operations (synchronous for now)
    [[nodiscard]] virtual Result<size_t> read(std::span<std::byte> buffer) = 0;
    [[nodiscard]] virtual Result<size_t> write(std::span<const std::byte> data) = 0;
    [[nodiscard]] virtual Result<void> flush() = 0;
    [[nodiscard]] virtual Result<void> close() = 0; // Explicit close

    // Positioning
    [[nodiscard]] virtual Result<size_t> seek(std::ptrdiff_t offset, SeekDir dir) = 0;
    [[nodiscard]] virtual Result<size_t> tell() const = 0;

    // State queries
    [[nodiscard]] virtual State state() const noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept {
        return state() == State::open;
    }
    // Read/write capability might depend on how stream was opened
    [[nodiscard]] virtual bool is_readable() const noexcept = 0;
    [[nodiscard]] virtual bool is_writable() const noexcept = 0;

    // Buffering control
    [[nodiscard]] virtual Result<void> set_buffer_mode(BufferMode mode) = 0;
    [[nodiscard]] virtual BufferMode buffer_mode() const noexcept = 0;
};

} // namespace minix::io

// Enable std::error_code integration for minix::io::IOError
namespace std {
    template<>
    struct is_error_code_enum<minix::io::IOError> : std::true_type {};
}
