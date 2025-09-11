/**
 * @file randombytes.cpp
 * @brief Modern C++23 random bytes generation implementation
 *
 * Modernized random bytes generation using C++23 features with proper error handling.
 */

#include "randombytes.hpp"
#include <system_error>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif

namespace xinim::crypto::random {

// Error category for random operations
class random_error_category : public std::error_category {
public:
    const char* name() const noexcept override {
        return "random";
    }

    std::string message(int ev) const override {
        switch (static_cast<random_error>(ev)) {
            case random_error::system_call_failed:
                return "System call failed";
            case random_error::invalid_output_buffer:
                return "Invalid output buffer";
            case random_error::insufficient_entropy:
                return "Insufficient entropy";
            default:
                return "Unknown random error";
        }
    }
};

const std::error_category& random_category() noexcept {
    static const random_error_category category;
    return category;
}

std::error_code make_error_code(random_error e) noexcept {
    return {static_cast<int>(e), random_category()};
}

random_result randombytes(std::span<std::uint8_t> out) noexcept {
    if (out.empty()) {
        return std::unexpected(make_error_code(random_error::invalid_output_buffer));
    }

#ifdef _WIN32
    HCRYPTPROV ctx;
    if (!CryptAcquireContext(&ctx, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return std::unexpected(make_error_code(random_error::system_call_failed));
    }

    auto cleanup = [ctx]() noexcept {
        CryptReleaseContext(ctx, 0);
    };

    std::unique_ptr<void, decltype(cleanup)> ctx_guard{nullptr, cleanup};

    std::size_t remaining = out.size();
    std::uint8_t* current = out.data();

    while (remaining > 0) {
        std::size_t len = (remaining > 1048576) ? 1048576 : remaining;
        if (!CryptGenRandom(ctx, static_cast<DWORD>(len), current)) {
            return std::unexpected(make_error_code(random_error::system_call_failed));
        }
        current += len;
        remaining -= len;
    }

    return {};

#elif defined(__linux__) && defined(SYS_getrandom)
    std::size_t remaining = out.size();
    std::uint8_t* current = out.data();

    while (remaining > 0) {
        ssize_t ret = syscall(SYS_getrandom, current, remaining, 0);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(make_error_code(random_error::system_call_failed));
        }

        current += ret;
        remaining -= static_cast<std::size_t>(ret);
    }

    return {};

#else
    static int fd = -1;

    // Open /dev/urandom if not already open
    if (fd == -1) {
        do {
            fd = open("/dev/urandom", O_RDONLY);
        } while (fd == -1 && errno == EINTR);

        if (fd == -1) {
            return std::unexpected(make_error_code(random_error::system_call_failed));
        }
    }

    std::size_t remaining = out.size();
    std::uint8_t* current = out.data();

    while (remaining > 0) {
        ssize_t ret = read(fd, current, remaining);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(make_error_code(random_error::system_call_failed));
        }

        current += ret;
        remaining -= static_cast<std::size_t>(ret);
    }

    return {};
#endif
}

std::expected<std::vector<std::uint8_t>, error_code>
randombytes(std::size_t length) {
    if (length == 0) {
        return std::unexpected(make_error_code(random_error::invalid_output_buffer));
    }

    std::vector<std::uint8_t> result(length);
    if (auto err = randombytes(std::span<std::uint8_t>{result})) {
        return result;
    } else {
        return std::unexpected(err.error());
    }
}

} // namespace xinim::crypto::random