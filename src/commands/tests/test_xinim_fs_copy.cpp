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
    explicit temp_root(const std::string &prefix) {
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

    temp_root(const temp_root &) = delete;
    temp_root &operator=(const temp_root &) = delete;

    [[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
    fs::path path_{};
};

void write_file(const fs::path &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string read_file(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

struct test_case {
    std::string name;
    xinim::fs::mode mode;
    fs::copy_options copy_options;
    enum class source_kind { regular_file, directory, symlink, missing };
    source_kind source_type{source_kind::regular_file};
    bool create_destination_file{false};
    bool create_destination_dir{false};
    bool destination_in_missing_parent{false};
    std::string source_content{"source"};
    std::string destination_content{"dest"};
    bool expect_success{true};
    std::errc expected_error{};
};

bool run_case(const test_case &test, int &failures) {
    std::print(std::cout, "Test Case: {}... ", test.name);
    temp_root root("xinim_copy");

    const fs::path source = root.path() / "source";
    fs::path destination = root.path() / "dest";
    if (test.destination_in_missing_parent) {
        destination = root.path() / "missing" / "dest";
    }

    switch (test.source_type) {
    case test_case::source_kind::regular_file:
        write_file(source, test.source_content);
        break;
    case test_case::source_kind::directory:
        fs::create_directory(source);
        write_file(source / "child.txt", test.source_content);
        break;
    case test_case::source_kind::symlink:
        write_file(root.path() / "target.txt", test.source_content);
        fs::create_symlink(root.path() / "target.txt", source);
        break;
    case test_case::source_kind::missing:
        break;
    }

    if (test.create_destination_file) {
        write_file(destination, test.destination_content);
    } else if (test.create_destination_dir) {
        fs::create_directory(destination);
    }

    const xinim::fs::operation_context ctx{test.mode, false, true};
    const auto result = xinim::fs::copy(source, destination, test.copy_options, ctx);

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

    if (test.source_type == test_case::source_kind::regular_file) {
        if (!fs::is_regular_file(destination)) {
            std::println(std::cout, "FAIL (destination file missing)");
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
    } else if (test.source_type == test_case::source_kind::directory) {
        if (!fs::is_directory(destination)) {
            std::println(std::cout, "FAIL (destination directory missing)");
            ++failures;
            return false;
        }
        const bool recursive =
            (test.copy_options & fs::copy_options::recursive) != fs::copy_options::none;
        const fs::path child = destination / "child.txt";
        if (recursive && (!fs::is_regular_file(child) || read_file(child) != test.source_content)) {
            std::println(std::cout, "FAIL (recursive directory copy mismatch)");
            ++failures;
            return false;
        }
    } else if (test.source_type == test_case::source_kind::symlink &&
               (test.copy_options & fs::copy_options::copy_symlinks) != fs::copy_options::none) {
        if (!fs::is_symlink(destination)) {
            std::println(std::cout, "FAIL (symlink not preserved)");
            ++failures;
            return false;
        }
    }

    std::println(std::cout, "PASS");
    return true;
}

} // namespace

int main() {
    int failures = 0;

    const std::vector<test_case> tests = {
        {"copy_file_standard", xinim::fs::mode::standard, fs::copy_options::none, test_case::source_kind::regular_file, false, false, false, "alpha", "", true, {}},
        {"copy_file_direct", xinim::fs::mode::direct, fs::copy_options::none, test_case::source_kind::regular_file, false, false, false, "beta", "", true, {}},
        {"copy_file_overwrite", xinim::fs::mode::standard, fs::copy_options::overwrite_existing, test_case::source_kind::regular_file, true, false, false, "new", "old", true, {}},
        {"copy_file_skip_existing", xinim::fs::mode::standard, fs::copy_options::skip_existing, test_case::source_kind::regular_file, true, false, false, "newer", "keep", true, {}},
        {"copy_directory_recursive", xinim::fs::mode::standard, fs::copy_options::recursive, test_case::source_kind::directory, false, false, false, "nested", "", true, {}},
        {"copy_symlink_as_link", xinim::fs::mode::standard, fs::copy_options::copy_symlinks, test_case::source_kind::symlink, false, false, false, "linked", "", true, {}},
        {"copy_missing_source_fails", xinim::fs::mode::standard, fs::copy_options::none, test_case::source_kind::missing, false, false, false, "", "", false, std::errc::no_such_file_or_directory},
        {"copy_directory_nonrecursive_creates_shell", xinim::fs::mode::standard, fs::copy_options::none, test_case::source_kind::directory, false, false, false, "nested", "", true, {}},
        {"copy_missing_parent_fails", xinim::fs::mode::standard, fs::copy_options::none, test_case::source_kind::regular_file, false, false, true, "orphan", "", false, std::errc::no_such_file_or_directory},
    };

    for (const auto &test : tests) {
        run_case(test, failures);
    }

    if (failures != 0) {
        std::println(std::cerr, "\n{} XINIM::FS::COPY TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::COPY TESTS PASSED.");
    return EXIT_SUCCESS;
}
