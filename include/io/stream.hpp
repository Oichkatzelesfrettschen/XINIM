#pragma once

#include <array>
#include <cstddef> // For size_t, byte
#include <cstdint> // For uint8_t
#include <span>
#include <optional>
#include <expected> // Assuming C++23 std::expected, or a placeholder/library version
#include <system_error> // For std::error_code
#include <string_view>  // For parameters
#include <ios>          // For std::ios_base::seekdir
#include <utility>      // For std::exchange, std::move

// Placeholder for std::expected if not available (e.g. pre-C++23)
// This is a very basic placeholder. A real version is more complex.
#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
namespace std {
    template<class E>
    class unexpected {
    public:
        explicit unexpected(const E& e) : val(e) {}
        explicit unexpected(E&& e) : val(std::move(e)) {}
        const E& error() const& noexcept { return val; }
        E& error() & noexcept { return val; }
        E&& error() && noexcept { return std::move(val); }
        const E&& error() const&& noexcept { return std::move(val); }
    private:
        E val;
    };

    struct unexpect_t { explicit unexpect_t() = default; };
    inline constexpr unexpect_t unexpect{};

    template<class T, class E>
    class expected {
    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        // Constructors
        constexpr expected() noexcept requires(std::is_default_constructible_v<T>) : has_val(true), val() {}
        constexpr expected(const expected& other) requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
            : has_val(other.has_val) {
            if (has_val) new(&val) T(other.val); else new(&err) E(other.err);
        }
        constexpr expected(expected&& other) noexcept requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
            : has_val(other.has_val) {
            if (has_val) new(&val) T(std::move(other.val)); else new(&err) E(std::move(other.err));
        }

        template<class U, class G>
        constexpr explicit(!std::is_convertible_v<const U&, T> || !std::is_convertible_v<const G&, E>)
        expected(const expected<U, G>& other) requires(std::is_constructible_v<T, const U&> && std::is_constructible_v<E, const G&>)
            : has_val(other.has_value()) {
            if (has_val) new(&val) T(other.value()); else new(&err) E(other.error());
        }

        template<class U, class G>
        constexpr explicit(!std::is_convertible_v<U&&, T> || !std::is_convertible_v<G&&, E>)
        expected(expected<U, G>&& other) requires(std::is_constructible_v<T, U&&> && std::is_constructible_v<E, G&&>)
            : has_val(other.has_value()) {
            if (has_val) new(&val) T(std::move(other.value())); else new(&err) E(std::move(other.error()));
        }


        template<class U = T>
        constexpr explicit(!std::is_convertible_v<U&&, T>)
        expected(U&& v) noexcept requires(!std::is_same_v<std::remove_cvref_t<U>, expected> && !std::is_same_v<std::remove_cvref_t<U>, unexpected<E>> && std::is_constructible_v<T, U&&>)
            : has_val(true) {
            new(&val) T(std::forward<U>(v));
        }

        constexpr expected(const unexpected_type& u) : has_val(false) { new(&err) E(u.error()); }
        constexpr expected(unexpected_type&& u) : has_val(false) { new(&err) E(std::move(u.error())); }


        template<class... Args>
        constexpr explicit expected(std::in_place_t, Args&&... args) requires(std::is_constructible_v<T, Args&&...>)
            : has_val(true) {
            new(&val) T(std::forward<Args>(args)...);
        }

        template<class U, class... Args>
        constexpr explicit expected(std::in_place_t, std::initializer_list<U> il, Args&&... args) requires(std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>)
            : has_val(true) {
            new(&val) T(il, std::forward<Args>(args)...);
        }

        template<class... Args>
        constexpr explicit expected(unexpect_t, Args&&... args) requires(std::is_constructible_v<E, Args&&...>)
            : has_val(false) {
            new(&err) E(std::forward<Args>(args)...);
        }

        template<class U, class... Args>
        constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args) requires(std::is_constructible_v<E, std::initializer_list<U>&, Args&&...>)
            : has_val(false) {
            new(&err) E(il, std::forward<Args>(args)...);
        }


        ~expected() { if (has_val) val.~T(); else err.~E(); }

        // Observers
        constexpr bool has_value() const noexcept { return has_val; }
        constexpr explicit operator bool() const noexcept { return has_val; }

        constexpr T* operator->() noexcept { return &val; }
        constexpr const T* operator->() const noexcept { return &val; }

