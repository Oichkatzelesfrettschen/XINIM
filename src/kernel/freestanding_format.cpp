#include "freestanding_format.hpp"

#include <climits>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace xinim::kernel::format {
    namespace {

        enum class FormatLength {
            Default,
            Long,
            LongLong,
            Size,
        };

        struct CharacterRun {
            char character;
            std::size_t amount;
        };

        struct CharacterField {
            char character;
            std::size_t width;
            bool left_aligned;
        };

        struct FormatOutput {
            char *buffer;
            std::size_t capacity;
            std::size_t count;
            CharacterSink sink;
            void *sink_context;
            bool failed;

            void advance(std::size_t amount) noexcept {
                if (amount > std::numeric_limits<std::size_t>::max() - count) {
                    failed = true;
                    return;
                }
                count += amount;
            }

            void put(char character) noexcept {
                if (failed) {
                    return;
                }
                if (sink != nullptr) {
                    sink(sink_context, character);
                } else if (buffer != nullptr && capacity != 0U && count < capacity - 1U) {
                    buffer[count] = character;
                }
                advance(1U);
            }

            void put_repeated(CharacterRun run) noexcept {
                if (failed || run.amount == 0U) {
                    return;
                }
                if (sink != nullptr) {
                    for (std::size_t index = 0U; index < run.amount; ++index) {
                        sink(sink_context, run.character);
                    }
                } else if (buffer != nullptr && capacity != 0U && count < capacity - 1U) {
                    const std::size_t available = capacity - 1U - count;
                    const std::size_t written = run.amount < available ? run.amount : available;
                    for (std::size_t index = 0U; index < written; ++index) {
                        buffer[count + index] = run.character;
                    }
                }
                advance(run.amount);
            }

            void finish() noexcept {
                if (sink != nullptr || buffer == nullptr || capacity == 0U) {
                    return;
                }
                const std::size_t terminator = count < capacity ? count : capacity - 1U;
                buffer[terminator] = '\0';
            }
        };

        [[nodiscard]] int format_message(FormatOutput &output, const char *format,
                                         va_list arguments) noexcept {
            if (format == nullptr ||
                (output.sink == nullptr && output.buffer == nullptr && output.capacity != 0U)) {
                return -1;
            }

            auto put_string = [&output](const char *value, std::size_t width,
                                        bool left_aligned) noexcept {
                if (value == nullptr) {
                    value = "(null)";
                }
                std::size_t length = 0U;
                while (value[length] != '\0') {
                    ++length;
                }
                const std::size_t padding = width > length ? width - length : 0U;
                if (!left_aligned) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
                while (*value != '\0') {
                    output.put(*value++);
                }
                if (left_aligned) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
            };

            auto put_character = [&output](const CharacterField &field) noexcept {
                const std::size_t padding = field.width > 1U ? field.width - 1U : 0U;
                if (!field.left_aligned) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
                output.put(field.character);
                if (field.left_aligned) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
            };

            auto put_number = [&output](uint64_t value, unsigned int base, bool uppercase,
                                        bool negative, bool pointer_prefix, std::size_t width,
                                        bool left_aligned, bool zero_padded) noexcept {
                char digits[64];
                std::size_t digit_count = 0U;
                do {
                    const unsigned int digit = static_cast<unsigned int>(value % base);
                    const char alphabet_base = uppercase ? 'A' : 'a';
                    digits[digit_count++] =
                        digit < 10U ? static_cast<char>('0' + digit)
                                    : static_cast<char>(static_cast<unsigned int>(alphabet_base) +
                                                        digit - 10U);
                    value /= base;
                } while (value != 0U);

                const std::size_t prefix_length = (negative ? 1U : 0U) + (pointer_prefix ? 2U : 0U);
                const std::size_t value_length = digit_count + prefix_length;
                const std::size_t padding = width > value_length ? width - value_length : 0U;
                if (!left_aligned && !zero_padded) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
                if (negative) {
                    output.put('-');
                }
                if (pointer_prefix) {
                    output.put('0');
                    output.put('x');
                }
                if (!left_aligned && zero_padded) {
                    output.put_repeated(CharacterRun{'0', padding});
                }
                while (digit_count > 0U) {
                    output.put(digits[--digit_count]);
                }
                if (left_aligned) {
                    output.put_repeated(CharacterRun{' ', padding});
                }
            };

            while (*format != '\0' && !output.failed) {
                if (*format != '%') {
                    output.put(*format++);
                    continue;
                }
                ++format;

                bool left_aligned = false;
                bool zero_padded = false;
                bool has_flags = false;
                bool has_zero_flag = false;
                bool parsing_flags = true;
                while (parsing_flags) {
                    switch (*format) {
                    case '-':
                        has_flags = true;
                        left_aligned = true;
                        ++format;
                        break;
                    case '0':
                        has_flags = true;
                        has_zero_flag = true;
                        zero_padded = true;
                        ++format;
                        break;
                    default:
                        parsing_flags = false;
                        break;
                    }
                }
                if (left_aligned) {
                    zero_padded = false;
                }

                std::size_t width = 0U;
                bool has_width = false;
                while (*format >= '0' && *format <= '9') {
                    has_width = true;
                    const unsigned int digit = static_cast<unsigned int>(*format - '0');
                    if (width > (static_cast<std::size_t>(INT_MAX) - digit) / 10U) {
                        output.failed = true;
                        break;
                    }
                    width = width * 10U + digit;
                    ++format;
                }
                if (output.failed) {
                    break;
                }

                FormatLength length = FormatLength::Default;
                if (*format == 'l') {
                    ++format;
                    length = FormatLength::Long;
                    if (*format == 'l') {
                        ++format;
                        length = FormatLength::LongLong;
                    }
                } else if (*format == 'z') {
                    ++format;
                    length = FormatLength::Size;
                }

                auto read_unsigned = [&arguments, length]() noexcept -> uint64_t {
                    switch (length) {
                    case FormatLength::Long:
                        return va_arg(arguments, unsigned long);
                    case FormatLength::LongLong:
                        return va_arg(arguments, unsigned long long);
                    case FormatLength::Size:
                        return va_arg(arguments, std::size_t);
                    case FormatLength::Default:
                        return va_arg(arguments, unsigned int);
                    }
                    return 0U;
                };

                auto read_signed = [&arguments, length]() noexcept -> int64_t {
                    switch (length) {
                    case FormatLength::Long:
                        return va_arg(arguments, long);
                    case FormatLength::LongLong:
                        return va_arg(arguments, long long);
                    case FormatLength::Size:
                        return va_arg(arguments, std::make_signed_t<std::size_t>);
                    case FormatLength::Default:
                        return va_arg(arguments, int);
                    }
                    return 0;
                };

                switch (*format) {
                case 's':
                    if (length != FormatLength::Default || has_zero_flag) {
                        output.failed = true;
                        break;
                    }
                    put_string(va_arg(arguments, const char *), width, left_aligned);
                    break;
                case 'd':
                case 'i': {
                    const int64_t signed_value = read_signed();
                    const bool negative = signed_value < 0;
                    const uint64_t magnitude =
                        negative ? uint64_t{0} - static_cast<uint64_t>(signed_value)
                                 : static_cast<uint64_t>(signed_value);
                    put_number(magnitude, 10U, false, negative, false, width, left_aligned,
                               zero_padded);
                    break;
                }
                case 'u':
                    put_number(read_unsigned(), 10U, false, false, false, width, left_aligned,
                               zero_padded);
                    break;
                case 'x':
                case 'X':
                    put_number(read_unsigned(), 16U, *format == 'X', false, false, width,
                               left_aligned, zero_padded);
                    break;
                case 'o':
                    put_number(read_unsigned(), 8U, false, false, false, width, left_aligned,
                               zero_padded);
                    break;
                case 'p':
                    if (length != FormatLength::Default) {
                        output.failed = true;
                        break;
                    }
                    put_number(reinterpret_cast<uintptr_t>(va_arg(arguments, void *)), 16U, false,
                               false, true, width, left_aligned, zero_padded);
                    break;
                case 'c':
                    if (length != FormatLength::Default || has_zero_flag) {
                        output.failed = true;
                        break;
                    }
                    put_character(CharacterField{static_cast<char>(va_arg(arguments, int)), width,
                                                 left_aligned});
                    break;
                case '%':
                    if (length != FormatLength::Default || has_flags || has_width) {
                        output.failed = true;
                        break;
                    }
                    output.put('%');
                    break;
                default:
                    output.failed = true;
                    break;
                }
                if (*format != '\0') {
                    ++format;
                }
            }

            output.finish();
            if (output.failed || output.count > static_cast<std::size_t>(INT_MAX)) {
                return -1;
            }
            return static_cast<int>(output.count);
        }

    } // namespace

    int to_buffer(char *buffer, std::size_t capacity, const char *format,
                  va_list arguments) noexcept {
        FormatOutput output{buffer, capacity, 0U, nullptr, nullptr, false};
        return format_message(output, format, arguments);
    }

    int to_sink(CharacterSink sink, void *context, const char *format, va_list arguments) noexcept {
        if (sink == nullptr) {
            return -1;
        }
        FormatOutput output{nullptr, 0U, 0U, sink, context, false};
        return format_message(output, format, arguments);
    }

} // namespace xinim::kernel::format
