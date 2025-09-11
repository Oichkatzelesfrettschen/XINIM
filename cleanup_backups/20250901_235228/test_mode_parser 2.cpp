// commands/tests/test_mode_parser.cpp
#include <iostream>
#include <filesystem>
#include <expected>
#include <string_view>
#include <vector>
#include <string>       // For std::string in ParseResult
#include <algorithm>    // For std::ranges::all_of (used by ModeParser, indirectly)
#include <ranges>       // For std::ranges::all_of (C++20)
#include <charconv>     // For std::from_chars
#include <cstdlib>      // For EXIT_SUCCESS, EXIT_FAILURE
#include <print>        // For std::println (C++23)
#include <optional>     // For std::optional

#include "xinim/mode_parser.hpp" // The externalized ModeParser

// --- ModeParser Definition is now removed from here ---


struct TestCase {
    std::string name;
    std::string_view mode_str;
    std::filesystem::perms current_perms; // For symbolic modes
    std::filesystem::perms expected_perms;
    bool expect_success;
    std::string expected_error_msg_part; // For error cases

    void run(const xinim::util::ModeParser& parser, int& failures) const { // Use namespaced parser
        std::print(std::cout, "Test Case: {} ('{}')... ", name, mode_str);
        xinim::util::ModeParser::ParseResult result = parser.parse(mode_str, current_perms); // Use namespaced parser

        if (result.has_value()) {
            if (expect_success) {
                if (result.value() == expected_perms) {
                    std::println(std::cout, "PASS");
                } else {
                    std::println(std::cout, "FAIL");
                    std::println(std::cerr, "  Path: {} Mode: '{}'", name, mode_str);
                    std::println(std::cerr, "  Expected: 0{:o}, Got: 0{:o}",
                               static_cast<unsigned int>(expected_perms),
                               static_cast<unsigned int>(result.value()));
                    failures++;
                }
            } else {
                std::println(std::cout, "FAIL (expected error, got success: 0{:o})", static_cast<unsigned int>(result.value()));
                std::println(std::cerr, "  Path: {} Mode: '{}'", name, mode_str);
                failures++;
            }
        } else { // Error case
            if (expect_success) {
                std::println(std::cout, "FAIL (expected success, got error: {})", result.error());
                std::println(std::cerr, "  Path: {} Mode: '{}'", name, mode_str);
                failures++;
            } else {
                if (expected_error_msg_part.empty() || result.error().find(expected_error_msg_part) != std::string::npos) {
                    std::println(std::cout, "PASS (got expected error: {})", result.error());
                } else {
                    std::println(std::cout, "FAIL");
                    std::println(std::cerr, "  Path: {} Mode: '{}'", name, mode_str);
                    std::println(std::cerr, "  Expected error containing: '{}', Got: '{}'", expected_error_msg_part, result.error());
                    failures++;
                }
            }
        }
    }
};

// Helper to create perms from octal for expected values easily
std::filesystem::perms p(unsigned int octal_val) {
    return static_cast<std::filesystem::perms>(octal_val);
}


int main() {
    xinim::util::ModeParser parser; // Use namespaced parser
    int failures = 0;

    std::vector<TestCase> test_cases = {
        // Octal tests
        {"Octal_755", "755", std::filesystem::perms::none, p(0755), true, ""},
        {"Octal_0644", "0644", std::filesystem::perms::none, p(0644), true, ""},
        {"Octal_0", "0", std::filesystem::perms::none, p(00), true, ""},
        {"Octal_4755_setuid", "4755", std::filesystem::perms::none, p(04755), true, ""},
        {"Octal_2755_setgid", "2755", std::filesystem::perms::none, p(02755), true, ""},
        {"Octal_1755_sticky", "1755", std::filesystem::perms::none, p(01755), true, ""},
        {"Octal_7777_all_special", "7777", std::filesystem::perms::none, p(07777), true, ""},
        {"Octal_Invalid_8", "8", std::filesystem::perms::none, std::filesystem::perms::none, false, "Invalid octal mode"}, // Error message from ModeParser
        {"Octal_Invalid_abc", "abc", std::filesystem::perms::none, std::filesystem::perms::none, false, "Symbolic mode clause"}, // Error from symbolic part
        {"Octal_Too_Large", "17777", std::filesystem::perms::none, std::filesystem::perms::none, false, "Octal mode value too large"},
        {"Octal_Empty", "", std::filesystem::perms::none, std::filesystem::perms::none, false, "Empty mode string"},

        // Symbolic tests (current_perms, expected_perms)
        {"Symbolic_u+x", "u+x", p(0644), p(0744), true, ""},
        {"Symbolic_g-w", "g-w", p(0664), p(0644), true, ""},
        {"Symbolic_o=r", "o=r", p(0666), p(0664), true, ""},
        {"Symbolic_a+r", "a+r", p(0222), p(0666), true, ""},
        {"Symbolic_ug+x", "ug+x", p(0600), p(0750), true, ""},
        {"Symbolic_go-rwx", "go-rwx", p(0777), p(0700), true, ""},
        {"Symbolic_u=rwx,g=rx,o=", "u=rwx,g=rx,o=", p(0000), p(0750), true, ""},
        {"Symbolic_u+s_setuid", "u+s", p(0755), p(04755), true, ""},
        {"Symbolic_g+s_setgid", "g+s", p(0755), p(02755), true, ""},
        {"Symbolic_a+t_sticky", "a+t", p(0755), p(01755), true, ""},
        {"Symbolic_o+t_sticky_only_o", "o+t", p(0755), p(01755), true, ""},
        {"Symbolic_ug=r,o=---", "ug=r,o=", p(0777), p(0440), true, ""},
        // Implicit 'a' for who when not specified, assuming umask doesn't strip bits here (parser doesn't know umask)
        {"Symbolic_implicit_a_plus_x", "+x", p(0644), p(0755), true, ""},
        {"Symbolic_implicit_a_eq_r", "=r", p(0777), p(0444), true, ""},

        // Symbolic error cases
        {"Symbolic_Invalid_Op_char", "u?x", p(0644), std::filesystem::perms::none, false, "Invalid operator"},
        {"Symbolic_Invalid_Who_char", "z+x", p(0644), std::filesystem::perms::none, false, "Invalid 'who' character"},
        {"Symbolic_Invalid_What_char", "u+z", p(0644), std::filesystem::perms::none, false, "Invalid permission character"},
        {"Symbolic_Empty_Clause_Start", ",u+x", p(0644), std::filesystem::perms::none, false, "Empty clause"},
        {"Symbolic_Empty_Clause_Mid", "u+x,,g+w", p(0644), std::filesystem::perms::none, false, "Empty clause"},
        {"Symbolic_Trailing_Comma_Accepted", "u+x,", p(0644), p(0744), true, ""},
        {"Symbolic_Equals_Only", "u=", p(0777), p(0077), true, ""},
        {"Symbolic_Plus_Only_NoPerms", "u+", p(0600), std::filesystem::perms::none, false, "Missing permissions"},
        {"Symbolic_Minus_Only_NoPerms", "u-", p(0600), std::filesystem::perms::none, false, "Missing permissions"},
        {"Symbolic_Complex_1", "u=rw,g=r,o=,ug+x,o+r", p(0000), p(0754), true, ""},
    };

    for (const auto& tc : test_cases) {
        tc.run(parser, failures);
    }

    if (failures > 0) {
        std::println(std::cerr, "\n{} MODE PARSER TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL MODE PARSER TESTS PASSED.");
    return EXIT_SUCCESS;
}
