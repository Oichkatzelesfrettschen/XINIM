#include "../../../include/minix/io/format.hpp"       // For declarations
#include "../../../include/minix/io/standard_streams.hpp" // For get_standard_output/error
#include <string>        // For std::string, std::to_string (used by helpers)
#include <vector>        // Potentially for temporary buffers if needed
#include <algorithm>     // For std::reverse if used by number to ascii, std::min
#include <limits>        // For numeric limits if needed
#include <cstring>       // For std::strlen, std::memchr
#include <cstddef>       // For std::byte, size_t
#include <span>          // For std::span, std::as_bytes

// Anonymous namespace for internal helpers, similar to doprintf's static functions
namespace {

// Helper to put a single character to the stream
// Returns true on success, false on error.
bool stream_put_char(minix::io::Stream& stream, char c) {
    std::byte b = static_cast<std::byte>(c);
    auto result = stream.write(std::span<const std::byte>(&b, 1));
    return result && result.value() == 1;
}

// Helper to put a string (null-terminated or view) to the stream
bool stream_put_string(minix::io::Stream& stream, const char* s, size_t len = 0) {
    if (s == nullptr) s = "(null)";
    if (len == 0 && s != nullptr) len = std::strlen(s);
    if (len == 0) return true; // Nothing to write

    auto result = stream.write(std::as_bytes(std::span(s, len)));
    return result && result.value() == len;
}

// --- Number to ASCII conversion (integer part) ---
constexpr int kNumToAsciiMaxDigits = 65; // Max for 64-bit long long in binary + sign + null
void long_to_ascii(long long num, int radix, char* buffer_str, bool is_unsigned = false, bool uppercase_hex = false) {
    char temp_buf[kNumToAsciiMaxDigits];
    int i = 0;
    bool negative = false;
    unsigned long long n_val;

    if (!is_unsigned && radix == 10 && num < 0) {
        negative = true;
        n_val = static_cast<unsigned long long>(-num);
    } else {
        n_val = static_cast<unsigned long long>(num);
    }

    if (n_val == 0) {
        buffer_str[0] = '0';
        buffer_str[1] = '\0';
        return;
    }

    char hex_char_a = uppercase_hex ? 'A' : 'a';
    while (n_val != 0 && i < kNumToAsciiMaxDigits - 2) { // -2 for potential sign and null terminator
        int digit = n_val % radix;
        temp_buf[i++] = (digit < 10) ? ('0' + digit) : (hex_char_a + digit - 10);
        n_val /= radix;
    }

    if (negative && i < kNumToAsciiMaxDigits - 1) {
        temp_buf[i++] = '-';
    }

    int j = 0;
    while (i > 0) {
        buffer_str[j++] = temp_buf[--i];
    }
    buffer_str[j] = '\0';
}

// Helper for padded output, adapted from _printit
bool print_padded_string(minix::io::Stream& stream, const char* str,
                         int width, int precision, bool left_justify, char pad_char, size_t& chars_written_ref) {
    if (str == nullptr) str = "(null)";
    size_t len = std::strlen(str);
    size_t effective_len = len;

    if (precision >= 0 && static_cast<size_t>(precision) < len) {
        effective_len = static_cast<size_t>(precision);
    }

    int padding = width - static_cast<int>(effective_len);

    if (!left_justify) {
        for (int i = 0; i < padding; ++i) {
            if (!stream_put_char(stream, pad_char)) return false;
            chars_written_ref++;
        }
    }

    for (size_t i = 0; i < effective_len; ++i) {
        if (!stream_put_char(stream, str[i])) return false;
        chars_written_ref++;
    }

    if (left_justify) {
        for (int i = 0; i < padding; ++i) {
            if (!stream_put_char(stream, pad_char)) return false;
            chars_written_ref++;
        }
    }
    return true;
}

} // anonymous namespace

