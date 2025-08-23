/**
 * @file cmp.cpp
 * @brief Compare two files byte by byte.
 * @authors Paul Polderman & Michiel Huisjes (original authors)
 * @date 2023-10-27 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the original `cmp` utility from MINIX.
 * It compares two files and reports the first difference found.
 * It supports options for silent mode (-s) and listing all differing bytes (-l).
 *
 * Usage: cmp [-l] [-s] file1 file2
 */

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>
#include <iomanip>

namespace {

/**
 * @struct CmpOptions
 * @brief Holds the command-line options for the cmp utility.
 */
struct CmpOptions {
    bool list_all_diffs = false; // -l flag
    bool silent = false;         // -s flag
    std::filesystem::path file1_path;
    std::filesystem::path file2_path;
};

/**
 * @brief Prints the usage message to standard error.
 */
void printUsage() {
    std::cerr << "Usage: cmp [-l] [-s] file1 file2" << std::endl;
}

/**
 * @brief Parses command-line arguments and populates the options struct.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return A CmpOptions struct.
 * @throws std::runtime_error if arguments are invalid.
 */
CmpOptions parseArguments(int argc, char* argv[]) {
    CmpOptions opts;
    std::vector<std::string_view> args(argv + 1, argv + argc);
    std::vector<std::filesystem::path> files;

    for (const auto& arg : args) {
        if (arg.starts_with('-')) {
            if (arg == "-l") {
                opts.list_all_diffs = true;
            } else if (arg == "-s") {
                opts.silent = true;
            } else if (arg == "-") {
                files.push_back("-");
            } else {
                throw std::runtime_error("Invalid option: " + std::string(arg));
            }
        } else {
            files.push_back(arg);
        }
    }

    if (files.size() != 2) {
        throw std::runtime_error("Exactly two files must be specified.");
    }

    opts.file1_path = files[0];
    opts.file2_path = files[1];

    return opts;
}

/**
 * @class FileComparer
 * @brief Encapsulates the logic for comparing two files.
 */
class FileComparer {
public:
    FileComparer(const CmpOptions& options);
    int compare();

private:
    std::istream& get_stream(const std::filesystem::path& path, std::ifstream& file_stream);

    CmpOptions m_opts;
};

FileComparer::FileComparer(const CmpOptions& options) : m_opts(options) {}

std::istream& FileComparer::get_stream(const std::filesystem::path& path, std::ifstream& file_stream) {
    if (path == "-") {
        return std::cin;
    }
    file_stream.open(path, std::ios::binary);
    if (!file_stream) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    return file_stream;
}

int FileComparer::compare() {
    std::ifstream f1_stream, f2_stream;
    std::istream& in1 = get_stream(m_opts.file1_path, f1_stream);
    std::istream& in2 = get_stream(m_opts.file2_path, f2_stream);

    long long byte_count = 0;
    long long line_count = 1;
    bool files_differ = false;

    constexpr size_t BUFFER_SIZE = 8192;
    std::vector<char> buffer1(BUFFER_SIZE);
    std::vector<char> buffer2(BUFFER_SIZE);

    while (true) {
        in1.read(buffer1.data(), BUFFER_SIZE);
        in2.read(buffer2.data(), BUFFER_SIZE);

        std::streamsize bytes_read1 = in1.gcount();
        std::streamsize bytes_read2 = in2.gcount();

        if (bytes_read1 == 0 && bytes_read2 == 0) {
            break; // Both files ended.
        }

        std::streamsize limit = std::min(bytes_read1, bytes_read2);

        for (std::streamsize i = 0; i < limit; ++i) {
            byte_count++;
            if (buffer1[i] != buffer2[i]) {
                files_differ = true;
                if (m_opts.silent) {
                    return 1;
                }
                if (m_opts.list_all_diffs) {
                     std::cout << std::setw(8) << byte_count << " "
                               << std::oct << std::setw(3) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(buffer1[i])) << " "
                               << std::oct << std::setw(3) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(buffer2[i])) << std::dec << std::endl;
                } else {
                    std::cout << m_opts.file1_path.string() << " " << m_opts.file2_path.string()
                              << " differ: char " << byte_count << ", line " << line_count << std::endl;
                    return 1;
                }
            }
            if (buffer1[i] == '\n') {
                line_count++;
            }
        }

        if (bytes_read1 != bytes_read2) {
            if (!m_opts.silent) {
                 std::cerr << "cmp: EOF on " << (bytes_read1 < bytes_read2 ? m_opts.file1_path.string() : m_opts.file2_path.string()) << std::endl;
            }
            return 1;
        }
    }

    return files_differ ? 1 : 0;
}

} // namespace

/**
 * @brief Main entry point for the cmp command.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return 0 if files are identical, 1 if they differ, 2 on error.
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 2;
    }

    try {
        CmpOptions options = parseArguments(argc, argv);
        FileComparer comparer(options);
        return comparer.compare();
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("Invalid option") != std::string::npos ||
            std::string(e.what()).find("Exactly two files") != std::string::npos) {
            printUsage();
        } else {
            std::cerr << "cmp: " << e.what() << std::endl;
        }
        return 2;
    }
}
