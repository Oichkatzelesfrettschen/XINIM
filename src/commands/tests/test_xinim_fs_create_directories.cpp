#include "xinim/filesystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>

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

std::optional<mode_t> current_mode(const fs::path& path) {
    struct stat stat_buffer {};
    if (::stat(path.c_str(), &stat_buffer) != 0) {
        return std::nullopt;
    }
    return stat_buffer.st_mode;
}

mode_t expected_mode(fs::perms perms) {
    mode_t mode = 0;
    if ((perms & fs::perms::owner_read) != fs::perms::none) mode |= S_IRUSR;
    if ((perms & fs::perms::owner_write) != fs::perms::none) mode |= S_IWUSR;
    if ((perms & fs::perms::owner_exec) != fs::perms::none) mode |= S_IXUSR;
    if ((perms & fs::perms::group_read) != fs::perms::none) mode |= S_IRGRP;
    if ((perms & fs::perms::group_write) != fs::perms::none) mode |= S_IWGRP;
    if ((perms & fs::perms::group_exec) != fs::perms::none) mode |= S_IXGRP;
    if ((perms & fs::perms::others_read) != fs::perms::none) mode |= S_IROTH;
    if ((perms & fs::perms::others_write) != fs::perms::none) mode |= S_IWOTH;
    if ((perms & fs::perms::others_exec) != fs::perms::none) mode |= S_IXOTH;
    if ((perms & fs::perms::set_uid) != fs::perms::none) mode |= S_ISUID;
    if ((perms & fs::perms::set_gid) != fs::perms::none) mode |= S_ISGID;
    if ((perms & fs::perms::sticky_bit) != fs::perms::none) mode |= S_ISVTX;
    return mode;
}

bool verify_permissions(const fs::path& path, fs::perms perms) {
    const auto actual = current_mode(path);
    return actual.has_value() && ((*actual & 07777) == expected_mode(perms));
}

struct test_case {
    std::string name;
    fs::path relative_path;
    fs::perms perms;
    xinim::fs::mode mode;
    bool expect_success;
    std::optional<std::errc> expected_error;
    bool create_blocking_file{false};
};

bool run_case(const test_case& test, int& failures) {
    std::print(std::cout, "Test Case: {}... ", test.name);
    temp_root root("xinim_create_dirs");
    const fs::path full_path = root.path() / test.relative_path;

    if (test.create_blocking_file) {
        const fs::path blocking_parent = full_path.parent_path();
        fs::create_directories(blocking_parent.parent_path());
        std::ofstream out(blocking_parent);
        out << "block";
        out.close();
    }

    const xinim::fs::operation_context ctx{test.mode, false, true};
    const auto result = xinim::fs::create_directories(full_path, test.perms, ctx);

    if (!test.expect_success) {
        if (result.has_value()) {
            std::println(std::cout, "FAIL (expected error, got success)");
            ++failures;
            return false;
        }
        if (test.expected_error.has_value() && result.error() != *test.expected_error) {
            std::println(std::cout,
                         "FAIL (expected {}, got {})",
                         std::make_error_code(*test.expected_error).message(),
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
    if (!fs::is_directory(full_path)) {
        std::println(std::cout, "FAIL (path not created)");
        ++failures;
        return false;
    }
    if (!verify_permissions(full_path, test.perms)) {
        std::println(std::cout, "FAIL (permissions mismatch)");
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
        {"new_path_standard_755", "a/b/c", static_cast<fs::perms>(0755), xinim::fs::mode::standard, true, {}},
        {"new_path_direct_700", "d/e/f", static_cast<fs::perms>(0700), xinim::fs::mode::direct, true, {}},
        {"existing_path_standard", "g/h/i", static_cast<fs::perms>(0777), xinim::fs::mode::standard, true, {}},
        {"blocking_file_standard", "j/block/newdir", static_cast<fs::perms>(0755), xinim::fs::mode::standard, false, std::errc::not_a_directory, true},
    };

    for (const auto& test : tests) {
        if (test.name == "existing_path_standard") {
            temp_root root("xinim_create_dirs_existing");
            const fs::path full_path = root.path() / test.relative_path;
            fs::create_directories(full_path);
            const xinim::fs::operation_context ctx{test.mode, false, true};
            std::print(std::cout, "Test Case: {}... ", test.name);
            const auto result = xinim::fs::create_directories(full_path, test.perms, ctx);
            if (!result.has_value() || !fs::is_directory(full_path)) {
                std::println(std::cout, "FAIL");
                ++failures;
            } else {
                std::println(std::cout, "PASS");
            }
            continue;
        }
        run_case(test, failures);
    }

    if (failures != 0) {
        std::println(std::cerr, "\n{} XINIM::FS::CREATE_DIRECTORIES TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::CREATE_DIRECTORIES TESTS PASSED.");
    return EXIT_SUCCESS;
}
