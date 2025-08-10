/**
 * @file dd.cpp
 * @brief Convert and copy a file.
 * @author Original authors of dd
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the classic `dd` utility.
 * It provides a robust, type-safe, and extensible implementation for block-based
 * file copying and conversion. The design uses modern C++ idioms, including
 * RAII for resource management, exceptions for error handling, and the std::filesystem
 * and
 * std::chrono libraries.
 * Usage: dd [operand]... Operands: if=FILE         Read from FILE instead
 * of stdin of=FILE         Write to FILE instead of stdout ibs=N           Set input block size to
 * N bytes (default: 512) obs=N           Set output block size to N bytes (default: 512) bs=N Set
 * both input and output block size to N count=N         Copy only N input blocks skip=N Skip N
 * input blocks at start of input seek=N          Skip N output blocks at start of output
 *   conv=CONV[,CONV...]
 *                   Convert the file as per the comma-separated list of symbols.
 *                   Supported conversions: ucase, lcase, swab, noerror, sync
 */

#include <algorithm>
#include <charconv>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

enum class Conversion { UCASE, LCASE, SWAB, NOERROR, SYNC };

struct DdOptions {
    std::string ifile = "-";
    std::string ofile = "-";
    size_t ibs = 512;
    size_t obs = 512;
    size_t count = 0; // 0 means until EOF
    size_t skip = 0;
    size_t seek = 0;
    std::vector<Conversion> conv_flags;
};

class DdCommand;
DdCommand *running_command = nullptr;

void handle_signal(int signum) {
    if (running_command) {
        // Forward to a non-static member function if needed
    }
    // Since we can't call printStatistics directly, we just exit.
    // A more complex signal handler might set a flag.
    std::cerr << "\n dd: interrupted." << std::endl;
    _exit(1);
}

class DdCommand {
  public:
    explicit DdCommand(DdOptions opts) : options(std::move(opts)) {}

    void run() {
        running_command = this;
        std::signal(SIGINT, handle_signal);

        start_time = std::chrono::steady_clock::now();

        open_files();
        handle_skip_seek();
        main_loop();

        end_time = std::chrono::steady_clock::now();
        print_statistics();
    }

  private:
    void open_files() {
        if (options.ifile == "-") {
            in = &std::cin;
        } else {
            ifile_stream.open(options.ifile, std::ios::binary);
            if (!ifile_stream)
                throw std::runtime_error("Cannot open input file: " + options.ifile);
            in = &ifile_stream;
        }

        if (options.ofile == "-") {
            out = &std::cout;
        } else {
            ofile_stream.open(options.ofile, std::ios::binary | std::ios::trunc);
            if (!ofile_stream)
                throw std::runtime_error("Cannot open output file: " + options.ofile);
            out = &ofile_stream;
        }
    }

    void handle_skip_seek() {
        if (options.skip > 0) {
            in->seekg(options.skip * options.ibs, std::ios::beg);
            if (in->fail())
                throw std::runtime_error("Error skipping in input file.");
        }
        if (options.seek > 0) {
            out->seekp(options.seek * options.obs, std::ios::beg);
            if (out->fail())
                throw std::runtime_error("Error seeking in output file.");
        }
    }

    void main_loop() {
        std::vector<char> buffer(options.ibs);
        size_t blocks_copied = 0;

        while (!in->eof() && (options.count == 0 || blocks_copied < options.count)) {
            in->read(buffer.data(), options.ibs);
            std::streamsize bytes_read = in->gcount();

            if (bytes_read == 0)
                break;

            if (bytes_read == options.ibs) {
                records_in_full++;
            } else {
                records_in_partial++;
            }

            std::vector<char> processed_buffer(buffer.begin(), buffer.begin() + bytes_read);
            apply_conversions(processed_buffer);

            out->write(processed_buffer.data(), processed_buffer.size());
            if (out->fail())
                throw std::runtime_error("Write error.");

            if (processed_buffer.size() == options.obs) {
                records_out_full++;
            } else if (processed_buffer.size() > 0) {
                records_out_partial++;
            }

            blocks_copied++;
        }
    }

