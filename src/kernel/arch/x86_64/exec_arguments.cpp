#include "exec_arguments.hpp"

#include <cerrno>
#include <cstring>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        [[nodiscard]] bool add_overflows(size_t left, size_t right) noexcept {
            return left > std::numeric_limits<size_t>::max() - right;
        }

        [[nodiscard]] size_t bounded_string_size(const char *string, size_t capacity) noexcept {
            if (string == nullptr) {
                return 0U;
            }
            for (size_t index = 0U; index < capacity; ++index) {
                if (string[index] == '\0') {
                    return index + 1U;
                }
            }
            return 0U;
        }

        [[nodiscard]] ExecCopyError commit_string(ExecArguments &storage, ExecVectorKind kind,
                                                  size_t string_size) noexcept {
            const char **pointers =
                kind == ExecVectorKind::Arguments ? storage.arguments : storage.environment;
            size_t &count = kind == ExecVectorKind::Arguments ? storage.argument_count
                                                              : storage.environment_count;
            if (count >= kMaximumExecVectorEntries || string_size == 0U ||
                string_size > kExecArgumentEnvironmentByteLimit - storage.string_bytes) {
                return ExecCopyError::TooLarge;
            }
            pointers[count] = storage.strings + storage.string_bytes;
            ++count;
            pointers[count] = nullptr;
            storage.string_bytes += string_size;
            return ExecCopyError::None;
        }

    } // namespace

    bool exec_vector_entry_address(uintptr_t vector_address, size_t index,
                                   uintptr_t &entry_address) noexcept {
        if (index > std::numeric_limits<uintptr_t>::max() / sizeof(uintptr_t)) {
            return false;
        }
        const uintptr_t offset = index * sizeof(uintptr_t);
        if (vector_address > std::numeric_limits<uintptr_t>::max() - offset) {
            return false;
        }
        entry_address = vector_address + offset;
        return true;
    }

    bool exec_stack_layout_fits(size_t argument_count, size_t environment_count,
                                size_t string_bytes, size_t &required_bytes) noexcept {
        required_bytes = 0U;
        if (argument_count > kMaximumExecVectorEntries ||
            environment_count > kMaximumExecVectorEntries ||
            string_bytes > kExecArgumentEnvironmentByteLimit) {
            return false;
        }
        if (add_overflows(kExecFixedLayoutWords, argument_count) ||
            add_overflows(kExecFixedLayoutWords + argument_count, environment_count)) {
            return false;
        }
        const size_t word_count = kExecFixedLayoutWords + argument_count + environment_count;
        if (word_count > std::numeric_limits<size_t>::max() / sizeof(uint64_t)) {
            return false;
        }
        const size_t vector_bytes = word_count * sizeof(uint64_t);
        if (add_overflows(string_bytes, vector_bytes) ||
            add_overflows(string_bytes + vector_bytes, kExecStackAlignment - 1U)) {
            return false;
        }
        required_bytes = string_bytes + vector_bytes + kExecStackAlignment - 1U;
        return required_bytes <= XINIM_X86_64_INITIAL_STACK_SIZE_BYTES;
    }

    ExecCopyError append_exec_string(ExecArguments &storage, ExecVectorKind kind,
                                     const char *string) noexcept {
        const size_t remaining = kExecArgumentEnvironmentByteLimit - storage.string_bytes;
        const size_t string_size = bounded_string_size(string, remaining);
        if (string_size == 0U) {
            return ExecCopyError::TooLarge;
        }
        std::memcpy(storage.strings + storage.string_bytes, string, string_size);
        return commit_string(storage, kind, string_size);
    }

    ExecCopyError copy_exec_vector(ExecArguments &storage, ExecVectorKind kind,
                                   uintptr_t user_vector,
                                   const ExecUserMemoryReader &reader) noexcept {
        if (user_vector == 0U || reader.copy_bytes == nullptr || reader.copy_string == nullptr) {
            return ExecCopyError::BadAddress;
        }
        for (size_t index = 0U; index <= kMaximumExecVectorEntries; ++index) {
            uintptr_t entry_address = 0U;
            if (!exec_vector_entry_address(user_vector, index, entry_address)) {
                return ExecCopyError::BadAddress;
            }
            uintptr_t user_string = 0U;
            if (reader.copy_bytes(reader.context, &user_string, entry_address,
                                  sizeof(user_string)) != 0) {
                return ExecCopyError::BadAddress;
            }
            if (user_string == 0U) {
                return ExecCopyError::None;
            }
            if (index == kMaximumExecVectorEntries) {
                return ExecCopyError::TooLarge;
            }

            const size_t remaining = kExecArgumentEnvironmentByteLimit - storage.string_bytes;
            if (remaining == 0U) {
                return ExecCopyError::TooLarge;
            }
            char *destination = storage.strings + storage.string_bytes;
            const int copy_result =
                reader.copy_string(reader.context, destination, user_string, remaining);
            if (copy_result == -ENAMETOOLONG) {
                return ExecCopyError::TooLarge;
            }
            if (copy_result != 0) {
                return ExecCopyError::BadAddress;
            }
            const size_t string_size = bounded_string_size(destination, remaining);
            if (string_size == 0U) {
                return ExecCopyError::BadAddress;
            }
            const ExecCopyError commit_result = commit_string(storage, kind, string_size);
            if (commit_result != ExecCopyError::None) {
                return commit_result;
            }
        }
        return ExecCopyError::TooLarge;
    }

} // namespace xinim::kernel::x86_64
