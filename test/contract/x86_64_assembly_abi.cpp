#include "arch/x86_64/context_restore.hpp"
#include "context.hpp"
#include "interrupts.hpp"
#include "timer.hpp"

#include <cstdint>
#include <type_traits>
#include <xinim/abi/native_syscall_numbers.h>
#include <xinim/abi/syscall_dispatch.h>
#include <xinim/abi/x86_64_context_layout.h>
#include <xinim/sys/syscalls.h>

static_assert(sizeof(xinim::kernel::CpuContext) == XINIM_X86_64_CONTEXT_SIZE);
static_assert(sizeof(xinim::kernel::X86_64InterruptFrame) == XINIM_X86_64_INTERRUPT_USER_SIZE);
static_assert(SYS_exit == XINIM_NATIVE_SYS_EXIT);
static_assert(SYS_dup == XINIM_NATIVE_SYS_DUP);
static_assert(SYS_execve == XINIM_NATIVE_SYS_EXECVE);
static_assert(SYS_chown == XINIM_NATIVE_SYS_CHOWN);
static_assert(SYS_fchown == XINIM_NATIVE_SYS_FCHOWN);
static_assert(SYS_truncate == XINIM_NATIVE_SYS_TRUNCATE);
static_assert(SYS_ftruncate == XINIM_NATIVE_SYS_FTRUNCATE);
static_assert(SYS_flock == XINIM_NATIVE_SYS_FLOCK);
static_assert(SYS_symlink == XINIM_NATIVE_SYS_SYMLINK);
static_assert(SYS_readlink == XINIM_NATIVE_SYS_READLINK);
static_assert(SYS_lstat == XINIM_NATIVE_SYS_LSTAT);

using XinimContextRestore = void (*)(xinim::kernel::CpuContext *);
static_assert(std::is_same_v<decltype(&load_context), XinimContextRestore>);
static_assert(std::is_same_v<decltype(&load_context_ring3), XinimContextRestore>);

using XinimSyscallDispatch = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                                          uint64_t, uint64_t);
static_assert(std::is_same_v<decltype(&xinim_syscall_dispatch), XinimSyscallDispatch>);

using XinimInterruptCallback = void (*)(const xinim::kernel::X86_64InterruptFrame *) noexcept;
using XinimTimerCallback = void (*)(const xinim::kernel::X86_64InterruptFrame *,
                                    const void *) noexcept;
static_assert(std::is_same_v<decltype(&timer_interrupt_c_handler), XinimTimerCallback>);
static_assert(std::is_same_v<decltype(&keyboard_interrupt_c_handler), XinimInterruptCallback>);
static_assert(std::is_same_v<decltype(&com1_interrupt_c_handler), XinimInterruptCallback>);
static_assert(std::is_same_v<decltype(&com2_interrupt_c_handler), XinimInterruptCallback>);

consteval bool has_valid_initial_fxsave_image() {
    xinim::kernel::CpuContext context{};
    context.initialize(0, 0);
    return context.fxsave_area[XINIM_X86_64_FXSAVE_FCW_OFFSET] ==
               static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_FCW) &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_FCW_OFFSET + 1] ==
               static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_FCW >>
                                    XINIM_X86_64_FXSAVE_BITS_PER_BYTE) &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_FTW_OFFSET] == 0 &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET] ==
               static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_MXCSR) &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET + 1] ==
               static_cast<uint8_t>(XINIM_X86_64_FXSAVE_RESET_MXCSR >>
                                    XINIM_X86_64_FXSAVE_BITS_PER_BYTE) &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET + 2] == 0 &&
           context.fxsave_area[XINIM_X86_64_FXSAVE_MXCSR_OFFSET + 3] == 0;
}

static_assert(has_valid_initial_fxsave_image());
