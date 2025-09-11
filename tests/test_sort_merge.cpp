/**
 * @file test_sort_merge.cpp
 * @brief Unit tests for multi-file merge functionality of the XINIM sort utility.
 *
 * This file provides a unified and enhanced set of unit tests for the merge functionality
 * of the modern C++23 sort utility, synthesizing legacy and updated test cases. It verifies
 * the correct merging of sorted input files, with and without uniqueness constraints,
 * using temporary files for robust testing. The tests ensure proper sorting, error handling,
 * and cleanup, harmonizing the approaches from previous implementations.
 */

#include "../commands/sort.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace xinim::sort_utility;
namespace fs = std::filesystem;

/// Helper function to create a temporary file with given lines.
static auto create_temp_file(const fs::path &path, const std::vector<std::string> &lines) -> void {
    std::ofstream out{path};
    if (!out.is_open()) {
        throw std::runtime_error(std::format("Failed to create temporary file: {}", path.string()));
    }
    for (const auto &line : lines) {
        out << line << '\n';
    }
}

int main() {
    // Use temporary directory for test files
    const auto temp_dir = fs::temp_directory_path();
    const auto file1 = temp_dir / "sort_merge_input1.txt";
    const auto file2 = temp_dir / "sort_merge_input2.txt";
    const auto file3 = temp_dir / "sort_merge_input3.txt"; // Added for 3-way merge test
    const auto outfile = temp_dir / "sort_merge_output.txt";

    // Test Case 1: Basic merge without uniqueness
    {
        create_temp_file(file1, {"apple", "orange"});
        create_temp_file(file2, {"banana", "pear"});

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge;
        cfg.input_files = {file1, file2};
        cfg.output_file = outfile;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result && "Basic merge operation failed");

        std::ifstream in{outfile};
        std::vector<std::string> merged;
        std::string line;
        while (std::getline(in, line)) {
            merged.push_back(line);
        }

        std::vector<std::string> expected{"apple", "banana", "orange", "pear"};
        assert(merged == expected && "Basic merge output does not match expected");
    }

    // Test Case 2: Merge with uniqueness
    {
        create_temp_file(file1, {"a", "c"});
        create_temp_file(file2, {"b", "c"});

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge | SortFlag::Unique;
        cfg.input_files = {file1, file2};
        cfg.output_file = outfile;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result && "Unique merge operation failed");

        std::ifstream in{outfile};
        std::vector<std::string> merged;
        std::string line;
        while (std::getline(in, line)) {
            merged.push_back(line);
        }

        std::vector<std::string> expected{"a", "b", "c"};
        assert(merged == expected && "Unique merge output does not match expected");
    }

    // Test Case 3: Merge with single input file (edge case)
    {
        create_temp_file(file1, {"apple", "banana"});

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge;
        cfg.input_files = {file1};
        cfg.output_file = outfile;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(!result && "Merge with single file should fail");
        assert(result.error().find("at least two input sources") != std::string::npos &&
               "Expected error message for single file merge not found");
    }

    // Test Case 4: Merge with stdin input
    {
        create_temp_file(file1, {"apple", "banana"});
        // Simulate stdin input by redirecting a string stream
        std::stringstream ss;
        ss << "apricot\nblueberry\n";
        auto *const old_buf = std::cin.rdbuf(ss.rdbuf());

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge;
        cfg.input_files = {file1};
        cfg.output_file = outfile;
        cfg.use_stdin = true;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result && "Merge with stdin failed");

        std::ifstream in{outfile};
        std::vector<std::string> merged;
        std::string line;
        while (std::getline(in, line)) {
            merged.push_back(line);
        }

        std::vector<std::string> expected{"apple", "apricot", "banana", "blueberry"};
        assert(merged == expected && "Merge with stdin output does not match expected");

        // Restore stdin
        std::cin.rdbuf(old_buf);
    }

    // Test Case 5: Merge three input files (from eirikr branch)
    {
        create_temp_file(file1, {"apple", "orange"});
        create_temp_file(file2, {"banana", "pear"});
        create_temp_file(file3, {"avocado", "kiwi"});

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge;
        cfg.input_files = {file1, file2, file3};
        cfg.output_file = outfile;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result && "Three-way merge operation failed");

        std::ifstream in{outfile};
        std::vector<std::string> merged;
        std::string line;
        while (std::getline(in, line)) {
            merged.push_back(line);
        }

        std::vector<std::string> expected{"apple", "avocado", "banana", "kiwi", "orange", "pear"};
        assert(merged == expected && "Three-way merge output does not match expected");
    }

    // Cleanup temporary files
    fs::remove(file1);
    fs::remove(file2);
    fs::remove(file3);
    fs::remove(outfile);

    return 0;
}