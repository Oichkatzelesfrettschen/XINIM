#include "arch/x86_64/exec_arguments.hpp"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace {

    struct ReaderState {
        bool fail_pointer_copy{false};
        bool fail_string_copy{false};
    };

    int copy_bytes(void *context, void *destination, uintptr_t source, size_t size) noexcept {
        const auto *state = static_cast<const ReaderState *>(context);
        if (state->fail_pointer_copy) {
            return -EFAULT;
        }
        std::memcpy(destination, reinterpret_cast<const void *>(source), size);
        return 0;
    }

    int copy_string(void *context, char *destination, uintptr_t source, size_t capacity) noexcept {
        const auto *state = static_cast<const ReaderState *>(context);
        if (state->fail_string_copy) {
            return -EFAULT;
        }
        const auto *input = reinterpret_cast<const char *>(source);
        for (size_t index = 0U; index < capacity; ++index) {
            destination[index] = input[index];
            if (input[index] == '\0') {
                return 0;
            }
        }
        if (capacity != 0U) {
            destination[capacity - 1U] = '\0';
        }
        return -ENAMETOOLONG;
    }

    xinim::kernel::x86_64::ExecUserMemoryReader make_reader(ReaderState &state) noexcept {
        return {&state, copy_bytes, copy_string};
    }

    void test_canonical_stack_budget() {
        using namespace xinim::kernel::x86_64;
        size_t required_bytes = 0U;
        assert(exec_stack_layout_fits(kMaximumExecVectorEntries, kMaximumExecVectorEntries,
                                      kExecArgumentEnvironmentByteLimit, required_bytes));
        assert(required_bytes == XINIM_X86_64_INITIAL_STACK_SIZE_BYTES);
        assert(!exec_stack_layout_fits(kMaximumExecVectorEntries, kMaximumExecVectorEntries,
                                       kExecArgumentEnvironmentByteLimit + 1U, required_bytes));
        assert(!exec_stack_layout_fits(kMaximumExecVectorEntries + 1U, 0U, 0U, required_bytes));
    }

    void test_exact_aggregate_acceptance_and_one_byte_rejection() {
        using namespace xinim::kernel::x86_64;
        std::string exact(kExecArgumentEnvironmentByteLimit - 1U, 'a');
        ExecArguments accepted{};
        assert(append_exec_string(accepted, ExecVectorKind::Arguments, exact.c_str()) ==
               ExecCopyError::None);
        assert(accepted.string_bytes == kExecArgumentEnvironmentByteLimit);

        std::string excessive(kExecArgumentEnvironmentByteLimit, 'b');
        const uintptr_t excessive_vector[] = {
            reinterpret_cast<uintptr_t>(excessive.c_str()),
            0U,
        };
        ReaderState state{};
        ExecArguments rejected{};
        assert(copy_exec_vector(rejected, ExecVectorKind::Arguments,
                                reinterpret_cast<uintptr_t>(excessive_vector),
                                make_reader(state)) == ExecCopyError::TooLarge);
        assert(rejected.string_bytes == 0U);
    }

    void test_argument_environment_aggregate_accounting() {
        using namespace xinim::kernel::x86_64;
        ExecArguments storage{};
        assert(append_exec_string(storage, ExecVectorKind::Arguments, "printf") ==
               ExecCopyError::None);
        assert(append_exec_string(storage, ExecVectorKind::Arguments, "payload") ==
               ExecCopyError::None);
        assert(append_exec_string(storage, ExecVectorKind::Environment, "PATH=/bin") ==
               ExecCopyError::None);
        assert(storage.argument_count == 2U);
        assert(storage.environment_count == 1U);
        assert(storage.string_bytes == 7U + 8U + 10U);
        assert(std::strcmp(storage.arguments[0], "printf") == 0);
        assert(std::strcmp(storage.arguments[1], "payload") == 0);
        assert(std::strcmp(storage.environment[0], "PATH=/bin") == 0);
        assert(storage.arguments[2] == nullptr);
        assert(storage.environment[1] == nullptr);
    }

    void test_32_kib_single_argument() {
        using namespace xinim::kernel::x86_64;
        std::string payload(32U * 1024U, 'x');
        const uintptr_t vector[] = {
            reinterpret_cast<uintptr_t>(payload.c_str()),
            0U,
        };
        ReaderState state{};
        ExecArguments storage{};
        assert(copy_exec_vector(storage, ExecVectorKind::Arguments,
                                reinterpret_cast<uintptr_t>(vector),
                                make_reader(state)) == ExecCopyError::None);
        assert(storage.argument_count == 1U);
        assert(storage.string_bytes == payload.size() + 1U);
        assert(std::strcmp(storage.arguments[0], payload.c_str()) == 0);
    }

    void test_vector_termination_and_pointer_arithmetic() {
        using namespace xinim::kernel::x86_64;
        const char value[] = "x";
        uintptr_t unterminated[kMaximumExecVectorEntries + 1U]{};
        for (uintptr_t &entry : unterminated) {
            entry = reinterpret_cast<uintptr_t>(value);
        }
        ReaderState state{};
        ExecArguments storage{};
        assert(copy_exec_vector(storage, ExecVectorKind::Arguments,
                                reinterpret_cast<uintptr_t>(unterminated),
                                make_reader(state)) == ExecCopyError::TooLarge);

        uintptr_t entry_address = 0U;
        assert(exec_vector_entry_address(0x1000U, 2U, entry_address));
        assert(entry_address == 0x1000U + 2U * sizeof(uintptr_t));
        assert(!exec_vector_entry_address(
            std::numeric_limits<uintptr_t>::max() - sizeof(uintptr_t) + 1U, 1U, entry_address));
        assert(!exec_vector_entry_address(0U, std::numeric_limits<size_t>::max(), entry_address));
    }

    void test_user_copy_failures() {
        using namespace xinim::kernel::x86_64;
        const uintptr_t empty_vector[] = {0U};
        ReaderState state{};
        ExecArguments storage{};
        state.fail_pointer_copy = true;
        assert(copy_exec_vector(storage, ExecVectorKind::Arguments,
                                reinterpret_cast<uintptr_t>(empty_vector),
                                make_reader(state)) == ExecCopyError::BadAddress);

        const char value[] = "x";
        const uintptr_t vector[] = {reinterpret_cast<uintptr_t>(value), 0U};
        state.fail_pointer_copy = false;
        state.fail_string_copy = true;
        assert(copy_exec_vector(storage, ExecVectorKind::Arguments,
                                reinterpret_cast<uintptr_t>(vector),
                                make_reader(state)) == ExecCopyError::BadAddress);
    }

} // namespace

int main() {
    test_canonical_stack_budget();
    test_exact_aggregate_acceptance_and_one_byte_rejection();
    test_argument_environment_aggregate_accounting();
    test_32_kib_single_argument();
    test_vector_termination_and_pointer_arithmetic();
    test_user_copy_failures();
    return 0;
}
