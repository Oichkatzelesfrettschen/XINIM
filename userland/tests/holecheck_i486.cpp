#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr uint32_t kOpenReadOnly = 0x0000U;
constexpr uint32_t kErrorThreshold = 0xFFFFF000U;
constexpr uint32_t kMaxFileBytes = 256U;
constexpr char kUsage[] = "usage: holecheck PATH ZERO_PREFIX TEXT\r\n";
constexpr char kOpenFail[] = "holecheck: open failed\r\n";
constexpr char kStatFail[] = "holecheck: stat failed\r\n";
constexpr char kReadFail[] = "holecheck: read failed\r\n";
constexpr char kSizeFail[] = "holecheck: size mismatch\r\n";
constexpr char kPrefixFail[] = "holecheck: non-zero prefix\r\n";
constexpr char kTextFail[] = "holecheck: text mismatch\r\n";
constexpr char kOk[] = "holecheck ok\r\n";

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

    uint32_t zero_prefix = 0U;
    if (!parse_u32(argv[2], zero_prefix)) {
        write_string(2, kUsage);
        return 1;
    }

    const uint32_t text_length = string_length(argv[3]);
    const uint32_t expected_size = zero_prefix + text_length;
    if (expected_size > kMaxFileBytes) {
        write_string(2, kSizeFail);
        return 1;
    }

    xinim::userland::UserspaceStat stat_buffer{};
    const uint32_t stat_result = xinim::userland::i386::stat(argv[1], &stat_buffer);
    if (is_error(stat_result)) {
        write_string(2, kStatFail);
        return 1;
    }
    if (stat_buffer.st_size != expected_size) {
        write_string(2, kSizeFail);
        return 1;
    }

    const uint32_t fd = xinim::userland::i386::open(argv[1], kOpenReadOnly, 0U);
    if (is_error(fd)) {
        write_string(2, kOpenFail);
        return 1;
    }

    uint8_t buffer[kMaxFileBytes];
    const uint32_t read_result = xinim::userland::i386::read(static_cast<int>(fd), buffer, expected_size);
    static_cast<void>(xinim::userland::i386::close(static_cast<int>(fd)));
    if (is_error(read_result) || read_result != expected_size) {
        write_string(2, kReadFail);
        return 1;
    }

    for (uint32_t index = 0U; index < zero_prefix; ++index) {
        if (buffer[index] != 0U) {
            write_string(2, kPrefixFail);
            return 1;
        }
    }
    for (uint32_t index = 0U; index < text_length; ++index) {
        if (buffer[zero_prefix + index] != static_cast<uint8_t>(argv[3][index])) {
            write_string(2, kTextFail);
            return 1;
        }
    }

    write_string(1, kOk);
    return 0;
}
