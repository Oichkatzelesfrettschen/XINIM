/**
 * @file test_sort_merge.cpp
 * @brief Unit tests for multi-file merge functionality of the sort utility.
 */

#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#define XINIM_SORT_NO_MAIN
#include "../commands/sort.cpp"

using namespace xinim::sort_utility;
namespace fs = std::filesystem;

int main() {
    const auto tmp = fs::temp_directory_path();
    const auto file1 = tmp / "sort_merge_a.txt";
    const auto file2 = tmp / "sort_merge_b.txt";
    const auto out = tmp / "sort_merge_out.txt";

    // Prepare first scenario: basic merge without uniqueness.
    {
        std::ofstream f1{file1};
        f1 << "apple\norange\n";
        f1.close();
        std::ofstream f2{file2};
        f2 << "banana\npear\n";
        f2.close();

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge;
        cfg.input_files = {file1, file2};
        cfg.output_file = out;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result);

        std::ifstream fo{out};
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(fo, line)) {
            lines.push_back(line);
        }
        std::vector<std::string> expected{"apple", "banana", "orange", "pear"};
        assert(lines == expected);
    }

    // Prepare second scenario: merge with uniqueness.
    {
        std::ofstream f1{file1};
        f1 << "a\nc\n";
        f1.close();
        std::ofstream f2{file2};
        f2 << "b\nc\n";
        f2.close();

        SortConfig cfg;
        cfg.global_flags = SortFlag::Merge | SortFlag::Unique;
        cfg.input_files = {file1, file2};
        cfg.output_file = out;

        SortUtilityApp app{cfg};
        auto result = app.run();
        assert(result);

        std::ifstream fo{out};
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(fo, line)) {
            lines.push_back(line);
        }
        std::vector<std::string> expected{"a", "b", "c"};
        assert(lines == expected);
    }

    // Cleanup.
    fs::remove(file1);
    fs::remove(file2);
    fs::remove(out);
    return 0;
}