namespace minix::io {

Result<size_t> vprint_format(Stream& output_stream, const char* format_str, va_list args) {
    if (!output_stream.is_open() || !output_stream.is_writable()) {
        return std::unexpected(make_error_code(IOError::not_open));
    }

    size_t chars_written = 0;
    const char* p = format_str;
    char num_buffer[kNumToAsciiMaxDigits];
    std::string temp_str_buffer; // For constructing strings with prefixes before padding

    while (*p) {
        if (*p != '%') {
            if (!stream_put_char(output_stream, *p)) {
                return std::unexpected(make_error_code(IOError::io_error));
            }
            chars_written++;
            p++;
            continue;
        }

        p++; // Skip '%'

        bool left_justify = false;
        bool force_sign = false;
        bool space_sign = false;
        bool alternate_form = false;
        char pad_char = ' ';
        bool zero_pad_flag = false; // Differentiate '0' flag from pad_char='0'

        bool flags_done = false;
        while(!flags_done) {
            switch(*p) {
                case '-': left_justify = true; p++; break;
                case '+': force_sign = true; p++; break;
                case ' ': space_sign = true; p++; break;
                case '#': alternate_form = true; p++; break;
                case '0': zero_pad_flag = true; p++; break;
                default: flags_done = true; break;
            }
        }
        if (left_justify) pad_char = ' '; // '-' overrides '0' for pad char type
        else if (zero_pad_flag) pad_char = '0';


        int width = 0;
        if (*p == '*') {
            width = va_arg(args, int);
            p++;
            if (width < 0) {
                left_justify = true; pad_char = ' '; width = -width;
            }
        } else {
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }

        int precision = -1;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                precision = va_arg(args, int);
                p++;
            } else {
                precision = 0;
                while (*p >= '0' && *p <= '9') {
                    precision = precision * 10 + (*p - '0');
                    p++;
                }
            }
             if (precision < 0) precision = 0; // e.g. "%.*s", -5 -> precision 0
        }

        enum LengthMod { L_NONE, L_LONG, L_LONGLONG } length_mod = L_NONE;
        if (*p == 'l') {
            p++;
            if (*p == 'l') { length_mod = L_LONGLONG; p++; }
            else { length_mod = L_LONG; }
        }
        // TODO: Add other length modifiers like 'h', 'z', 'j', 't'

