#include "xinim/filesystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

class temp_root {
public:
    explicit temp_root(const std::string& prefix) {
        static int counter = 0;
        path_ = fs::temp_directory_path() / (prefix + "_" + std::to_string(counter++));
        std::error_code ec;
        fs::create_directory(path_, ec);
        if (ec) {
            std::println(std::cerr, "FATAL: failed to create {}: {}", path_.string(), ec.message());
            std::exit(EXIT_FAILURE);
        }
    }

    ~temp_root() {
        std::error_code ec;
        fs::remove_all(path_, ec);
        if (ec) {
            std::println(std::cerr, "Warning: failed to remove {}: {}", path_.string(), ec.message());
        }
    }

    temp_root(const temp_root&) = delete;
    temp_root& operator=(const temp_root&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_{};
};

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

struct test_case {
    std::string name;
    xinim::fs::mode mode;
    std::filesystem::copy_options copy_options;
    bool create_source_file{true};
    bool create_destination_file{false};
    bool create_destination_dir{false};
    bool create_missing_parent{false};
    bool create_source_dir{false};
    std::string source_content{"source"};
    std::string destination_content{"dest"};
    bool expect_success{true};
    std::errc expected_error{};
};

bool run_case(const test_case& test, int& failures) {
    std::print(std::cout, "Test Case: {}... ", test.name);
    temp_root root("xinim_copy_file");

    const fs::path source = root.path() / "source.txt";
    fs::path destination = root.path() / "dest.txt";
    if (test.create_missing_parent) {
        destination = root.path() / "missing" / "dest.txt";
    }

    if (test.create_source_dir) {
        fs::create_directory(source);
    } else if (test.create_source_file) {
        write_file(source, test.source_content);
    }

    if (test.create_destination_dir) {
        fs::create_directory(destination);
    } else if (test.create_destination_file) {
        write_file(destination, test.destination_content);
    }

    const xinim::fs::operation_context ctx{test.mode, false, true};
    const auto result = xinim::fs::copy_file(source, destination, test.copy_options, ctx);

    if (!test.expect_success) {
        if (result.has_value()) {
            std::println(std::cout, "FAIL (expected error, got success)");
            ++failures;
            return false;
        }
        if (result.error() != test.expected_error) {
            std::println(std::cout,
                         "FAIL (expected {}, got {})",
                         std::make_error_code(test.expected_error).message(),
                         result.error().message());
            ++failures;
            return false;
        }
        std::println(std::cout, "PASS");
        return true;
    }

    if (!result.has_value()) {
        std::println(std::cout, "FAIL ({})", result.error().message());
        ++failures;
        return false;
    }
    if (!fs::is_regular_file(destination)) {
        std::println(std::cout, "FAIL (destination missing)");
        ++failures;
        return false;
    }

    const std::string expected_content =
        (test.create_destination_file &&
         (test.copy_options & fs::copy_options::skip_existing) != fs::copy_options::none)
            ? test.destination_content
            : test.source_content;

    if (read_file(destination) != expected_content) {
        std::println(std::cout, "FAIL (content mismatch)");
        ++failures;
        return false;
    }

    std::println(std::cout, "PASS");
    return true;
}

} // namespace

int main() {
    int failures = 0;

    const std::vector<test_case> tests = {
        {"copy_new_standard", xinim::fs::mode::standard, fs::copy_options::none, true, false, false, false, false, "Hello World", "", true, {}},
        {"copy_new_direct", xinim::fs::mode::direct, fs::copy_options::none, true, false, false, false, false, "Direct Copy", "", true, {}},
        {"copy_overwrite_standard", xinim::fs::mode::standard, fs::copy_options::overwrite_existing, true, true, false, false, false, "Overwrite Me", "Old Data", true, {}},
        {"copy_skip_existing_standard", xinim::fs::mode::standard, fs::copy_options::skip_existing, true, true, false, false, false, "New Data", "Keep Me", true, {}},
        {"copy_fail_if_exists_standard", xinim::fs::mode::standard, fs::copy_options::none, true, true, false, false, false, "Source", "Existing", false, std::errc::file_exists},
        {"copy_source_dir_fails", xinim::fs::mode::standard, fs::copy_options::none, false, false, false, false, true, "", "", false, std::errc::is_a_directory},
        {"copy_missing_parent_fails", xinim::fs::mode::standard, fs::copy_options::none, true, false, false, true, false, "Source", "", false, std::errc::no_such_file_or_directory},
        {"copy_missing_source_fails", xinim::fs::mode::standard, fs::copy_options::none, false, false, false, false, false, "", "", false, std::errc::no_such_file_or_directory},
    };

    for (const auto& test : tests) {
        run_case(test, failures);
    }

    if (failures != 0) {
        std::println(std::cerr, "\n{} XINIM::FS::COPY_FILE TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::COPY_FILE TESTS PASSED.");
    return EXIT_SUCCESS;
}
