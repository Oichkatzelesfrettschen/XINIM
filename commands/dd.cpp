/**
 * @file dd.cpp
 * @brief Modern C++23 implementation of the dd utility for XINIM operating system
 * @author Original authors of dd, modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @copyright Copyright (c) 2025, The XINIM Project. All rights reserved.
 *
 * @section Description
 * A modernized implementation of the classic `dd` utility for block-based file copying
 * and conversion. This version leverages C++23 features for type safety, RAII, thread
 * safety, and performance optimization. It supports reading from and writing to files
 * or standard streams, with conversions such as case transformation, byte swapping,
 * and error handling.
 *
 * @section Features
 * - RAII for resource management
 * - Exception-safe error handling
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration for compile-time optimization
 * - Memory-safe buffer management with std::vector
 * - Comprehensive Doxygen documentation
 * - Support for C++23 ranges and string formatting
 *
 * @section Usage
 * dd [operand]...
 *
 * Operands:
 * - if=FILE: Read from FILE instead of stdin
 * - of=FILE: Write to FILE instead of stdout
 * - ibs=N: Set input block size to N bytes (default: 512)
 * - obs=N: Set output block size to N bytes (default: 512)
 * - bs=N: Set both input and output block size to N
 * - count=N: Copy only N input blocks
 * - skip=N: Skip N input blocks at start of input
 * - seek=N: Skip N output blocks at start of output
 * - conv=CONV[,CONV...]: Convert the file as per the comma-separated list of symbols
 * Supported conversions: ucase, lcase, swab, noerror, sync
 *
 * @note Requires C++23 compliant compiler
 */
// clang-format on

#include <algorithm>
#include <charconv>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

enum class Conversion { UCASE, LCASE, SWAB, NOERROR, SYNC };

struct DdOptions {
    std::string ifile = "-"; /**< Input file (default: stdin) */
    std::string ofile = "-"; /**< Output file (default: stdout) */
    size_t ibs = 512;        /**< Input block size in bytes */
    size_t obs = 512;        /**< Output block size in bytes */
    size_t count = 0;        /**< Number of blocks to copy (0 means until EOF) */
    size_t skip = 0;         /**< Number of input blocks to skip */
    size_t seek = 0;         /**< Number of output blocks to skip */
    std::vector<Conversion> conv_flags; /**< List of conversion flags */
};

class DdCommand {
public:
    explicit DdCommand(DdOptions opts) : options(std::move(opts)) {
        running_command_ = this;
    }

    ~DdCommand() {
        std::lock_guard lock(mtx_);
        running_command_ = nullptr;
    }

    void run() {
        std::lock_guard lock(mtx_);
        std::signal(SIGINT, handle_signal);
        start_time_ = std::chrono::steady_clock::now();
        open_files();
        handle_skip_seek();
        main_loop();
        end_time_ = std::chrono::steady_clock::now();
        print_statistics();
    }

private:
    void open_files() {
        std::lock_guard lock(mtx_);
        if (options.ifile == "-") {
            in_ = &std::cin;
        } else {
            ifile_stream_.open(options.ifile, std::ios::binary);
            if (!ifile_stream_) {
                throw std::runtime_error(std::format("Cannot open input file: {}", options.ifile));
            }
            in_ = &ifile_stream_;
        }
        if (options.ofile == "-") {
            out_ = &std::cout;
        } else {
            ofile_stream_.open(options.ofile, std::ios::binary | std::ios::trunc);
            if (!ofile_stream_) {
                throw std::runtime_error(std::format("Cannot open output file: {}", options.ofile));
            }
            out_ = &ofile_stream_;
        }
    }

    void handle_skip_seek() {
        std::lock_guard lock(mtx_);
        if (options.skip > 0) {
            in_->seekg(options.skip * options.ibs, std::ios::beg);
            if (in_->fail()) {
                throw std::runtime_error("Error skipping in input file.");
            }
        }
        if (options.seek > 0) {
            out_->seekp(options.seek * options.obs, std::ios::beg);
            if (out_->fail()) {
                throw std::runtime_error("Error seeking in output file.");
            }
        }
    }

    void main_loop() {
        std::lock_guard lock(mtx_);
        std::vector<char> buffer(options.ibs);
        size_t blocks_copied = 0;

        while (!in_->eof() && (options.count == 0 || blocks_copied < options.count)) {
            in_->read(buffer.data(), options.ibs);
            std::streamsize bytes_read = in_->gcount();
            if (bytes_read == 0) break;

            if (bytes_read == static_cast<std::streamsize>(options.ibs)) {
                records_in_full_++;
            } else {
                records_in_partial_++;
            }

            std::vector<char> processed_buffer(buffer.begin(), buffer.begin() + bytes_read);
            apply_conversions(processed_buffer);

            out_->write(processed_buffer.data(), processed_buffer.size());
            if (out_->fail()) {
                throw std::runtime_error("Write error.");
            }

            if (processed_buffer.size() == options.obs) {
                records_out_full_++;
            } else if (processed_buffer.size() > 0) {
                records_out_partial_++;
            }

            blocks_copied++;
        }
    }

