#pragma once

#include <cstdint>

namespace xinim::abi::lattice {

enum class SourceAbi : uint8_t {
    kNative = 0,
    kLinux = 1,
    kBsd = 2,
    kSvr4 = 3,
    kMach = 4,
};

enum class TransformTarget : uint8_t {
    kNativeSyscalls = 0,
    kLegacyLinux64Table = 1,
};

struct TaggedSyscall {
    SourceAbi source;
    uint8_t word_bits;
    uint8_t arg_shape;
    uint32_t foreign_syscall_no;
};

struct TransformResult {
    uint32_t syscall_no;
    bool supported;
    uint16_t estimated_cycles;
};

constexpr uint64_t kTagBit = (1ULL << 63);
constexpr uint64_t kTagSourceMask = (0x7ULL << 60);
constexpr uint64_t kTagWordBitsMask = (1ULL << 59); // 0=64, 1=32
constexpr uint64_t kTagArgShapeMask = (0xFFULL << 32);
constexpr uint64_t kTagSyscallMask = 0x00000000FFFFFFFFULL;

[[nodiscard]] constexpr bool is_tagged(uint64_t raw) {
    return (raw & kTagBit) != 0;
}

[[nodiscard]] constexpr uint64_t encode_tagged_syscall(
    SourceAbi source,
    uint32_t word_bits,
    uint8_t arg_shape,
    uint32_t foreign_syscall_no) {
    const uint64_t source_tag = (static_cast<uint64_t>(source) & 0x7ULL) << 60;
    const uint64_t bits_tag = (word_bits == 32U) ? kTagWordBitsMask : 0ULL;
    const uint64_t shape_tag = (static_cast<uint64_t>(arg_shape) & 0xFFULL) << 32;
    return kTagBit | source_tag | bits_tag | shape_tag | foreign_syscall_no;
}

[[nodiscard]] constexpr TaggedSyscall decode_tagged_syscall(uint64_t raw) {
    return TaggedSyscall{
        .source = static_cast<SourceAbi>((raw & kTagSourceMask) >> 60),
        .word_bits = (raw & kTagWordBitsMask) ? static_cast<uint8_t>(32) : static_cast<uint8_t>(64),
        .arg_shape = static_cast<uint8_t>((raw & kTagArgShapeMask) >> 32),
        .foreign_syscall_no = static_cast<uint32_t>(raw & kTagSyscallMask),
    };
}

[[nodiscard]] uint32_t shared_prefix_bits(uint32_t a, uint32_t b);

[[nodiscard]] TransformResult transform_syscall(
    SourceAbi source,
    uint32_t word_bits,
    uint8_t arg_shape,
    uint32_t foreign_syscall_no,
    TransformTarget target);

} // namespace xinim::abi::lattice

