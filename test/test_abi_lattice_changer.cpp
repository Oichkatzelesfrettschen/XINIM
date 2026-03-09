#include "include/xinim/abi/lattice_changer.hpp"
#include "include/xinim/sys/syscalls.h"

#include <cstdlib>
#include <iostream>
#include <print>

namespace {

bool expect(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::println(std::cerr, "FAIL: {}", message);
        ++failures;
        return false;
    }
    return true;
}

} // namespace

int main() {
    int failures = 0;

    const auto linux64_getpid = xinim::abi::lattice::transform_syscall(
        xinim::abi::lattice::SourceAbi::kLinux, 64, 0, 39,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    expect(linux64_getpid.supported &&
               linux64_getpid.syscall_no == static_cast<uint32_t>(SYS_getpid),
           "Linux64 getpid must map to SYS_getpid",
           failures);

    const auto linux32_write = xinim::abi::lattice::transform_syscall(
        xinim::abi::lattice::SourceAbi::kLinux, 32, 0, 4,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    expect(linux32_write.supported &&
               linux32_write.syscall_no == static_cast<uint32_t>(SYS_write),
           "Linux32 write must map to SYS_write",
           failures);

    const auto bsd_read = xinim::abi::lattice::transform_syscall(
        xinim::abi::lattice::SourceAbi::kBsd, 64, 0, 0,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    expect(!bsd_read.supported,
           "BSD bridge is intentionally disabled in early bring-up",
           failures);

    const auto mach_call = xinim::abi::lattice::transform_syscall(
        xinim::abi::lattice::SourceAbi::kMach, 64, 0, 1,
        xinim::abi::lattice::TransformTarget::kNativeSyscalls);
    expect(!mach_call.supported,
           "Mach translation should stay unsupported until message bridge exists",
           failures);

    const auto raw = xinim::abi::lattice::encode_tagged_syscall(
        xinim::abi::lattice::SourceAbi::kLinux, 32, 3, 114);
    expect(xinim::abi::lattice::is_tagged(raw),
           "encoded syscall must carry the tag bit",
           failures);
    const auto decoded = xinim::abi::lattice::decode_tagged_syscall(raw);
    expect(decoded.source == xinim::abi::lattice::SourceAbi::kLinux &&
               decoded.word_bits == 32 &&
               decoded.arg_shape == 3 &&
               decoded.foreign_syscall_no == 114,
           "decode must recover source ABI, width, shape, and syscall number",
           failures);

    expect(xinim::abi::lattice::shared_prefix_bits(0xA0000000u, 0xAFFFFFFFu) >= 4,
           "shared-prefix metric should detect common high bits",
           failures);

    if (failures != 0) {
        std::println(std::cerr, "{} lattice changer ABI test(s) failed.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "ALL lattice changer ABI tests passed.");
    return EXIT_SUCCESS;
}

