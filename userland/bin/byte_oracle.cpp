#include <xinim/userland/dietlibc_cpp23.hpp>

extern "C" {
#include <fcntl.h>
}

namespace {

    using Size = decltype(sizeof(0));

    bool write_all(int file_descriptor, const char *bytes, Size byte_count) noexcept {
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

    void report(const char *message) noexcept {
        Size length = 0U;
        while (message[length] != '\0')
            ++length;
        static_cast<void>(write_all(2, message, length));
    }

    int hexadecimal_value(char character) noexcept {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10;
        if (character >= 'A' && character <= 'F')
            return character - 'A' + 10;
        return -1;
    }

    bool decode_expected_byte(const char *expected_hexadecimal, Size byte_index,
                              unsigned char &expected_byte) noexcept {
        const Size character_index = byte_index * 2U;
        const int high_nibble = hexadecimal_value(expected_hexadecimal[character_index]);
        const int low_nibble = hexadecimal_value(expected_hexadecimal[character_index + 1U]);
        if (high_nibble < 0 || low_nibble < 0)
            return false;
        expected_byte = static_cast<unsigned char>((high_nibble << 4U) | low_nibble);
        return true;
    }

    bool has_valid_hexadecimal_length(const char *expected_hexadecimal,
                                      Size &expected_byte_count) noexcept {
        Size character_count = 0U;
        while (expected_hexadecimal[character_count] != '\0') {
            if (hexadecimal_value(expected_hexadecimal[character_count]) < 0)
                return false;
            ++character_count;
        }
        if ((character_count % 2U) != 0U)
            return false;
        expected_byte_count = character_count / 2U;
        return true;
    }

} // namespace

int main(int argument_count, char **arguments) {
    if (argument_count != 3) {
        report("byte-oracle: expected PATH HEX-BYTES\n");
        return 2;
    }

    Size expected_byte_count = 0U;
    if (!has_valid_hexadecimal_length(arguments[2], expected_byte_count)) {
        report("byte-oracle: invalid hexadecimal byte string\n");
        return 2;
    }

    const int input_descriptor = open(arguments[1], O_RDONLY);
    if (input_descriptor < 0) {
        report("byte-oracle: cannot open input\n");
        return 1;
    }

    unsigned char buffer[128];
    Size observed_byte_count = 0U;
    bool matches = true;
    while (matches) {
        const auto bytes_read = read(input_descriptor, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            report("byte-oracle: read failed\n");
            matches = false;
            break;
        }
        if (bytes_read == 0)
            break;

        const auto chunk_size = static_cast<Size>(bytes_read);
        for (Size chunk_index = 0U; chunk_index < chunk_size; ++chunk_index) {
            if (observed_byte_count >= expected_byte_count) {
                matches = false;
                break;
            }
            unsigned char expected_byte = 0U;
            if (!decode_expected_byte(arguments[2], observed_byte_count, expected_byte) ||
                buffer[chunk_index] != expected_byte) {
                matches = false;
                break;
            }
            ++observed_byte_count;
        }
    }

    if (close(input_descriptor) != 0) {
        report("byte-oracle: close failed\n");
        return 1;
    }
    if (!matches || observed_byte_count != expected_byte_count) {
        report("byte-oracle: byte mismatch\n");
        return 1;
    }
    return 0;
}
