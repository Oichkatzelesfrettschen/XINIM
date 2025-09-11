// commands/tests/test_xinim_fs_create_directory.cpp
#include "xinim/filesystem.hpp" // For xinim::fs free functions and operation_context
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>      // For EXIT_SUCCESS, EXIT_FAILURE, rand, srand
#include <ctime>        // For time (for srand)
#include <system_error> // For std::error_code constants, std::errc
#include <fstream>      // For std::ofstream (to create a dummy file)
#include <cassert>      // For assert
#include <format>      // For std::format (C++20/23)
#include <iostream>    // For std::cout

// Helper to manage a temporary test directory
// Note: TempTestDir constructor creates the base directory 'path'.
// Tests for create_directory will create subdirectories *within* this 'path'.
class TempTestDir {
public:
    std::filesystem::path path;
    std::string id_str;

    TempTestDir(const std::string& base_name = "test_dir_base") {
        static int counter = 0;
        // time(nullptr) might not be granular enough for rapid test runs.
        // Using a simple counter and process ID (if available and useful) might be better for uniqueness.
        // For now, time + counter should be okay for sequential tests.
        id_str = base_name + "_" + std::to_string(time(nullptr) % 100000) + "_" + std::to_string(counter++);
        path = std::filesystem::temp_directory_path() / id_str;

        std::error_code ec_create;
        std::filesystem::create_directory(path, ec_create);
        if (ec_create) {
            std::println(std::cerr, "FATAL: Failed to create temporary base directory {}: {}", path.string(), ec_create.message());
            std::exit(EXIT_FAILURE);
        }
    }

    ~TempTestDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        if (ec) {
            std::println(std::cerr, "Warning: Failed to remove temporary directory {}: {}", path.string(), ec.message());
        }
    }

    TempTestDir(const TempTestDir&) = delete;
    TempTestDir& operator=(const TempTestDir&) = delete;
};

// Test function signature
using TestFunc = bool(const std::string& test_name_prefix); // fs_ops removed

