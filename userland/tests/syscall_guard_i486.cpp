#include "xinim/sys/syscalls.h"
#include "xinim/userland/syscall_i386.hpp"

#include <stdint.h>

namespace {

    constexpr char kTerminalPath[] = "/dev/tty";
    constexpr char kSuccess[] = "syscall-guard: ok\r\n";
    constexpr char kFailure[] = "syscall-guard: fail\r\n";
    constexpr uint32_t kAtCurrentDirectory = static_cast<uint32_t>(-100);
    constexpr uint32_t kDetachTerminal = 0x5422U;
    constexpr uint32_t kNoControllingTerminal = static_cast<uint32_t>(-6);
    constexpr uint32_t kInvalidUserAddress = 1U;
    constexpr uint32_t kBadAddress = static_cast<uint32_t>(-14);

    [[nodiscard]] bool socket_pointers_rejected() noexcept {
        const uint32_t socket_fd =
            xinim::userland::x86_32::syscall3(static_cast<uint32_t>(SYS_socket), 2U, 2U, 0U);
        if (static_cast<int32_t>(socket_fd) < 0) {
            return false;
        }
        uint32_t option_length = sizeof(uint32_t);
        const uint32_t option_length_address =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&option_length));
        constexpr char payload[] = "x";
        const uint32_t payload_address =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(payload));
        return xinim::userland::x86_32::syscall5(static_cast<uint32_t>(SYS_setsockopt), socket_fd,
                                                 1U, 2U, kInvalidUserAddress,
                                                 sizeof(uint32_t)) == kBadAddress &&
               xinim::userland::x86_32::syscall5(static_cast<uint32_t>(SYS_getsockopt), socket_fd,
                                                 1U, 2U, kInvalidUserAddress,
                                                 option_length_address) == kBadAddress &&
               xinim::userland::x86_32::syscall2(static_cast<uint32_t>(SYS_getsockname), socket_fd,
                                                 kInvalidUserAddress) == kBadAddress &&
               xinim::userland::x86_32::syscall2(static_cast<uint32_t>(SYS_getpeername), socket_fd,
                                                 kInvalidUserAddress) == kBadAddress &&
               xinim::userland::x86_32::syscall4(static_cast<uint32_t>(SYS_socketpair), 1U, 1U, 0U,
                                                 kInvalidUserAddress) == kBadAddress &&
               xinim::userland::x86_32::syscall5(static_cast<uint32_t>(SYS_sendto), socket_fd,
                                                 payload_address, sizeof(payload) - 1U, 0U,
                                                 kInvalidUserAddress) == kBadAddress;
    }

    [[nodiscard]] uint32_t openat_terminal() noexcept {
        return xinim::userland::x86_32::syscall4(
            static_cast<uint32_t>(SYS_openat), kAtCurrentDirectory,
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(kTerminalPath)), 0U, 0U);
    }

    void report(const char *message, uint32_t length) noexcept {
        static_cast<void>(xinim::userland::x86_32::write(1, message, length));
    }

} // namespace

extern "C" int xinim_user_main(int argc, char **argv, char **envp) noexcept {
    (void) argc;
    (void) argv;
    (void) envp;

    const uint32_t terminal_fd = openat_terminal();
    if (static_cast<int32_t>(terminal_fd) < 0) {
        report(kFailure, sizeof(kFailure) - 1U);
        return 1;
    }
    const uint32_t detach_result = xinim::userland::x86_32::syscall3(
        static_cast<uint32_t>(SYS_ioctl), terminal_fd, kDetachTerminal, 0U);
    static_cast<void>(xinim::userland::x86_32::close(static_cast<int>(terminal_fd)));
    if (detach_result != 0U || openat_terminal() != kNoControllingTerminal ||
        xinim::userland::x86_32::open(kTerminalPath) != kNoControllingTerminal) {
        report(kFailure, sizeof(kFailure) - 1U);
        return 1;
    }
    if (!socket_pointers_rejected()) {
        report(kFailure, sizeof(kFailure) - 1U);
        return 1;
    }
    report(kSuccess, sizeof(kSuccess) - 1U);
    return 0;
}
