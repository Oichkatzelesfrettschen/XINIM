#pragma once

#include <cstddef>
#include <cstdint>
#include <xinim/abi/exec_limits.h>

namespace xinim::kernel::x86_64 {

    inline constexpr size_t kMaximumExecVectorEntries = XINIM_EXEC_MAX_VECTOR_ENTRIES;
    inline constexpr size_t kExecArgumentEnvironmentByteLimit =
        XINIM_EXEC_ARGUMENT_ENVIRONMENT_BYTES;
    inline constexpr size_t kExecAuxiliaryVectorWords = XINIM_EXEC_AUXILIARY_VECTOR_WORDS;
    inline constexpr size_t kExecStackAlignment = XINIM_EXEC_STACK_ALIGNMENT_BYTES;
    inline constexpr size_t kExecFixedLayoutWords = 1U + 1U + 1U + kExecAuxiliaryVectorWords;
    inline constexpr size_t kExecMaximumLayoutOverhead =
        (kExecFixedLayoutWords + 2U * kMaximumExecVectorEntries) * sizeof(uint64_t) +
        (kExecStackAlignment - 1U);

    static_assert(kExecArgumentEnvironmentByteLimit + kExecMaximumLayoutOverhead ==
                  XINIM_X86_64_INITIAL_STACK_SIZE_BYTES);

    enum class ExecVectorKind : uint8_t {
        Arguments,
        Environment,
    };

    enum class ExecCopyError : uint8_t {
        None,
        BadAddress,
        TooLarge,
    };

    struct ExecUserMemoryReader {
        void *context{nullptr};
        int (*copy_bytes)(void *context, void *destination, uintptr_t source,
                          size_t size) noexcept {nullptr};
        int (*copy_string)(void *context, char *destination, uintptr_t source,
                           size_t capacity) noexcept {nullptr};
    };

    struct ExecArguments {
        char strings[kExecArgumentEnvironmentByteLimit]{};
        const char *arguments[kMaximumExecVectorEntries + 1U]{};
        const char *environment[kMaximumExecVectorEntries + 1U]{};
        size_t string_bytes{0U};
        size_t argument_count{0U};
        size_t environment_count{0U};
    };

    [[nodiscard]] bool exec_vector_entry_address(uintptr_t vector_address, size_t index,
                                                 uintptr_t &entry_address) noexcept;

    [[nodiscard]] bool exec_stack_layout_fits(size_t argument_count, size_t environment_count,
                                              size_t string_bytes, size_t &required_bytes) noexcept;

    [[nodiscard]] ExecCopyError append_exec_string(ExecArguments &storage, ExecVectorKind kind,
                                                   const char *string) noexcept;

    [[nodiscard]] ExecCopyError copy_exec_vector(ExecArguments &storage, ExecVectorKind kind,
                                                 uintptr_t user_vector,
                                                 const ExecUserMemoryReader &reader) noexcept;

} // namespace xinim::kernel::x86_64
