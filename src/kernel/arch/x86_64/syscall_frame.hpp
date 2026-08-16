#pragma once

#include <cstddef>
#include <cstdint>
#include <xinim/abi/x86_64_syscall_frame.h>

namespace xinim::kernel::x86_64 {

    struct SyscallFrame {
        uint64_t rax;
        uint64_t r9;
        uint64_t r8;
        uint64_t r10;
        uint64_t rdx;
        uint64_t rsi;
        uint64_t rdi;
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t rbp;
        uint64_t rbx;
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
        uint64_t rsp;
        uint64_t ss;
    };

    static_assert(offsetof(SyscallFrame, rax) == XINIM_X86_64_SYSCALL_RAX);
    static_assert(offsetof(SyscallFrame, r9) == XINIM_X86_64_SYSCALL_R9);
    static_assert(offsetof(SyscallFrame, rip) == XINIM_X86_64_SYSCALL_RIP);
    static_assert(offsetof(SyscallFrame, rsp) == XINIM_X86_64_SYSCALL_RSP);
    static_assert(sizeof(SyscallFrame) == XINIM_X86_64_SYSCALL_FRAME_SIZE);

    [[nodiscard]] const SyscallFrame *active_syscall_frame() noexcept;

} // namespace xinim::kernel::x86_64
