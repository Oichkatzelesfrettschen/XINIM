#pragma once

namespace xinim::userland::posix_printf {

    using Size = decltype(sizeof(0));

    struct OutputSink {
        void *context;
        bool (*write)(void *context, const char *bytes, Size byte_count) noexcept;
    };

    struct FormatResult {
        int exit_status;
        bool stopped_by_b_escape;
    };

    namespace detail {

        struct Execution {
            OutputSink standard_output;
            OutputSink standard_error;
            int exit_status{0};
            bool stop_output{false};
            bool stopped_by_b_escape{false};
        };

        struct FormatSpecification {
            bool left_adjust{false};
            bool always_sign{false};
            bool leading_space{false};
            bool alternative_form{false};
            bool zero_pad{false};
            Size field_width{0};
            bool precision_specified{false};
            Size precision{0};
            char conversion{'\0'};
        };

        struct ParsedInteger {
            unsigned long magnitude{0};
            bool negative{false};
            bool valid{false};
            bool complete{false};
            bool overflow{false};
        };

        inline constexpr bool is_octal_digit(char character) noexcept {
            return character >= '0' && character <= '7';
        }

        inline constexpr bool is_decimal_digit(char character) noexcept {
            return character >= '0' && character <= '9';
        }

        inline constexpr int digit_value(char character) noexcept {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        }

        inline bool emit(Execution &execution, OutputSink sink, const char *bytes,
                         Size byte_count) noexcept {
            if (byte_count == 0)
                return true;
            if (sink.write == nullptr || !sink.write(sink.context, bytes, byte_count)) {
                execution.exit_status = 1;
                execution.stop_output = true;
                return false;
            }
            return true;
        }

        inline bool emit_output(Execution &execution, const char *bytes, Size byte_count) noexcept {
            if (execution.stop_output)
                return false;
            return emit(execution, execution.standard_output, bytes, byte_count);
        }

        inline bool emit_character(Execution &execution, char character) noexcept {
            return emit_output(execution, &character, 1U);
        }

        inline bool emit_repeated(Execution &execution, char character, Size count) noexcept {
            char buffer[32];
            for (char &byte : buffer)
                byte = character;
            while (count > 0U && !execution.stop_output) {
                const Size chunk_size = count < sizeof(buffer) ? count : sizeof(buffer);
                if (!emit_output(execution, buffer, chunk_size))
                    return false;
                count -= chunk_size;
            }
            return !execution.stop_output;
        }

        inline Size string_length(const char *text) noexcept {
            Size length = 0U;
            while (text[length] != '\0')
                ++length;
            return length;
        }

        inline void report_integer_error(Execution &execution, const char *argument) noexcept {
            execution.exit_status = 1;
            static constexpr char prefix[] = "printf: invalid integer: ";
            static constexpr char suffix[] = "\n";
            static_cast<void>(
                emit(execution, execution.standard_error, prefix, sizeof(prefix) - 1U));
            static_cast<void>(
                emit(execution, execution.standard_error, argument, string_length(argument)));
            static_cast<void>(
                emit(execution, execution.standard_error, suffix, sizeof(suffix) - 1U));
        }

        inline void report_format_error(Execution &execution) noexcept {
            execution.exit_status = 1;
            static constexpr char message[] = "printf: invalid conversion specification\n";
            static_cast<void>(
                emit(execution, execution.standard_error, message, sizeof(message) - 1U));
        }

        inline ParsedInteger parse_integer(const char *text) noexcept {
            ParsedInteger result{};
            Size index = 0U;
            if (text[index] == '-' || text[index] == '+') {
                result.negative = text[index] == '-';
                ++index;
            }

            if (text[index] == '\'' || text[index] == '"') {
                ++index;
                if (text[index] == '\0')
                    return result;
                result.magnitude = static_cast<unsigned char>(text[index]);
                result.valid = true;
                ++index;
                result.complete = text[index] == '\0';
                return result;
            }

            unsigned base = 10U;
            if (text[index] == '0') {
                if ((text[index + 1U] == 'x' || text[index + 1U] == 'X') &&
                    digit_value(text[index + 2U]) >= 0 && digit_value(text[index + 2U]) < 16) {
                    base = 16U;
                    index += 2U;
                } else {
                    base = 8U;
                }
            }

            constexpr unsigned long maximum = ~0UL;
            while (true) {
                const int digit = digit_value(text[index]);
                if (digit < 0 || static_cast<unsigned>(digit) >= base)
                    break;
                result.valid = true;
                const auto unsigned_digit = static_cast<unsigned long>(digit);
                if (result.magnitude > (maximum - unsigned_digit) / base) {
                    result.magnitude = maximum;
                    result.overflow = true;
                } else if (!result.overflow) {
                    result.magnitude = (result.magnitude * base) + unsigned_digit;
                }
                ++index;
            }
            result.complete = result.valid && text[index] == '\0';
            return result;
        }

