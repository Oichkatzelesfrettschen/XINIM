#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr uint32_t kOpenWriteOnly = 0x0001U;
constexpr uint32_t kOpenCreate = 0x0040U;
constexpr uint32_t kOpenTruncate = 0x0200U;
constexpr uint32_t kSeekSet = 0U;
constexpr uint32_t kErrorThreshold = 0xFFFFF000U;
constexpr char kUsage[] = "usage: seekwrite PATH OFFSET TEXT\r\n";
constexpr char kOpenFail[] = "seekwrite: open failed\r\n";
constexpr char kSeekFail[] = "seekwrite: lseek failed\r\n";
constexpr char kWriteFail[] = "seekwrite: write failed\r\n";

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

[[nodiscard]] bool parse_u32(const char* text, uint32_t& value) noexcept {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }

    uint32_t parsed = 0U;
    for (uint32_t index = 0U; text[index] != '\0'; ++index) {
        const char ch = text[index];
        if (ch < '0' || ch > '9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<uint32_t>(ch - '0');
    }
    value = parsed;
    return true;
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)envp;

    if (argc < 4 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr || argv[3] == nullptr) {
        write_string(2, kUsage);
        return 1;
    }

    uint32_t offset = 0U;
    if (!parse_u32(argv[2], offset)) {
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

    const uint32_t seek_result = xinim::userland::i386::lseek(
        static_cast<int>(fd),
        static_cast<int32_t>(offset),
        static_cast<int>(kSeekSet));
    if (is_error(seek_result)) {
        static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
        write_string(2, kSeekFail);
        return 1;
    }

    const uint32_t length = string_length(argv[3]);
    const uint32_t written = xinim::userland::i386::write(static_cast<int>(fd), argv[3], length);
    static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
    if (is_error(written) || written != length) {
        write_string(2, kWriteFail);
        return 1;
    }

    return 0;
}