        constexpr T& operator*() & noexcept { return val; }
        constexpr const T& operator*() const& noexcept { return val; }
        constexpr T&& operator*() && noexcept { return std::move(val); }
        constexpr const T&& operator*() const&& noexcept { return std::move(val); }

        constexpr T& value() & { if (!has_val) throw "bad_expected_access"; return val; }
        constexpr const T& value() const& { if (!has_val) throw "bad_expected_access"; return val; }
        constexpr T&& value() && { if (!has_val) throw "bad_expected_access"; return std::move(val); }
        constexpr const T&& value() const&& { if (!has_val) throw "bad_expected_access"; return std::move(val); }


        constexpr E& error() & noexcept { return err; }
        constexpr const E& error() const& noexcept { return err; }
        constexpr E&& error() && noexcept { return std::move(err); }
        constexpr const E&& error() const&& noexcept { return std::move(err); }

    private:
        bool has_val;
        union {
            T val;
            E err;
        };
    };
} // namespace std
#endif


namespace minix::io {

// Forward declaration
class Stream;

// Type alias for the underlying stream table
using StreamTable = std::array<Stream, 20>; // max_streams defined later, but size known

// Syscall result type alias
using SyscallResult = std::expected<size_t, std::error_code>;

// Stream Descriptor class
class StreamDescriptor {
    int fd_{-1};
public:
    constexpr explicit StreamDescriptor(int fd = -1) noexcept : fd_(fd) {}
    constexpr operator int() const noexcept { return fd_; }
    constexpr bool valid() const noexcept { return fd_ >= 0; }
    constexpr bool operator==(const StreamDescriptor& other) const = default;
};

// Stream State enum
enum class StreamState : std::uint8_t {
    closed,
    open_read,
    open_write,
    open_read_write,
    error
};

// Stream BufferMode enum
enum class StreamBufferMode : std::uint8_t {
    none,
    line,
    full
};

// Main Stream class
class Stream {
public:
    static constexpr size_t default_buffer_size = 1024;

private:
    StreamDescriptor descriptor_;
    StreamState state_{StreamState::closed};
    StreamBufferMode buffer_mode_{StreamBufferMode::full};
    std::array<std::byte, default_buffer_size> buffer_{};
    size_t read_pos_{0};
    size_t write_pos_{0};

public:
    // Constructors and Destructor
    Stream() = default; // Default constructor makes a closed stream
    explicit Stream(StreamDescriptor desc, StreamState state, StreamBufferMode mode = StreamBufferMode::full)
        : descriptor_(desc), state_(state), buffer_mode_(mode) {}

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&& other) noexcept;
    Stream& operator=(Stream&& other) noexcept;
    ~Stream();

    // Interface methods
    [[nodiscard]] constexpr bool is_open() const noexcept {
        return state_ != StreamState::closed && state_ != StreamState::error;
    }
    [[nodiscard]] constexpr bool needs_flush() const noexcept {
        return write_pos_ > 0 &&
               (state_ == StreamState::open_write || state_ == StreamState::open_read_write);
    }

    SyscallResult flush() noexcept;
    SyscallResult write(std::span<const std::byte> data) noexcept;
    SyscallResult read(std::span<std::byte> buffer) noexcept;

    std::expected<void, std::error_code> put_char(char c) noexcept;
    std::expected<char, std::error_code> get_char() noexcept;

    std::expected<size_t, std::error_code> seek(std::int64_t offset, std::ios_base::seekdir dir) noexcept;
    std::expected<size_t, std::error_code> tell() const noexcept;

    [[nodiscard]] constexpr StreamState state() const noexcept { return state_; }
    [[nodiscard]] constexpr StreamDescriptor descriptor() const noexcept { return descriptor_; }
    void set_buffer_mode(StreamBufferMode mode) noexcept { buffer_mode_ = mode; }
    [[nodiscard]] constexpr StreamBufferMode buffer_mode() const noexcept { return buffer_mode_; }

private:
    SyscallResult flush_internal() noexcept;
    SyscallResult fill_internal() noexcept;
};

// Global stream table and accessors
inline constexpr size_t max_streams = 20;
extern StreamTable& stream_table();

extern Stream& standard_input();
extern Stream& standard_output();
extern Stream& standard_error();

void initialize_io();

} // namespace minix::io
