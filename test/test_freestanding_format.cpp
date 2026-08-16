#include "freestanding_format.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace {

    int failures = 0;

    void check(bool condition, const char *expression, int line) {
        if (!condition) {
            std::fprintf(stderr, "FAIL:%d: %s\n", line, expression);
            ++failures;
        }
    }

#define CHECK(expression) check((expression), #expression, __LINE__)

    int format(char *buffer, std::size_t capacity, const char *format_string, ...) {
        va_list arguments;
        va_start(arguments, format_string);
        const int result =
            xinim::kernel::format::to_buffer(buffer, capacity, format_string, arguments);
        va_end(arguments);
        return result;
    }

    void test_required_integer_and_string_subset() {
        char buffer[128]{};
        const int result =
            format(buffer, sizeof(buffer), "%s %d %u %x %X %o %c %% %zu %ld %lld", "value", -7, 9U,
                   0x2aU, 0x2aU, 8U, 'Z', static_cast<std::size_t>(12U), 13L, 14LL);
        CHECK(result == 32);
        CHECK(std::strcmp(buffer, "value -7 9 2a 2A 10 Z % 12 13 14") == 0);

        using SignedSize = std::make_signed_t<std::size_t>;
        CHECK(format(buffer, sizeof(buffer), "%zd", static_cast<SignedSize>(-3)) == 2);
        CHECK(std::strcmp(buffer, "-3") == 0);
    }

    void test_width_padding_and_truncation() {
        char buffer[32]{};
        CHECK(format(buffer, sizeof(buffer), "%5d|%-5s|%05d", 42, "x", -7) == 17);
        CHECK(std::strcmp(buffer, "   42|x    |-0007") == 0);

        CHECK(format(buffer, sizeof(buffer), "%5c|%-5c", 'A', 'B') == 11);
        CHECK(std::strcmp(buffer, "    A|B    ") == 0);

        char short_buffer[5]{};
        CHECK(format(short_buffer, sizeof(short_buffer), "abcdef") == 6);
        CHECK(std::strcmp(short_buffer, "abcd") == 0);

        CHECK(format(nullptr, 0U, "abc%d", 7) == 4);
        CHECK(format(nullptr, 1U, "abc") == -1);
    }

    void test_overflow_and_unsupported_formats_fail() {
        char buffer[8]{};
        CHECK(format(buffer, sizeof(buffer), "%999999999999999999999d", 1) == -1);
        CHECK(format(buffer, sizeof(buffer), "%2147483647dX", 1) == -1);
        CHECK(format(buffer, sizeof(buffer), "%.2f", 1.0) == -1);
        CHECK(format(buffer, sizeof(buffer), "%*d", 4, 1) == -1);
        CHECK(format(buffer, sizeof(buffer), "%ls", "value") == -1);
        CHECK(format(buffer, sizeof(buffer), "%lp", nullptr) == -1);
        CHECK(format(buffer, sizeof(buffer), "%lc", 'A') == -1);
        CHECK(format(buffer, sizeof(buffer), "%zs", "value") == -1);
        CHECK(format(buffer, sizeof(buffer), "%0s", "value") == -1);
        CHECK(format(buffer, sizeof(buffer), "%0c", 'A') == -1);
        CHECK(format(buffer, sizeof(buffer), "%-0s", "value") == -1);
        CHECK(format(buffer, sizeof(buffer), "%0-s", "value") == -1);
        CHECK(format(buffer, sizeof(buffer), "%-0c", 'A') == -1);
        CHECK(format(buffer, sizeof(buffer), "%0-c", 'A') == -1);
        CHECK(format(buffer, sizeof(buffer), "%5%") == -1);
        CHECK(format(buffer, sizeof(buffer), "%-5%") == -1);
    }

} // namespace

int main() {
    test_required_integer_and_string_subset();
    test_width_padding_and_truncation();
    test_overflow_and_unsupported_formats_fail();
    return failures == 0 ? 0 : 1;
}