        inline Size parse_decimal_size(const char *format, Size &index) noexcept {
            Size value = 0U;
            constexpr Size maximum = static_cast<Size>(~static_cast<Size>(0U));
            while (is_decimal_digit(format[index])) {
                const Size digit = static_cast<Size>(format[index] - '0');
                if (value > (maximum - digit) / 10U) {
                    value = maximum;
                } else {
                    value = (value * 10U) + digit;
                }
                ++index;
            }
            return value;
        }

        inline bool consumes_argument(char conversion) noexcept {
            switch (conversion) {
            case 'b':
            case 'c':
            case 'd':
            case 'i':
            case 'o':
            case 's':
            case 'u':
            case 'x':
            case 'X':
                return true;
            default:
                return false;
            }
        }

        inline bool is_supported_conversion(char conversion) noexcept {
            return consumes_argument(conversion) || conversion == '%';
        }

        inline bool parse_format_specification(const char *format, Size &index,
                                               FormatSpecification &specification) noexcept {
            bool scanning_flags = true;
            while (scanning_flags) {
                switch (format[index]) {
                case '-':
                    specification.left_adjust = true;
                    ++index;
                    break;
                case '+':
                    specification.always_sign = true;
                    ++index;
                    break;
                case ' ':
                    specification.leading_space = true;
                    ++index;
                    break;
                case '#':
                    specification.alternative_form = true;
                    ++index;
                    break;
                case '0':
                    specification.zero_pad = true;
                    ++index;
                    break;
                default:
                    scanning_flags = false;
                    break;
                }
            }

            if (is_decimal_digit(format[index])) {
                specification.field_width = parse_decimal_size(format, index);
            }
            if (format[index] == '.') {
                specification.precision_specified = true;
                ++index;
                specification.precision = parse_decimal_size(format, index);
            }

            specification.conversion = format[index];
            if (format[index] == '\0' || !is_supported_conversion(format[index]))
                return false;
            ++index;
            return true;
        }

        inline void emit_padded_bytes(Execution &execution,
                                      const FormatSpecification &specification, const char *bytes,
                                      Size byte_count) noexcept {
            const Size visible_count =
                specification.precision_specified && byte_count > specification.precision
                    ? specification.precision
                    : byte_count;
            const Size padding = specification.field_width > visible_count
                                     ? specification.field_width - visible_count
                                     : 0U;
            if (!specification.left_adjust) {
                static_cast<void>(emit_repeated(execution, ' ', padding));
            }
            static_cast<void>(emit_output(execution, bytes, visible_count));
            if (specification.left_adjust) {
                static_cast<void>(emit_repeated(execution, ' ', padding));
            }
        }

        inline char digit_character(unsigned digit, bool uppercase) noexcept {
            if (digit < 10U)
                return static_cast<char>('0' + digit);
            return static_cast<char>((uppercase ? 'A' : 'a') + (digit - 10U));
        }