        char current_specifier = *p;
        switch (current_specifier) {
            case 'd': case 'i':
            case 'u': case 'o': case 'x': case 'X': {
                long long val_ll = 0;
                unsigned long long val_ull = 0;
                bool is_signed_type = (current_specifier == 'd' || current_specifier == 'i');

                if (is_signed_type) {
                    if (length_mod == L_LONGLONG) val_ll = va_arg(args, long long);
                    else if (length_mod == L_LONG) val_ll = va_arg(args, long);
                    else val_ll = va_arg(args, int);
                } else {
                    if (length_mod == L_LONGLONG) val_ull = va_arg(args, unsigned long long);
                    else if (length_mod == L_LONG) val_ull = va_arg(args, unsigned long);
                    else val_ull = va_arg(args, unsigned int);
                }

                int radix = 10;
                if (current_specifier == 'o') radix = 8;
                else if (current_specifier == 'x' || current_specifier == 'X') radix = 16;

                bool actual_negative = false;
                temp_str_buffer.clear(); // Used for prefix

                if (is_signed_type) {
                    if (val_ll < 0) { actual_negative = true; temp_str_buffer += '-'; val_ull = static_cast<unsigned long long>(-val_ll); }
                    else { val_ull = static_cast<unsigned long long>(val_ll); if (force_sign) temp_str_buffer += '+'; else if (space_sign) temp_str_buffer += ' '; }
                } else { // unsigned types
                    if (alternate_form && val_ull != 0) {
                        if (radix == 8) temp_str_buffer += '0';
                        else if (radix == 16) temp_str_buffer += (current_specifier == 'x' ? "0x" : "0X");
                    }
                }

                long_to_ascii(is_signed_type ? static_cast<long long>(val_ull) : 0 /*not used for signed path if negative handled*/,
                              radix, num_buffer, !is_signed_type || actual_negative, /* is_unsigned_for_conv */
                              current_specifier == 'X');

                std::string num_str_part = num_buffer;
                // Precision for integers means minimum number of digits
                if (precision > 0 && num_str_part.length() < static_cast<size_t>(precision)) {
                    num_str_part.insert(0, static_cast<size_t>(precision) - num_str_part.length(), '0');
                } else if (precision == 0 && (is_signed_type ? val_ll == 0 : val_ull == 0) && num_str_part == "0") {
                     // "%.0d" or "%.d" with 0 prints nothing or just sign/prefix
                     if (temp_str_buffer.empty() || temp_str_buffer == " ") num_str_part = ""; // If only space sign, then effectively empty
                }


                // Handle '0' padding with prefix correctly: [prefix][zero_padding][number_precision_padded]
                if (pad_char == '0' && !left_justify && !temp_str_buffer.empty() && width > 0) {
                    if (!stream_put_string(output_stream, temp_str_buffer.c_str(), temp_str_buffer.length())) return std::unexpected(make_error_code(IOError::io_error));
                    chars_written += temp_str_buffer.length();
                    // effective width for print_padded_string is total width minus prefix length
                    if (!print_padded_string(output_stream, num_str_part.c_str(), width - temp_str_buffer.length(), -1 /*precision already handled*/, left_justify, pad_char, chars_written))
                        return std::unexpected(make_error_code(IOError::io_error));
                } else {
                    temp_str_buffer += num_str_part; // Combine prefix and number
                    if (!print_padded_string(output_stream, temp_str_buffer.c_str(), width, -1 /*precision already handled*/, left_justify, pad_char, chars_written))
                        return std::unexpected(make_error_code(IOError::io_error));
                }
                break;
            }
            case 'c': {
                char c_val = static_cast<char>(va_arg(args, int));
                char c_str[2] = {c_val, '\0'};
                if (!print_padded_string(output_stream, c_str, width, -1, left_justify, pad_char, chars_written))
                     return std::unexpected(make_error_code(IOError::io_error));
                break;
            }
            case 's': {
                const char* s_val = va_arg(args, const char*);
                if (s_val == nullptr && precision != 0) s_val = "(null)"; // Don't print (null) if precision is .0
                else if (s_val == nullptr && precision == 0) s_val = "";

                if (!print_padded_string(output_stream, s_val, width, precision, left_justify, pad_char, chars_written))
                     return std::unexpected(make_error_code(IOError::io_error));
                break;
            }
            case 'p': {
                void* ptr_val = va_arg(args, void*);
                if (ptr_val == nullptr && precision != 0) { // POSIX specifies (nil) for NULL pointer
                    temp_str_buffer = "(nil)";
                } else if (ptr_val == nullptr && precision == 0) {
                    temp_str_buffer = ""; // "%.0p" with NULL might be empty
                }
                else {
                     long_to_ascii(reinterpret_cast<uintptr_t>(ptr_val), 16, num_buffer, true, false /*lowercase hex for pointers*/);
                     temp_str_buffer = "0x" + std::string(num_buffer);
                }
                if (!print_padded_string(output_stream, temp_str_buffer.c_str(), width, -1 /*precision not for ptr*/, left_justify, pad_char, chars_written))
                     return std::unexpected(make_error_code(IOError::io_error));
                break;
            }
            case '%': {
                if (!stream_put_char(output_stream, '%'))
                    return std::unexpected(make_error_code(IOError::io_error));
                chars_written++;
                break;
            }
            default:
                if (!stream_put_char(output_stream, '%')) return std::unexpected(make_error_code(IOError::io_error));
                chars_written++;
                if (*p) { // Check if not end of format string
                    if (!stream_put_char(output_stream, *p)) return std::unexpected(make_error_code(IOError::io_error));
                    chars_written++;
                } else { // '%' at end of string, just print '%'
                    p--; // Stay on '%' so p++ at end of loop moves past it
                }
                break;
        }
        p++;
    }
    return chars_written;
}

Result<size_t> print_format(Stream& output_stream, const char* format_str, ...) {
    va_list args;
    va_start(args, format_str);
    Result<size_t> result = vprint_format(output_stream, format_str, args);
    va_end(args);
    return result;
}

Result<size_t> print_stdout(const char* format_str, ...) {
    va_list args;
    va_start(args, format_str);
    Result<size_t> result = vprint_format(get_standard_output(), format_str, args);
    va_end(args);
    return result;
}

Result<size_t> print_stderr(const char* format_str, ...) {
    va_list args;
    va_start(args, format_str);
    Result<size_t> result = vprint_format(get_standard_error(), format_str, args);
    va_end(args);
    return result;
}

} // namespace minix::io
