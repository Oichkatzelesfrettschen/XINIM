#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr uint32_t kOpenReadOnly = 0x0000U;
constexpr uint32_t kErrorThreshold = 0xFFFFF000U;
constexpr uint32_t kMaxFileBytes = 256U;
constexpr char kUsage[] = "usage: gapcheck PATH HEAD ZERO_GAP TAIL\r\n";
constexpr char kOpenFail[] = "gapcheck: open failed\r\n";
constexpr char kStatFail[] = "gapcheck: stat failed\r\n";
constexpr char kReadFail[] = "gapcheck: read failed\r\n";
constexpr char kSizeFail[] = "gapcheck: size mismatch\r\n";
constexpr char kHeadFail[] = "gapcheck: head mismatch\r\n";
constexpr char kGapFail[] = "gapcheck: non-zero gap\r\n";
constexpr char kTailFail[] = "gapcheck: tail mismatch\r\n";
constexpr char kOk[] = "gapcheck ok\r\n";

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
    static_cast<void>(xinim::userland::x86_32::write(fd, text, string_length(text)));
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

    if (argc < 5 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr ||
        argv[3] == nullptr || argv[4] == nullptr) {
        write_string(2, kUsage);
        return 1;
    }

    uint32_t zero_gap = 0U;
    if (!parse_u32(argv[3], zero_gap)) {
        write_string(2, kUsage);
        return 1;
    }

    const uint32_t head_length = string_length(argv[2]);
    const uint32_t tail_length = string_length(argv[4]);
    const uint32_t expected_size = head_length + zero_gap + tail_length;
    if (expected_size > kMaxFileBytes) {
        write_string(2, kSizeFail);
        return 1;
    }

    xinim::userland::UserspaceStat stat_buffer{};
    const uint32_t stat_result = xinim::userland::x86_32::stat(argv[1], &stat_buffer);
    if (is_error(stat_result)) {
        write_string(2, kStatFail);
        return 1;
    }
    if (stat_buffer.st_size != expected_size) {
        write_string(2, kSizeFail);
        return 1;
    }

    const uint32_t fd = xinim::userland::x86_32::open(argv[1], kOpenReadOnly, 0U);
    if (is_error(fd)) {
        write_string(2, kOpenFail);
        return 1;
    }

    uint8_t buffer[kMaxFileBytes];
    const uint32_t read_result = xinim::userland::x86_32::read(static_cast<int>(fd), buffer, expected_size);
    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(fd)));
    if (is_error(read_result) || read_result != expected_size) {
        write_string(2, kReadFail);
        return 1;
    }

    for (uint32_t index = 0U; index < head_length; ++index) {
        if (buffer[index] != static_cast<uint8_t>(argv[2][index])) {
            write_string(2, kHeadFail);
            return 1;
        }
    }
    for (uint32_t index = 0U; index < zero_gap; ++index) {
        if (buffer[head_length + index] != 0U) {
            write_string(2, kGapFail);
            return 1;
        }
    }
    for (uint32_t index = 0U; index < tail_length; ++index) {
        if (buffer[head_length + zero_gap + index] != static_cast<uint8_t>(argv[4][index])) {
            write_string(2, kTailFail);
            return 1;
        }
    }

    write_string(1, kOk);
    return 0;
}
