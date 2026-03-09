#include "xinim/abi/lattice_changer.hpp"
#include "xinim/sys/syscalls.h"

#include <array>
#include <cstddef>

namespace xinim::abi::lattice {
namespace {

struct Mapping {
    SourceAbi source;
    uint8_t word_bits;
    uint8_t arg_shape;
    uint32_t foreign_no;
    uint32_t native_no;
    uint32_t linux64_no;
};

constexpr TransformResult kUnsupported{
    .syscall_no = 0u,
    .supported = false,
    .estimated_cycles = 0u,
};

constexpr std::array<Mapping, 44> kMappings{{
    // Native ABI passthrough (core subset only)
    {SourceAbi::kNative, 64, 0, SYS_read, SYS_read, 0},
    {SourceAbi::kNative, 64, 0, SYS_write, SYS_write, 0},
    {SourceAbi::kNative, 64, 0, SYS_open, SYS_open, 0},
    {SourceAbi::kNative, 64, 0, SYS_close, SYS_close, 0},
    {SourceAbi::kNative, 64, 0, SYS_lseek, SYS_lseek, 0},
    {SourceAbi::kNative, 64, 0, SYS_getpid, SYS_getpid, 0},
    {SourceAbi::kNative, 64, 0, SYS_getppid, SYS_getppid, 0},
    {SourceAbi::kNative, 64, 0, SYS_fork, SYS_fork, 0},
    {SourceAbi::kNative, 64, 0, SYS_execve, SYS_execve, 0},
    {SourceAbi::kNative, 64, 0, SYS_exit, SYS_exit, 0},
    {SourceAbi::kNative, 64, 0, SYS_wait4, SYS_wait4, 0},
    {SourceAbi::kNative, 32, 0, SYS_read, SYS_read, 0},
    {SourceAbi::kNative, 32, 0, SYS_write, SYS_write, 0},
    {SourceAbi::kNative, 32, 0, SYS_open, SYS_open, 0},
    {SourceAbi::kNative, 32, 0, SYS_close, SYS_close, 0},
    {SourceAbi::kNative, 32, 0, SYS_lseek, SYS_lseek, 0},
    {SourceAbi::kNative, 32, 0, SYS_getpid, SYS_getpid, 0},
    {SourceAbi::kNative, 32, 0, SYS_getppid, SYS_getppid, 0},
    {SourceAbi::kNative, 32, 0, SYS_fork, SYS_fork, 0},
    {SourceAbi::kNative, 32, 0, SYS_execve, SYS_execve, 0},
    {SourceAbi::kNative, 32, 0, SYS_exit, SYS_exit, 0},
    {SourceAbi::kNative, 32, 0, SYS_wait4, SYS_wait4, 0},

    // Linux 64 -> native and legacy table
    {SourceAbi::kLinux, 64, 0, 0, SYS_read, 0},
    {SourceAbi::kLinux, 64, 0, 1, SYS_write, 1},
    {SourceAbi::kLinux, 64, 0, 2, SYS_open, 2},
    {SourceAbi::kLinux, 64, 0, 3, SYS_close, 3},
    {SourceAbi::kLinux, 64, 0, 8, SYS_lseek, 8},
    {SourceAbi::kLinux, 64, 0, 39, SYS_getpid, 39},
    {SourceAbi::kLinux, 64, 0, 110, SYS_getppid, 110},
    {SourceAbi::kLinux, 64, 0, 57, SYS_fork, 57},
    {SourceAbi::kLinux, 64, 0, 59, SYS_execve, 59},
    {SourceAbi::kLinux, 64, 0, 60, SYS_exit, 60},
    {SourceAbi::kLinux, 64, 0, 61, SYS_wait4, 61},

    // Linux 32 -> native and legacy table (to x86_64-numbered table entries)
    {SourceAbi::kLinux, 32, 0, 3, SYS_read, 0},
    {SourceAbi::kLinux, 32, 0, 4, SYS_write, 1},
    {SourceAbi::kLinux, 32, 0, 5, SYS_open, 2},
    {SourceAbi::kLinux, 32, 0, 6, SYS_close, 3},
    {SourceAbi::kLinux, 32, 0, 19, SYS_lseek, 8},
    {SourceAbi::kLinux, 32, 0, 20, SYS_getpid, 39},
    {SourceAbi::kLinux, 32, 0, 64, SYS_getppid, 110},
    {SourceAbi::kLinux, 32, 0, 2, SYS_fork, 57},
    {SourceAbi::kLinux, 32, 0, 11, SYS_execve, 59},
    {SourceAbi::kLinux, 32, 0, 1, SYS_exit, 60},
    {SourceAbi::kLinux, 32, 0, 114, SYS_wait4, 61},
}};

[[nodiscard]] TransformResult exact_lookup(
    SourceAbi source,
    uint32_t word_bits,
    uint8_t arg_shape,
    uint32_t foreign_syscall_no,
    TransformTarget target) {
    for (const auto& item : kMappings) {
        if (item.source != source || item.word_bits != word_bits) {
            continue;
        }
        if (item.arg_shape != 0 && item.arg_shape != arg_shape) {
            continue;
        }
        if (item.foreign_no != foreign_syscall_no) {
            continue;
        }
        if (target == TransformTarget::kNativeSyscalls) {
            return TransformResult{item.native_no, true, 9};
        }
        if (item.linux64_no != 0 || item.foreign_no == 0) {
            return TransformResult{item.linux64_no, true, 9};
        }
    }
    return kUnsupported;
}

[[nodiscard]] TransformResult prefix_fallback(
    SourceAbi source,
    uint32_t word_bits,
    uint8_t arg_shape,
    uint32_t foreign_syscall_no,
    TransformTarget target) {
    // Prefix fallback is only allowed for explicitly-shaped tagged calls.
    // This prevents accidental remaps for ordinary syscall numbers.
    if (arg_shape == 0) {
        return kUnsupported;
    }

    uint32_t best_prefix = 0;
    const Mapping* best = nullptr;
    for (const auto& item : kMappings) {
        if (item.source != source || item.word_bits != word_bits) {
            continue;
        }
        if (item.arg_shape != 0 && item.arg_shape != arg_shape) {
            continue;
        }
        const uint32_t prefix = shared_prefix_bits(item.foreign_no, foreign_syscall_no);
        if (prefix > best_prefix) {
            best_prefix = prefix;
            best = &item;
        }
    }

    // Conservative safety gate to prevent accidental remaps.
    if (best == nullptr || best_prefix < 30) {
        return kUnsupported;
    }

    if (target == TransformTarget::kNativeSyscalls) {
        return TransformResult{best->native_no, true, 28};
    }
    if (best->linux64_no != 0 || best->foreign_no == 0) {
        return TransformResult{best->linux64_no, true, 28};
    }
    return kUnsupported;
}

} // namespace

uint32_t shared_prefix_bits(uint32_t a, uint32_t b) {
    const uint32_t x = a ^ b;
    if (x == 0) {
        return 32;
    }
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<uint32_t>(__builtin_clz(x));
#else
    uint32_t bits = 0;
    uint32_t mask = 0x80000000U;
    while ((x & mask) == 0 && mask != 0) {
        ++bits;
        mask >>= 1;
    }
    return bits;
#endif
}

TransformResult transform_syscall(
    SourceAbi source,
    uint32_t word_bits,
    uint8_t arg_shape,
    uint32_t foreign_syscall_no,
    TransformTarget target) {
    if (word_bits != 32U && word_bits != 64U) {
        return kUnsupported;
    }

    if (source == SourceAbi::kMach) {
        return kUnsupported;
    }

    const auto exact = exact_lookup(source, word_bits, arg_shape, foreign_syscall_no, target);
    if (exact.supported) {
        return exact;
    }

    return prefix_fallback(source, word_bits, arg_shape, foreign_syscall_no, target);
}

} // namespace xinim::abi::lattice