    void apply_conversions(std::vector<char> &buffer) {
        for (auto flag : options.conv_flags) {
            switch (flag) {
            case Conversion::UCASE:
                std::transform(buffer.begin(), buffer.end(), buffer.begin(), ::toupper);
                break;
            case Conversion::LCASE:
                std::transform(buffer.begin(), buffer.end(), buffer.begin(), ::tolower);
                break;
            case Conversion::SWAB:
                if (buffer.size() % 2 != 0)
                    truncated_records++;
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
                // This is handled by continuing on read errors, which is the default for streams.
                break;
            }
        }
    }

    void print_statistics() {
        std::cerr << records_in_full << "+" << records_in_partial << " records in" << std::endl;
        std::cerr << records_out_full << "+" << records_out_partial << " records out" << std::endl;
        if (truncated_records > 0) {
            std::cerr << truncated_records << " truncated records" << std::endl;
        }

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cerr << "dd finished in " << duration.count() << " ms" << std::endl;
    }

    DdOptions options;
    std::istream *in = nullptr;
    std::ostream *out = nullptr;
    std::ifstream ifile_stream;
    std::ofstream ofile_stream;

    size_t records_in_full = 0;
    size_t records_in_partial = 0;
    size_t records_out_full = 0;
    size_t records_out_partial = 0;
    size_t truncated_records = 0;

    std::chrono::steady_clock::time_point start_time, end_time;
};

size_t parse_num(std::string_view s) {
    long long val = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), val);
    if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
        throw std::invalid_argument("Invalid numeric value");
    }
    return static_cast<size_t>(val);
}

DdOptions parse_arguments(int argc, char *argv[]) {
    DdOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        auto pos = arg.find('=');
        if (pos == std::string_view::npos)
            throw std::runtime_error("Invalid argument: " + std::string(arg));

        std::string_view key = arg.substr(0, pos);
        std::string_view value = arg.substr(pos + 1);

        if (key == "if")
            opts.ifile = value;
        else if (key == "of")
            opts.ofile = value;
        else if (key == "ibs")
            opts.ibs = parse_num(value);
        else if (key == "obs")
            opts.obs = parse_num(value);
        else if (key == "bs")
            opts.ibs = opts.obs = parse_num(value);
        else if (key == "count")
            opts.count = parse_num(value);
        else if (key == "skip")
            opts.skip = parse_num(value);
        else if (key == "seek")
            opts.seek = parse_num(value);
        else if (key == "conv") {
            std::string_view v = value;
            while (!v.empty()) {
                auto comma_pos = v.find(',');
                std::string_view conv_str = v.substr(0, comma_pos);
                if (conv_str == "ucase")
                    opts.conv_flags.push_back(Conversion::UCASE);
                else if (conv_str == "lcase")
                    opts.conv_flags.push_back(Conversion::LCASE);
                else if (conv_str == "swab")
                    opts.conv_flags.push_back(Conversion::SWAB);
                else if (conv_str == "noerror")
                    opts.conv_flags.push_back(Conversion::NOERROR);
                else if (conv_str == "sync")
                    opts.conv_flags.push_back(Conversion::SYNC);
                else
                    throw std::runtime_error("Unknown conversion: " + std::string(conv_str));
                if (comma_pos == std::string_view::npos)
                    break;
                v.remove_prefix(comma_pos + 1);
            }
        } else {
            throw std::runtime_error("Unknown key: " + std::string(key));
        }
    }
    return opts;
}

} // namespace

int main(int argc, char *argv[]) {
    try {
        DdOptions options = parse_arguments(argc, argv);
        DdCommand command(std::move(options));
        command.run();
    } catch (const std::exception &e) {
        std::cerr << "dd: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
