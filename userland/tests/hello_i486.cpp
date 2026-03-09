#include <stdint.h>

#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr char kHello[] =
    "Hello from XINIM i486 userspace\r\n";
constexpr char kPidPrefix[] = "pid: ";
constexpr char kArgcPrefix[] = "argc: ";
constexpr char kArgvPrefix[] = "argv[";
constexpr char kArgvMid[] = "]: ";
constexpr char kEnvPrefix[] = "env[0]: ";
constexpr char kNewline[] = "\r\n";

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void write_string(const char* text) noexcept {
    static_cast<void>(xinim::userland::i386::write(1, text, string_length(text)));
}

void write_dec(uint32_t value) noexcept {
    if (value == 0U) {
        write_string("0");
        return;
    }

    char buffer[10];
    uint32_t index = 0U;
    while (value != 0U) {
        buffer[index] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
        ++index;
    }

    while (index > 0U) {
        --index;
        static_cast<void>(xinim::userland::i386::write(1, &buffer[index], 1U));
    }
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    write_string(kHello);
    write_string(kPidPrefix);
    write_dec(xinim::userland::i386::getpid());
    write_string(kNewline);
    write_string(kArgcPrefix);
    write_dec(static_cast<uint32_t>(argc));
    write_string(kNewline);
    for (int index = 0; index < argc; ++index) {
        write_string(kArgvPrefix);
        write_dec(static_cast<uint32_t>(index));
        write_string(kArgvMid);
        write_string(argv[index] != nullptr ? argv[index] : "(null)");
        write_string(kNewline);
    }
    if (envp != nullptr && envp[0] != nullptr) {
        write_string(kEnvPrefix);
        write_string(envp[0]);
        write_string(kNewline);
    }
    return 0;
}
