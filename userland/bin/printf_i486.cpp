#include "printf_core.hpp"

#include <xinim/userland/dietlibc_cpp23.hpp>

namespace {

    using xinim::userland::posix_printf::OutputSink;
    using xinim::userland::posix_printf::Size;

    bool write_to_descriptor(void *context, const char *bytes, Size byte_count) noexcept {
        const int file_descriptor = *static_cast<int *>(context);
        while (byte_count > 0U) {
            constexpr Size maximum_chunk = static_cast<Size>(~0U);
            const Size chunk_size = byte_count < maximum_chunk ? byte_count : maximum_chunk;
            const auto bytes_written =
                write(file_descriptor, bytes, static_cast<unsigned>(chunk_size));
            if (bytes_written <= 0)
                return false;
            const auto completed = static_cast<Size>(bytes_written);
            bytes += completed;
            byte_count -= completed;
        }
        return true;
    }

    bool write_literal(int file_descriptor, const char *text) noexcept {
        Size length = 0U;
        while (text[length] != '\0')
            ++length;
        return write_to_descriptor(&file_descriptor, text, length);
    }

} // namespace

extern "C" int xinim_user_main(int argument_count, char **arguments) asm("main");

extern "C" int xinim_user_main(int argument_count, char **arguments) {
    if (argument_count < 2) {
        static_cast<void>(write_literal(2, "printf: format operand is required\n"));
        return 1;
    }

    int standard_output_descriptor = 1;
    int standard_error_descriptor = 2;
    const OutputSink standard_output{&standard_output_descriptor, write_to_descriptor};
    const OutputSink standard_error{&standard_error_descriptor, write_to_descriptor};
    const auto result = xinim::userland::posix_printf::format(
        standard_output, standard_error, arguments[1], argument_count - 2, arguments + 2);
    return result.exit_status;
}