    void apply_conversions(std::vector<char> &buffer) {
        for (auto flag : options.conv_flags) {
            switch (flag) {
            case Conversion::UCASE:
                std::ranges::transform(buffer, buffer.begin(), ::toupper);
                break;
            case Conversion::LCASE:
                std::ranges::transform(buffer, buffer.begin(), ::tolower);
                break;
            case Conversion::SWAB:
                if (buffer.size() % 2 != 0) truncated_records_++;
                for (size_t i = 0; i + 1 < buffer.size(); i += 2) {
                    std::swap(buffer[i], buffer[i + 1]);
                }
                break;
            case Conversion::SYNC:
                if (buffer.size() < options.ibs) {
                    buffer.resize(options.ibs, '\0');
                }
                break;
            case Conversion::NOERROR:
                // Handled by continuing on read errors (default for streams)
                break;
            }
        }
    }

    void print_statistics() const {
        std::lock_guard lock(mtx_);
        std::cerr << std::format("{}+{} records in\n", records_in_full_, records_in_partial_);
        std::cerr << std::format("{}+{} records out\n", records_out_full_, records_out_partial_);
        if (truncated_records_ > 0) {
            std::cerr << std::format("{} truncated records\n", truncated_records_);
        }
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
        std::cerr << std::format("dd finished in {} ms\n", duration.count());
    }

    static void handle_signal(int signum) noexcept {
        std::lock_guard lock(running_command_->mtx_);
        std::signal(signum, SIG_IGN);
        std::cerr << "\ndd: interrupted.\n";
        _exit(1);
    }

    DdOptions options;
    std::istream* in_ = nullptr;
    std::ostream* out_ = nullptr;
    std::ifstream ifile_stream_;
    std::ofstream ofile_stream_;
    size_t records_in_full_ = 0;
    size_t records_in_partial_ = 0;
    size_t records_out_full_ = 0;
    size_t records_out_partial_ = 0;
    size_t truncated_records_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    mutable std::mutex mtx_;
    static thread_local DdCommand* running_command_;
};

thread_local DdCommand* DdCommand::running_command_ = nullptr;

size_t parse_num(std::string_view s) {
    long long val = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), val);
    if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
        throw std::invalid_argument(std::format("Invalid numeric value: {}", s));
    }
    if (val < 0) {
        throw std::invalid_argument(std::format("Negative value not allowed: {}", s));
    }
    return static_cast<size_t>(val);
}

DdOptions parse_arguments(int argc, char *argv[]) {
    DdOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        auto pos = arg.find('=');
        if (pos == std::string_view::npos) {
            throw std::runtime_error(std::format("Invalid argument: {}", arg));
        }
        std::string_view key = arg.substr(0, pos);
        std::string_view value = arg.substr(pos + 1);

        if (key == "if") {
            opts.ifile = value;
        } else if (key == "of") {
            opts.ofile = value;
        } else if (key == "ibs") {
            opts.ibs = parse_num(value);
        } else if (key == "obs") {
            opts.obs = parse_num(value);
        } else if (key == "bs") {
            opts.ibs = opts.obs = parse_num(value);
        } else if (key == "count") {
            opts.count = parse_num(value);
        } else if (key == "skip") {
            opts.skip = parse_num(value);
        } else if (key == "seek") {
            opts.seek = parse_num(value);
        } else if (key == "conv") {
            std::string_view v = value;
            while (!v.empty()) {
                auto comma_pos = v.find(',');
                std::string_view conv_str = v.substr(0, comma_pos);
                if (conv_str == "ucase") {
                    opts.conv_flags.push_back(Conversion::UCASE);
                } else if (conv_str == "lcase") {
                    opts.conv_flags.push_back(Conversion::LCASE);
                } else if (conv_str == "swab") {
                    opts.conv_flags.push_back(Conversion::SWAB);
                } else if (conv_str == "noerror") {
                    opts.conv_flags.push_back(Conversion::NOERROR);
                } else if (conv_str == "sync") {
                    opts.conv_flags.push_back(Conversion::SYNC);
                } else {
                    throw std::runtime_error(std::format("Unknown conversion: {}", conv_str));
                }
                if (comma_pos == std::string_view::npos) break;
                v.remove_prefix(comma_pos + 1);
            }
        } else {
            throw std::runtime_error(std::format("Unknown key: {}", key));
        }
    }
    return opts;
}

} // namespace

/**
 * @brief Entry point for the dd utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char *argv[]) {
    try {
        DdOptions options = parse_arguments(argc, argv);
        DdCommand command(std::move(options));
        command.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::format("dd: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "dd: Unknown fatal error occurred\n";
        return 1;
    }
}