        inline void emit_integer(Execution &execution, const FormatSpecification &specification,
                                 unsigned long magnitude, bool negative, unsigned base,
                                 bool uppercase, bool signed_conversion) noexcept {
            char reversed_digits[sizeof(unsigned long) * 3U];
            Size digit_count = 0U;
            if (magnitude == 0UL) {
                if (!specification.precision_specified || specification.precision != 0U) {
                    reversed_digits[digit_count++] = '0';
                }
            } else {
                while (magnitude != 0UL) {
                    const auto digit = static_cast<unsigned>(magnitude % base);
                    reversed_digits[digit_count++] = digit_character(digit, uppercase);
                    magnitude /= base;
                }
            }

            char prefix[2];
            Size prefix_count = 0U;
            if (signed_conversion) {
                if (negative) {
                    prefix[prefix_count++] = '-';
                } else if (specification.always_sign) {
                    prefix[prefix_count++] = '+';
                } else if (specification.leading_space) {
                    prefix[prefix_count++] = ' ';
                }
            }

            Size precision_zeros = 0U;
            if (specification.precision_specified && specification.precision > digit_count) {
                precision_zeros = specification.precision - digit_count;
            }
            if (specification.alternative_form && base == 8U) {
                const bool already_starts_with_zero =
                    (precision_zeros > 0U) ||
                    (digit_count > 0U && reversed_digits[digit_count - 1U] == '0');
                if (!already_starts_with_zero)
                    ++precision_zeros;
            } else if (specification.alternative_form && base == 16U && digit_count > 0U &&
                       !(digit_count == 1U && reversed_digits[0] == '0')) {
                prefix[prefix_count++] = '0';
                prefix[prefix_count++] = uppercase ? 'X' : 'x';
            }

            Size content_width = prefix_count + precision_zeros + digit_count;
            Size field_zeros = 0U;
            if (specification.zero_pad && !specification.left_adjust &&
                !specification.precision_specified && specification.field_width > content_width) {
                field_zeros = specification.field_width - content_width;
                content_width = specification.field_width;
            }
            const Size spaces = specification.field_width > content_width
                                    ? specification.field_width - content_width
                                    : 0U;

            if (!specification.left_adjust)
                static_cast<void>(emit_repeated(execution, ' ', spaces));
            static_cast<void>(emit_output(execution, prefix, prefix_count));
            static_cast<void>(emit_repeated(execution, '0', field_zeros + precision_zeros));
            while (digit_count > 0U && !execution.stop_output) {
                --digit_count;
                static_cast<void>(emit_character(execution, reversed_digits[digit_count]));
            }
            if (specification.left_adjust)
                static_cast<void>(emit_repeated(execution, ' ', spaces));
        }

        inline char simple_escape(char character) noexcept {
            switch (character) {
            case '\\':
                return '\\';
            case 'a':
                return '\a';
            case 'b':
                return '\b';
            case 'f':
                return '\f';
            case 'n':
                return '\n';
            case 'r':
                return '\r';
            case 't':
                return '\t';
            case 'v':
                return '\v';
            default:
                return '\0';
            }
        }

        inline void emit_format_escape(Execution &execution, const char *format,
                                       Size &index) noexcept {
            ++index;
            if (is_octal_digit(format[index])) {
                unsigned value = 0U;
                unsigned digit_count = 0U;
                while (digit_count < 3U && is_octal_digit(format[index])) {
                    value = (value * 8U) + static_cast<unsigned>(format[index] - '0');
                    ++index;
                    ++digit_count;
                }
                static_cast<void>(emit_character(execution, static_cast<char>(value & 0xFFU)));
                return;
            }

            const char escaped = simple_escape(format[index]);
            if (escaped != '\0') {
                static_cast<void>(emit_character(execution, escaped));
                ++index;
                return;
            }

            static_cast<void>(emit_character(execution, '\\'));
            if (format[index] != '\0') {
                static_cast<void>(emit_character(execution, format[index]));
                ++index;
            }
        }

        template <typename EmitByte>
        inline Size decode_b_argument(const char *argument, bool precision_specified,
                                      Size precision, EmitByte emit_byte,
                                      bool &stop_requested) noexcept {
            Size source_index = 0U;
            Size converted_count = 0U;
            const auto emit_converted = [&](char byte) {
                if (!precision_specified || converted_count < precision)
                    emit_byte(byte);
                ++converted_count;
            };

            while (argument[source_index] != '\0') {
                if (argument[source_index] != '\\') {
                    emit_converted(argument[source_index++]);
                    continue;
                }

                ++source_index;
                if (argument[source_index] == 'c') {
                    stop_requested = true;
                    break;
                }
                if (argument[source_index] == '0') {
                    ++source_index;
                    unsigned value = 0U;
                    unsigned digit_count = 0U;
                    while (digit_count < 3U && is_octal_digit(argument[source_index])) {
                        value = (value * 8U) + static_cast<unsigned>(argument[source_index] - '0');
                        ++source_index;
                        ++digit_count;
                    }
                    emit_converted(static_cast<char>(value & 0xFFU));
                    continue;
                }

                const char escaped = simple_escape(argument[source_index]);
                if (escaped != '\0') {
                    emit_converted(escaped);
                    ++source_index;
                    continue;
                }

                emit_converted('\\');
                if (argument[source_index] != '\0')
                    emit_converted(argument[source_index++]);
            }

            return precision_specified && converted_count > precision ? precision : converted_count;
        }