bool test_create_standard_success(const std::string& test_name_prefix) {
    std::string test_name = test_name_prefix + "test_create_standard_success";
    std::print(std::cout, "{}: ", test_name); // Use std::print for no newline
    TempTestDir temp_dir(test_name);
    std::filesystem::path dir_to_create = temp_dir.path / "new_dir_std";

    xinim::fs::operation_context ctx;
    ctx.execution_mode = xinim::fs::mode::standard;

    auto result = xinim::fs::create_directory(dir_to_create, std::filesystem::perms::all, ctx);
    if (!result) {
        std::println(std::cout, "FAIL (creation failed: {})", result.error().message());
        return false;
    }
    if (!std::filesystem::is_directory(dir_to_create)) {
        std::println(std::cout, "FAIL (directory not created or not a directory)");
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}

bool test_create_direct_success(const std::string& test_name_prefix) {
    std::string test_name = test_name_prefix + "test_create_direct_success";
    std::print(std::cout, "{}: ", test_name);
    TempTestDir temp_dir(test_name);
    std::filesystem::path dir_to_create = temp_dir.path / "new_dir_direct";

    auto perms = std::filesystem::perms::owner_all |
                 std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                 std::filesystem::perms::others_read | std::filesystem::perms::others_exec;

    xinim::fs::operation_context ctx;
    ctx.execution_mode = xinim::fs::mode::direct;

    auto result = xinim::fs::create_directory(dir_to_create, perms, ctx);
    if (!result) {
        std::println(std::cout, "FAIL (creation failed: {})", result.error().message());
        return false;
    }
    if (!std::filesystem::is_directory(dir_to_create)) {
        std::println(std::cout, "FAIL (directory not created or not a directory)");
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}

bool test_create_already_exists_dir_success(const std::string& test_name_prefix) {
    std::string test_name_std = test_name_prefix + "test_create_already_exists_dir_success (standard)";
    std::print(std::cout, "{}: ", test_name_std);
    TempTestDir temp_dir_std(test_name_std);
    std::filesystem::path dir_path_std = temp_dir_std.path / "existing_dir_std";
    std::filesystem::create_directory(dir_path_std);

    xinim::fs::operation_context ctx_std;
    ctx_std.execution_mode = xinim::fs::mode::standard;
    auto result_std = xinim::fs::create_directory(dir_path_std, std::filesystem::perms::all, ctx_std);
    if (!result_std) {
        std::println(std::cout, "FAIL (standard mode failed: {})", result_std.error().message());
        return false;
    }
    std::println(std::cout, "PASS");

    std::string test_name_direct = test_name_prefix + "test_create_already_exists_dir_success (direct)";
    std::print(std::cout, "{}: ", test_name_direct);
    TempTestDir temp_dir_direct(test_name_direct);
    std::filesystem::path dir_path_direct = temp_dir_direct.path / "existing_dir_direct";
    std::filesystem::create_directory(dir_path_direct);

    xinim::fs::operation_context ctx_direct;
    ctx_direct.execution_mode = xinim::fs::mode::direct;
    auto result_direct = xinim::fs::create_directory(dir_path_direct, std::filesystem::perms::all, ctx_direct);
    if (!result_direct) {
        std::println(std::cout, "FAIL (direct mode failed: {})", result_direct.error().message());
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}

bool test_create_fails_if_file_exists(const std::string& test_name_prefix) {
    std::string test_name_std = test_name_prefix + "test_create_fails_if_file_exists (standard)";
    std::print(std::cout, "{}: ", test_name_std);
    TempTestDir temp_dir_std(test_name_std);
    std::filesystem::path file_path_std = temp_dir_std.path / "existing_file.txt";
    { std::ofstream outfile(file_path_std); outfile << "hello"; }
    assert(std::filesystem::is_regular_file(file_path_std));

    xinim::fs::operation_context ctx_std;
    ctx_std.execution_mode = xinim::fs::mode::standard;
    auto result_std = xinim::fs::create_directory(file_path_std, std::filesystem::perms::all, ctx_std);
    if (result_std || result_std.error() != std::errc::file_exists) {
        std::println(std::cout, "FAIL (standard mode did not fail as expected or wrong error)");
        if(result_std) std::println(std::cerr, "    Expected failure, got success");
        else std::println(std::cerr, "    Expected file_exists, got {}", result_std.error().message());
        return false;
    }
    std::println(std::cout, "PASS");

    std::string test_name_direct = test_name_prefix + "test_create_fails_if_file_exists (direct)";
    std::print(std::cout, "{}: ", test_name_direct);
    TempTestDir temp_dir_direct(test_name_direct);
    std::filesystem::path file_path_direct = temp_dir_direct.path / "existing_file.txt";
    { std::ofstream outfile(file_path_direct); outfile << "hello"; }
    assert(std::filesystem::is_regular_file(file_path_direct));

    xinim::fs::operation_context ctx_direct;
    ctx_direct.execution_mode = xinim::fs::mode::direct;
    auto result_direct = xinim::fs::create_directory(file_path_direct, std::filesystem::perms::all, ctx_direct);
    if (result_direct || result_direct.error() != std::errc::file_exists ) {
        std::println(std::cout, "FAIL (direct mode did not fail as expected or wrong error)");
         if(result_direct) std::println(std::cerr, "    Expected failure, got success");
        else std::println(std::cerr, "    Expected file_exists (EEXIST), got {}", result_direct.error().message());
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}

bool test_create_direct_fails_no_parent(const std::string& test_name_prefix) {
    std::string test_name = test_name_prefix + "test_create_direct_fails_no_parent";
    std::print(std::cout, "{}: ", test_name);
    TempTestDir temp_dir(test_name);
    std::filesystem::path dir_to_create = temp_dir.path / "non_existent_parent" / "new_dir_direct";

    xinim::fs::operation_context ctx;
    ctx.execution_mode = xinim::fs::mode::direct;
    auto result = xinim::fs::create_directory(dir_to_create, std::filesystem::perms::owner_all, ctx);
    if (result || result.error() != std::errc::no_such_file_or_directory) {
        std::println(std::cout, "FAIL (direct mode did not fail with ENOENT as expected)");
        if(result) std::println(std::cerr, "    Expected failure, got success");
        else std::println(std::cerr, "    Expected no_such_file_or_directory (ENOENT), got {}", result.error().message());
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}

bool test_create_standard_fails_no_parent(const std::string& test_name_prefix) {
    std::string test_name = test_name_prefix + "test_create_standard_fails_no_parent";
    std::print(std::cout, "{}: ", test_name);
    TempTestDir temp_dir(test_name);
    std::filesystem::path dir_to_create = temp_dir.path / "non_existent_parent_std" / "new_dir_std";

    xinim::fs::operation_context ctx;
    ctx.execution_mode = xinim::fs::mode::standard;
    auto result = xinim::fs::create_directory(dir_to_create, std::filesystem::perms::all, ctx);
    if (result || result.error() != std::errc::no_such_file_or_directory) {
        std::println(std::cout, "FAIL (standard mode did not fail with ENOENT as expected)");
        if(result) std::println(std::cerr, "    Expected failure, got success");
        else std::println(std::cerr, "    Expected no_such_file_or_directory (ENOENT), got {}", result.error().message());
        return false;
    }
    std::println(std::cout, "PASS");
    return true;
}


int main() {
    // xinim::fs::filesystem_ops fs_ops; // Removed
    int failures = 0;
    std::string test_suite_prefix = "CreateDirectoryTests::";

    // Updated TestFunc type and vector initialization
    std::vector<std::function<bool(const std::string&)>> tests = {
        test_create_standard_success,
        test_create_direct_success,
        test_create_already_exists_dir_success,
        test_create_fails_if_file_exists,
        test_create_direct_fails_no_parent,
        test_create_standard_fails_no_parent
    };

    srand(static_cast<unsigned int>(time(nullptr)));

    for (auto& test_func : tests) { // Use auto&
        if (!test_func(test_suite_prefix)) { // Call with new signature
            failures++;
        }
    }

    if (failures > 0) {
        std::println(std::cout, "\n{} TEST(S) FAILED.", failures); // Print to cout for consistency or cerr for errors
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL TESTS PASSED.");
    return EXIT_SUCCESS;
}
