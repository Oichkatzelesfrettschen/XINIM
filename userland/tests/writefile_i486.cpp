#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr uint32_t kOpenWriteOnly = 0x0001U;
constexpr uint32_t kOpenCreate = 0x0040U;
constexpr uint32_t kOpenTruncate = 0x0200U;
constexpr uint32_t kErrorThreshold = 0xFFFFF000U;
constexpr char kUsage[] = "usage: writefile PATH TEXT\r\n";
constexpr char kOpenFail[] = "writefile: open failed\r\n";
constexpr char kWriteFail[] = "writefile: write failed\r\n";

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    while (text != nullptr && text[length] != '\0') {
        ++length;
    }
    return length;
}

[[nodiscard]] bool is_error(uint32_t value) noexcept {
    return value >= kErrorThreshold;
}

void write_string(int fd, const char* text) noexcept {
    static_cast<void>(xinim::userland::i386::write(fd, text, string_length(text)));
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)envp;

    if (argc < 3 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr) {
        write_string(2, kUsage);
        return 1;
    }

    const uint32_t fd = xinim::userland::i386::open(
        argv[1],
        kOpenWriteOnly | kOpenCreate | kOpenTruncate,
        0644U);
    if (is_error(fd)) {
        write_string(2, kOpenFail);
        return 1;
    }

    const uint32_t length = string_length(argv[2]);
    const uint32_t written = xinim::userland::i386::write(static_cast<int>(fd), argv[2], length);
    static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
    if (is_error(written) || written != length) {
        write_string(2, kWriteFail);
        return 1;
    }

    return 0;
}
