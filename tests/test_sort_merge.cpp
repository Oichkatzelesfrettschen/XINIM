#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#include "../commands/sort.cpp"

/**
 * @file test_sort_merge.cpp
 * @brief Unit test for the merge functionality of the modern sort utility.
 */
using namespace xinim::sort_utility;

/// Helper to create a temporary file with given lines.
static auto create_temp_file(const std::filesystem::path &path,
                             const std::vector<std::string> &lines) -> void {
    std::ofstream out{path};
    for (const auto &line : lines) {
        out << line << '\n';
    }
}

int main() {
    const auto file1 = std::filesystem::path{"merge_input1.txt"};
    const auto file2 = std::filesystem::path{"merge_input2.txt"};
    const auto outfile = std::filesystem::path{"merge_output.txt"};

    create_temp_file(file1, {"apple", "banana"});
    create_temp_file(file2, {"apricot", "blueberry"});

    SortConfig cfg;
    cfg.global_flags = SortFlag::Merge;
    cfg.input_files = {file1, file2};
    cfg.output_file = outfile;

    SortUtilityApp app{cfg};
    auto res = app.run();
    assert(res && "merge operation failed");

    std::ifstream in{outfile};
    std::vector<std::string> merged;
    std::string line;
    while (std::getline(in, line)) {
        merged.push_back(line);
    }

    assert((merged == std::vector<std::string>{"apple", "apricot", "banana", "blueberry"}));

    std::filesystem::remove(file1);
    std::filesystem::remove(file2);
    std::filesystem::remove(outfile);
    return 0;
}
