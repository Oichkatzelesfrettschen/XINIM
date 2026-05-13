#include <stdint.h>

#include "xinim/sys/syscalls.h"
#include "xinim/userland/syscall_i386.hpp"

namespace {

constexpr char kOk[] = "heapprobe: ok\r\n";
constexpr char kFail[] = "heapprobe: fail\r\n";
constexpr uint32_t kProbeBytes = 128U;

[[nodiscard]] uint32_t string_length(const char* text) noexcept {
    uint32_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void write_string(const char* text) noexcept {
    static_cast<void>(xinim::userland::x86_32::write(1, text, string_length(text)));
}

[[nodiscard]] uint32_t sys_brk(uint32_t value) noexcept {
    return xinim::userland::x86_32::syscall1(static_cast<uint32_t>(SYS_brk), value);
}

} // namespace

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)argc;
    (void)argv;
    (void)envp;

    const uint32_t initial_break = sys_brk(0U);
    if (initial_break == 0U) {
        write_string(kFail);
        return 1;
    }

    const uint32_t first_break = initial_break + kProbeBytes;
    if (sys_brk(first_break) != first_break) {
        write_string(kFail);
        return 1;
    }

    auto* probe = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(initial_break));
    for (uint32_t index = 0U; index < kProbeBytes; ++index) {
        probe[index] = static_cast<uint8_t>(index + 1U);
    }

    const uint32_t second_break = first_break + kProbeBytes;
    if (sys_brk(second_break) != second_break) {
        write_string(kFail);
        return 1;
    }

    auto* extension = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(first_break));
    for (uint32_t index = 0U; index < kProbeBytes; ++index) {
        if (extension[index] != 0U) {
            write_string(kFail);
            return 1;
        }
    }

    if (sys_brk(initial_break) != initial_break) {
        write_string(kFail);
        return 1;
    }

    write_string(kOk);
    return 0;
}
