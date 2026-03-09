#include "xinim/filesystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

class temp_entity {
public:
    enum class kind {
        file,
        directory,
        symlink,
    };

    explicit temp_entity(const std::string& prefix,
                         kind entity_kind,
                         const fs::path& symlink_target = {})
        : kind_(entity_kind) {
        static int counter = 0;
        path_ = fs::temp_directory_path() / (prefix + "_" + std::to_string(counter++));
        std::error_code ec;

        switch (kind_) {
        case kind::file: {
            std::ofstream out(path_);
            out << "chown test";
            out.close();
            break;
        }
        case kind::directory:
            fs::create_directory(path_, ec);
            break;
        case kind::symlink:
            fs::create_symlink(symlink_target, path_, ec);
            break;
        }

        if (ec) {
            std::println(std::cerr, "FATAL: failed to create {}: {}", path_.string(), ec.message());
            std::exit(EXIT_FAILURE);
        }
    }

    ~temp_entity() {
        std::error_code ec;
        if (fs::is_symlink(fs::symlink_status(path_, ec))) {
            ec.clear();
            fs::remove(path_, ec);
        } else {
            ec.clear();
            fs::remove_all(path_, ec);
        }
        if (ec) {
            std::println(std::cerr, "Warning: failed to remove {}: {}", path_.string(), ec.message());
        }
    }

    temp_entity(const temp_entity&) = delete;
    temp_entity& operator=(const temp_entity&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_{};
    kind kind_{kind::file};
};

struct test_case {
    std::string name;
    xinim::fs::operation_context ctx;
    temp_entity::kind entity_kind;
    bool expect_success;
    std::errc expected_error{};
    bool missing_path{false};
    bool needs_symlink_target{false};
};

bool run_case(const test_case& test, uid_t uid, gid_t gid, int& failures) {
    std::print(std::cout, "Test Case: {}... ", test.name);

    temp_entity symlink_target("xinim_chown_target", temp_entity::kind::file);
    fs::path path_under_test = fs::temp_directory_path() / (test.name + "_missing");
    std::optional<temp_entity> entity;

    if (!test.missing_path) {
        if (test.needs_symlink_target) {
            entity.emplace(test.name, test.entity_kind, symlink_target.path());
        } else {
            entity.emplace(test.name, test.entity_kind);
        }
        path_under_test = entity->path();
    }

    const auto result = xinim::fs::change_ownership(path_under_test, uid, gid, test.ctx);

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

    const auto verify = xinim::fs::get_status(path_under_test, test.ctx);
    if (!verify.has_value()) {
        std::println(std::cout, "FAIL (verify status failed: {})", verify.error().message());
        ++failures;
        return false;
    }
    if (verify->uid != uid || verify->gid != gid) {
        std::println(std::cout, "FAIL (ownership mismatch)");
        ++failures;
        return false;
    }

    std::println(std::cout, "PASS");
    return true;
}

} // namespace

int main() {
    int failures = 0;
    const uid_t uid = ::getuid();
    const gid_t gid = ::getgid();

    const std::vector<test_case> tests = {
        {"std_mode_file_fails", {xinim::fs::mode::standard, false, true}, temp_entity::kind::file, false, std::errc::operation_not_supported},
        {"direct_mode_file_current_owner", {xinim::fs::mode::direct, false, true}, temp_entity::kind::file, true, {}},
        {"direct_mode_dir_current_owner", {xinim::fs::mode::direct, false, true}, temp_entity::kind::directory, true, {}},
        {"direct_mode_missing_path", {xinim::fs::mode::direct, false, true}, temp_entity::kind::file, false, std::errc::no_such_file_or_directory, true},
        {"direct_mode_symlink_follow", {xinim::fs::mode::direct, false, true}, temp_entity::kind::symlink, true, {}, false, true},
        {"direct_mode_symlink_nofollow", {xinim::fs::mode::direct, false, false}, temp_entity::kind::symlink, true, {}, false, true},
    };

    for (const auto& test : tests) {
        run_case(test, uid, gid, failures);
    }

    if (failures != 0) {
        std::println(std::cerr, "\n{} XINIM::FS::CHANGE_OWNERSHIP TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::CHANGE_OWNERSHIP TESTS PASSED.");
    return EXIT_SUCCESS;
}
