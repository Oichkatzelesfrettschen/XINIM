#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr uint32_t kErrorThreshold = 0xFFFFF000U;
constexpr char kUsage[] = "usage: unlink PATH\r\n";
constexpr char kFail[] = "unlink: remove failed\r\n";

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

    if (argc < 2 || argv == nullptr || argv[1] == nullptr) {
        write_string(2, kUsage);
        return 1;
    }

    const uint32_t result = xinim::userland::i386::unlink(argv[1]);
    if (is_error(result)) {
        write_string(2, kFail);
        return 1;
    }
    return 0;
}
