/**
 * @file comm.cpp
 * @brief Select or reject lines common to two sorted files.
 * @author Martin C. Atkins (original author)
 * @date 2023-10-27 (modernization)
 *
 * @copyright This program was originally written by Martin C. Atkins and released into the public
 * domain.
 *
 * This program is a C++23 modernization of the original `comm` utility.
 * It compares two sorted files line by line and, based on command-line options,
 * outputs lines unique to the first file, lines unique to the second file, and
 * lines common to both files.
 *
 * Usage: comm [-[123]] file1 file2
 */

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

/**
 * @struct CommOptions
 * @brief Holds the command-line options for the comm utility.
 */
struct CommOptions {
    bool suppress_col1 = false;
    bool suppress_col2 = false;
    bool suppress_col3 = false;
    std::filesystem::path file1_path;
    std::filesystem::path file2_path;
};

/**
 * @brief Prints the usage message to standard error.
 */
void printUsage() { std::cerr << "Usage: comm [-[123]] file1 file2" << std::endl; }

/**
 * @brief Parses command-line arguments and populates the options struct.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return A CommOptions struct.
 * @throws std::runtime_error if arguments are invalid.
 */
CommOptions parseArguments(int argc, char *argv[]) {
    CommOptions opts;
    std::vector<std::string_view> args(argv + 1, argv + argc);
    std::vector<std::filesystem::path> files;

    for (const auto &arg : args) {
        if (arg.starts_with('-') && arg.length() > 1) {
            for (char c : arg.substr(1)) {
                switch (c) {
                case '1':
                    opts.suppress_col1 = true;
                    break;
                case '2':
                    opts.suppress_col2 = true;
                    break;
                case '3':
                    opts.suppress_col3 = true;
                    break;
                default:
                    throw std::runtime_error("Invalid option: " + std::string(1, c));
                }
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
 * @brief Encapsulates the logic for comparing two sorted files.
 */
class FileComparer {
  public:
    /**
     * @brief Construct a new FileComparer using the provided options.
     *
     *
     * Initializes the tab prefix array based on which output columns are
     * suppressed. Each
     * column that is not suppressed is assigned the
     * appropriate number of leading tab
     * characters so that subsequent output
     * aligns with POSIX \c comm behavior.
     *
     *
     * @param options Parsed command-line options controlling comparison
     * behavior and column
     * suppression.
     */
    FileComparer(const CommOptions &options);

    /**
     * @brief Execute the comparison of the two input streams.
     *
     * Reads lines
     * from both input files, emitting lines to the appropriate
     * column depending on whether
     * they are unique to the first file, unique to
     * the second file, or common to both. The
     * routine continues until both
     * streams are exhausted.
     */
    void run();

  private:
    /**
     * @brief Acquire an input stream for a given file path.
     *
     * The special path
     * "-" is interpreted as standard input. If a regular
     * file path is provided, an ifstream
     * is opened. Failure to open the file
     * results in a \c std::runtime_error.
     *
     *
     * @param path Path to the desired input file or "-" for \c stdin.
     * @return A unique
     * pointer owning the resulting input stream.
     */
    std::unique_ptr<std::istream> get_stream(const std::filesystem::path &path);

    /**
     * @brief Output a line to the specified column if it is not suppressed.
     *
     *
     * Each column may be suppressed via the command-line options. When a line
     * is emitted,
     * the appropriate tab prefix is prepended to maintain column
     * alignment.
     *
     *
     * @param col 1-based column index designating the output stream.
     * @param line The line to
     * output without a trailing newline.
     */
    void output(int col, const std::string &line);

    CommOptions m_opts;
    std::array<std::string, 3> m_tabs;
};

/**
 * @brief Construct a FileComparer and compute tab prefixes.
 *
 * Based on the suppression
 * options, this constructor precomputes the leading
 * tab characters for each output column so
 * that subsequent output aligns
 * exactly as the traditional \c comm utility expects.
 *
 * @param
 * options Parsed command-line options.
 */
FileComparer::FileComparer(const CommOptions &options) : m_opts(options) {
    int current_tab = 0;
    if (!m_opts.suppress_col1) {
        m_tabs[0] = "";
        current_tab++;
    }
    if (!m_opts.suppress_col2) {
        m_tabs[1] = std::string(current_tab, '\t');
        current_tab++;
    }
    if (!m_opts.suppress_col3) {
        m_tabs[2] = std::string(current_tab, '\t');
    }
}

/**
 * @brief Obtain a stream for the given path.
 *
 * The path "-" is treated as standard input;
 * otherwise an input file stream is
 * created. The caller assumes ownership of the resulting
 * stream via
 * \c std::unique_ptr.
 *
 * @param path Source file path or "-" for \c stdin.
 *
 * @return std::unique_ptr<std::istream> Owning pointer to the input stream.
 */
std::unique_ptr<std::istream> FileComparer::get_stream(const std::filesystem::path &path) {
    if (path == "-") {
        return std::unique_ptr<std::istream>(&std::cin, [](void *) {});
    }
    auto file_stream = std::make_unique<std::ifstream>(path);
    if (!*file_stream) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    return file_stream;
}

/**
 * @brief Emit a line to the selected column.
 *
 * Depending on the column suppression flags,
 * this function prefixes the line
 * with the appropriate number of tabs and writes it to \c
 * std::cout. Suppressed
 * columns result in no output.
 *
 * @param col 1-based index of the
 * output column.
 * @param line Line content without trailing newline.
 */
void FileComparer::output(int col, const std::string &line) {
    bool suppress = false;
    switch (col) {
    case 1:
        suppress = m_opts.suppress_col1;
        break;
    case 2:
        suppress = m_opts.suppress_col2;
        break;
    case 3:
        suppress = m_opts.suppress_col3;
        break;
    }

    if (!suppress) {
        std::cout << m_tabs[col - 1] << line << std::endl;
    }
}

/**
 * @brief Drive the comparison process.
 *
 * Continuously reads from the two input streams,
 * comparing lines and routing
 * them to the proper column via output(). Processing stops when both
 * streams
 * are exhausted.
 */
void FileComparer::run() {
    auto in1 = get_stream(m_opts.file1_path);
    auto in2 = get_stream(m_opts.file2_path);

    std::string line1, line2;
    bool eof1 = !std::getline(*in1, line1);
    bool eof2 = !std::getline(*in2, line2);

    while (!eof1 || !eof2) {
        if (eof1) {
            output(2, line2);
            eof2 = !std::getline(*in2, line2);
        } else if (eof2) {
            output(1, line1);
            eof1 = !std::getline(*in1, line1);
        } else {
            int cmp = line1.compare(line2);
            if (cmp < 0) {
                output(1, line1);
                eof1 = !std::getline(*in1, line1);
            } else if (cmp > 0) {
                output(2, line2);
                eof2 = !std::getline(*in2, line2);
            } else {
                output(3, line1);
                eof1 = !std::getline(*in1, line1);
                eof2 = !std::getline(*in2, line2);
            }
        }
    }
}

} // namespace

/**
 * @brief Main entry point for the comm command.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[]) {
    try {
        CommOptions options = parseArguments(argc, argv);
        FileComparer comparer(options);
        comparer.run();
    } catch (const std::exception &e) {
        std::cerr << "comm: " << e.what() << std::endl;
        if (std::string(e.what()).find("Invalid option") != std::string::npos ||
            std::string(e.what()).find("Exactly two files") != std::string::npos) {
            printUsage();
        }
        return 1;
    }
    return 0;
}
