#include "userland/bin/printf_core.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

    using xinim::userland::posix_printf::FormatResult;
    using xinim::userland::posix_printf::OutputSink;
    using xinim::userland::posix_printf::Size;

    struct Capture {
        std::string bytes;
        bool accept_writes{true};
    };

    bool append_capture(void *context, const char *bytes, Size byte_count) noexcept {
        auto &capture = *static_cast<Capture *>(context);
        if (!capture.accept_writes)
            return false;
        capture.bytes.append(bytes, byte_count);
        return true;
    }

    struct RunResult {
        std::string standard_output;
        std::string standard_error;
        FormatResult format_result;
    };

    RunResult run_printf(std::string_view format, const std::vector<std::string> &arguments) {
        Capture standard_output{};
        Capture standard_error{};
        std::vector<char *> argument_pointers;
        argument_pointers.reserve(arguments.size());
        for (const auto &argument : arguments) {
            argument_pointers.push_back(const_cast<char *>(argument.c_str()));
        }

        char *format_pointer = const_cast<char *>(format.data());
        const OutputSink output_sink{&standard_output, append_capture};
        const OutputSink error_sink{&standard_error, append_capture};
        const auto result = xinim::userland::posix_printf::format(
            output_sink, error_sink, format_pointer, static_cast<int>(argument_pointers.size()),
            argument_pointers.data());
        return RunResult{standard_output.bytes, standard_error.bytes, result};
    }

    bool expect_equal(std::string_view name, std::string_view actual, std::string_view expected) {
        if (actual == expected)
            return true;
        std::cerr << "FAIL: " << name << "\n  actual:   " << actual << "\n  expected: " << expected
                  << '\n';
        return false;
    }

    bool expect_true(std::string_view name, bool condition) {
        if (condition)
            return true;
        std::cerr << "FAIL: " << name << '\n';
        return false;
    }

} // namespace

int main() {
    int failures = 0;

    {
        const auto result = run_printf("%5d%4d\n", {"1", "21", "321", "4321", "54321"});
        failures += !expect_equal("format reuse with partial final cycle", result.standard_output,
                                  "    1  21\n  3214321\n54321   0\n");
        failures += !expect_true("format reuse status", result.format_result.exit_status == 0);
    }

    {
        const auto result = run_printf("%s|%c|%d|%u|%o|%x\n", {});
        failures += !expect_equal("missing operands use specified defaults", result.standard_output,
                                  "||0|0|0|0\n");
    }

    {
        const auto result = run_printf("[%+05d][%-5s][%.3s][%#x][%#o][%.0d]\n",
                                       {"42", "hi", "abcde", "16", "8", "0"});
        failures += !expect_equal("flags widths and precisions", result.standard_output,
                                  "[+0042][hi   ][abc][0x10][010][]\n");
    }

    {
        const auto result = run_printf("%d %d %d\n", {"010", "0x10", "'A"});
        failures +=
            !expect_equal("C integer constant extensions", result.standard_output, "8 16 65\n");
    }

    {
        const auto result = run_printf("%d:%d", {"12x", "7"});
        failures += !expect_equal("conversion error retains accumulated value",
                                  result.standard_output, "12:7");
        failures +=
            !expect_true("conversion error diagnostic",
                         result.standard_error.find("invalid integer: 12x") != std::string::npos);
        failures += !expect_true("conversion error status", result.format_result.exit_status != 0);
    }

    {
        const auto result = run_printf("[%b]tail", {"a\\nb"});
        failures +=
            !expect_equal("percent b escape conversion", result.standard_output, "[a\nb]tail");
    }

    {
        const auto result = run_printf("A%bZ%s", {"left\\cright", "ignored"});
        failures += !expect_equal("percent b c escape terminates all output",
                                  result.standard_output, "Aleft");
        failures +=
            !expect_true("percent b c stop state", result.format_result.stopped_by_b_escape);
    }

    {
        const auto result = run_printf("[%6.3b]", {"a\\tbcd"});
        failures +=
            !expect_equal("percent b precision and width", result.standard_output, "[   a\tb]");
    }

    {
        const auto result = run_printf("x\\141\\n", {});
        failures += !expect_equal("format octal escape consumes three digits",
                                  result.standard_output, "xa\n");
    }

    {
        const auto result = run_printf("%s,", {"a", "b", "c"});
        failures +=
            !expect_equal("single conversion format cycles", result.standard_output, "a,b,c,");
    }

    {
        const auto result = run_printf("[%#08X][%#.0o]", {"42", "0"});
        failures += !expect_equal("base prefixes and zero padding", result.standard_output,
                                  "[0X00002A][0]");
    }

    {
        const auto result = run_printf("[% +d][%0-5d][%05.3d][%.3d]", {"42", "42", "42", "-7"});
        failures +=
            !expect_equal("flag precedence", result.standard_output, "[+42][42   ][  042][-007]");
    }

    {
        const auto result = run_printf("[%5s][%-5s][%2s][%5.3s]", {"xy", "xy", "wide", "abcdef"});
        failures += !expect_equal("string width never truncates", result.standard_output,
                                  "[   xy][xy   ][wide][  abc]");
    }

    {
        const auto result = run_printf("[%d][%i][%u][%o][%x][%X][%c][%s][%%]",
                                       {"-2", "+3", "4", "8", "31", "31", "AB", "text"});
        failures += !expect_equal("mandatory conversion set", result.standard_output,
                                  "[-2][3][4][10][1f][1F][A][text][%]");
    }

    {
        const auto result = run_printf("[%d][%u][%o]", {"1", "1", "1"});
        failures += !expect_equal("bare numeric conversions add no padding", result.standard_output,
                                  "[1][1][1]");
    }

    {
        const std::string escaped_argument = R"(\\\a\b\f\n\r\t\v\0\07\012\0101Z)";
        const std::string expected{"\\\a\b\f\n\r\t\v\0\7\nAZ", 13U};
        const auto result = run_printf("%b", {escaped_argument});
        failures +=
            !expect_equal("percent b complete escape set", result.standard_output, expected);
    }

    {
        const auto result = run_printf("[%-5.2b]", {R"(A\011B)"});
        failures += !expect_equal("percent b left width after converted precision",
                                  result.standard_output, "[A\t   ]");
    }

    {
        const auto result = run_printf("<%d,%d>\n", {"12x", "7", "0xG", "9", "10"});
        failures += !expect_equal("numeric errors continue across format reuse",
                                  result.standard_output, "<12,7>\n<0,9>\n<10,0>\n");
        failures +=
            !expect_true("numeric error reuse status", result.format_result.exit_status != 0);
    }

    {
        const auto result = run_printf("%%", {"unused"});
        failures +=
            !expect_equal("non-consuming format does not loop", result.standard_output, "%");
    }

    {
        const auto result = run_printf("[%c]", {"xyz"});
        failures +=
            !expect_equal("character conversion uses first byte", result.standard_output, "[x]");
    }

    {
        Capture rejected_output{.bytes = {}, .accept_writes = false};
        Capture standard_error{};
        const OutputSink output_sink{&rejected_output, append_capture};
        const OutputSink error_sink{&standard_error, append_capture};
        char format[] = "output";
        const auto result =
            xinim::userland::posix_printf::format(output_sink, error_sink, format, 0, nullptr);
        failures += !expect_true("output failure status", result.exit_status != 0);
        failures +=
            !expect_true("output failure is not percent b stop", !result.stopped_by_b_escape);
    }

    return failures == 0 ? 0 : 1;
}