        inline void emit_b_argument(Execution &execution, const FormatSpecification &specification,
                                    const char *argument) noexcept {
            bool stop_requested = false;
            const Size converted_count = decode_b_argument(
                argument, specification.precision_specified, specification.precision,
                [](char) noexcept {}, stop_requested);
            const Size padding = specification.field_width > converted_count
                                     ? specification.field_width - converted_count
                                     : 0U;
            if (!specification.left_adjust) {
                static_cast<void>(emit_repeated(execution, ' ', padding));
            }

            bool output_stop_requested = false;
            static_cast<void>(decode_b_argument(
                argument, specification.precision_specified, specification.precision,
                [&execution](char byte) noexcept {
                    static_cast<void>(emit_character(execution, byte));
                },
                output_stop_requested));
            if (output_stop_requested || stop_requested) {
                execution.stop_output = true;
                execution.stopped_by_b_escape = true;
            }
            if (specification.left_adjust && !execution.stop_output) {
                static_cast<void>(emit_repeated(execution, ' ', padding));
            }
        }

        inline void emit_conversion(Execution &execution, const FormatSpecification &specification,
                                    const char *argument, bool argument_present) noexcept {
            switch (specification.conversion) {
            case '%':
                static_cast<void>(emit_character(execution, '%'));
                return;
            case 's': {
                const char *text = argument_present ? argument : "";
                emit_padded_bytes(execution, specification, text, string_length(text));
                return;
            }
            case 'c': {
                const char *text = argument_present ? argument : "";
                const Size byte_count = text[0] == '\0' ? 0U : 1U;
                emit_padded_bytes(execution, specification, text, byte_count);
                return;
            }
            case 'b':
                emit_b_argument(execution, specification, argument_present ? argument : "");
                return;
            default:
                break;
            }

            const char *numeric_argument = argument_present ? argument : "0";
            ParsedInteger parsed = parse_integer(numeric_argument);
            bool reported_error = false;
            if (!parsed.valid || !parsed.complete || parsed.overflow) {
                report_integer_error(execution, numeric_argument);
                reported_error = true;
            }

            if (specification.conversion == 'd' || specification.conversion == 'i') {
                constexpr unsigned long signed_maximum = static_cast<unsigned long>(__LONG_MAX__);
                constexpr unsigned long negative_limit = signed_maximum + 1UL;
                if ((!parsed.negative && parsed.magnitude > signed_maximum) ||
                    (parsed.negative && parsed.magnitude > negative_limit)) {
                    if (!reported_error)
                        report_integer_error(execution, numeric_argument);
                    parsed.magnitude = parsed.negative ? negative_limit : signed_maximum;
                }
                emit_integer(execution, specification, parsed.magnitude, parsed.negative, 10U,
                             false, true);
                return;
            }

            const unsigned long unsigned_value =
                parsed.negative ? 0UL - parsed.magnitude : parsed.magnitude;
            switch (specification.conversion) {
            case 'o':
                emit_integer(execution, specification, unsigned_value, false, 8U, false, false);
                break;
            case 'u':
                emit_integer(execution, specification, unsigned_value, false, 10U, false, false);
                break;
            case 'x':
                emit_integer(execution, specification, unsigned_value, false, 16U, false, false);
                break;
            case 'X':
                emit_integer(execution, specification, unsigned_value, false, 16U, true, false);
                break;
            default:
                break;
            }
        }

    } // namespace detail

    inline FormatResult format(OutputSink standard_output, OutputSink standard_error,
                               const char *format_operand, int argument_count,
                               char *const *arguments) noexcept {
        detail::Execution execution{standard_output, standard_error};
        int argument_index = 0;

        while (!execution.stop_output) {
            Size format_index = 0U;
            bool found_argument_conversion = false;
            while (format_operand[format_index] != '\0' && !execution.stop_output) {
                if (format_operand[format_index] == '\\') {
                    detail::emit_format_escape(execution, format_operand, format_index);
                    continue;
                }
                if (format_operand[format_index] != '%') {
                    static_cast<void>(
                        detail::emit_character(execution, format_operand[format_index++]));
                    continue;
                }

                ++format_index;
                detail::FormatSpecification specification{};
                if (!detail::parse_format_specification(format_operand, format_index,
                                                        specification)) {
                    detail::report_format_error(execution);
                    execution.stop_output = true;
                    break;
                }

                const bool consumes = detail::consumes_argument(specification.conversion);
                found_argument_conversion = found_argument_conversion || consumes;
                const bool argument_present = consumes && argument_index < argument_count;
                const char *argument = argument_present ? arguments[argument_index] : "";
                if (argument_present)
                    ++argument_index;
                detail::emit_conversion(execution, specification, argument, argument_present);
            }

            if (execution.stop_output || argument_index >= argument_count ||
                !found_argument_conversion) {
                break;
            }
        }

        return FormatResult{execution.exit_status, execution.stopped_by_b_escape};
    }

} // namespace xinim::userland::posix_printf
