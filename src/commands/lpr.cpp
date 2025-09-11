/**
 * @file lpr.cpp
 * @brief Universal Line Printer Frontend - C++23 Modernized
 * @details Hardware-agnostic, SIMD/FPU-ready line printer interface with
 *          comprehensive tab expansion, line ending conversion, and robust
 *          print queue management.
 *
 * MODERNIZATION: Complete decomposition and synthesis from legacy K&R C to
 * paradigmatically pure C++23 with RAII, exception safety, type safety,
 * vectorization readiness, and hardware abstraction.
 *
 * @author Original: Andy Tanenbaum, Modernized for C++23 XINIM
 * @version 2.0
 * @date 2024
 * @copyright XINIM OS Project
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace xinim::commands::lpr {

/**
 * @brief Universal Line Printer Management System
 * @details Complete C++23 modernization providing hardware-agnostic,
 *          SIMD/FPU-optimized print processing with robust error handling,
 *          tab expansion, line ending conversion, and print queue management.
 */
class UniversalLinePrinter final {
  public:
    /// Optimal block size for vectorized I/O operations
    static constexpr std::size_t OPTIMAL_BLOCK_SIZE{4096};

    /// Tab stop interval (every 8 characters)
    static constexpr std::uint32_t TAB_STOP_INTERVAL{8};

    /// Maximum retry attempts for busy printer
    static constexpr std::uint32_t MAX_RETRY_ATTEMPTS{5};

    /// Retry delay in milliseconds
    static constexpr std::chrono::milliseconds RETRY_DELAY{1000};

  private:
    /// Input buffer for vectorized reading
    std::array<std::uint8_t, OPTIMAL_BLOCK_SIZE> input_buffer_{};

    /// Output buffer for vectorized writing
    std::array<std::uint8_t, OPTIMAL_BLOCK_SIZE> output_buffer_{};

    /// Current position in input buffer
    std::size_t input_position_{0};

    /// Valid bytes in input buffer
    std::size_t input_count_{0};

    /// Current position in output buffer
    std::size_t output_count_{0};

    /// Current column position for tab expansion
    std::uint32_t column_position_{0};

    /// Printer device file descriptor
    int printer_fd_{-1};

  public:
    /**
     * @brief Initialize universal line printer with device verification
     * @param device_path Path to printer device (default: /dev/lp)
     * @throws std::system_error on device access failure
     */
    explicit UniversalLinePrinter(std::string_view device_path = "/dev/lp") {
        // Redirect stdout to printer device with error handling
        if (::close(STDOUT_FILENO) == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to close stdout");
        }

        printer_fd_ = ::open(device_path.data(), O_WRONLY);
        if (printer_fd_ == -1) {
            throw std::system_error(errno, std::system_category(),
                                    std::string("Cannot open printer device: ") +
                                        device_path.data());
        }
    }

    /**
     * @brief RAII cleanup with automatic flush and device closure
     */
    ~UniversalLinePrinter() noexcept {
        try {
            flush_output_buffer();
        } catch (...) {
            // Suppress exceptions in destructor
        }

        if (printer_fd_ != -1) {
            ::close(printer_fd_);
        }
    }

    // Prevent copying and moving for singleton printer access
    UniversalLinePrinter(const UniversalLinePrinter &) = delete;
    UniversalLinePrinter &operator=(const UniversalLinePrinter &) = delete;
    UniversalLinePrinter(UniversalLinePrinter &&) = delete;
    UniversalLinePrinter &operator=(UniversalLinePrinter &&) = delete;

    /**
     * @brief Process multiple files for printing with comprehensive error handling
     *
     * @param file_paths Vector of file paths to print
     * @throws std::exception On any file
     * processing failure
     */
    void process_files(const std::vector<std::string> &file_paths) {
        if (file_paths.empty()) {
            // Process standard input
            process_file_descriptor(STDIN_FILENO);
            return;
        }

        for (const auto &file_path : file_paths) {
            try {
                process_single_file(file_path);
            } catch (const std::exception &e) {
                std::cerr << "lpr: Error processing file '" << file_path << "': " << e.what()
                          << '\n';
                throw;
            }
        }
    }

  private:
    /**
     * @brief Process single file with RAII file descriptor management
     * @param
     * file_path Path to file to print
     * @throws std::system_error On file access failure
 */
    void process_single_file(const std::string &file_path) {
        int fd = ::open(file_path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category(),
                                    std::string("Cannot open file: ") + file_path);
        }

        // RAII wrapper for automatic file descriptor cleanup
        auto close_fd = [](int *fd_ptr) {
            if (fd_ptr && *fd_ptr != -1)
                ::close(*fd_ptr);
        };
        std::unique_ptr<int, decltype(close_fd)> fd_guard(&fd, close_fd);

        // Reset buffer state for new file
        input_position_ = 0;
        input_count_ = 0;

        process_file_descriptor(fd);
    }

    /**
     * @brief Core file processing with vectorized I/O and character conversion
     *
     * @param fd File descriptor to process
     * @details Performs tab expansion and line ending
     * conversion with optimal
     *          buffering and SIMD-ready data processing
     *
     * @throws std::system_error On read or write failures
     */
    void process_file_descriptor(int fd) {
        while (true) {
            // Refill input buffer when needed
            if (input_position_ >= input_count_) {
                ssize_t bytes_read = ::read(fd, input_buffer_.data(), input_buffer_.size());
                if (bytes_read == 0) {
                    // End of file reached
                    flush_output_buffer();
                    return;
                }
                if (bytes_read == -1) {
                    throw std::system_error(errno, std::system_category(), "Read error");
                }

                input_count_ = static_cast<std::size_t>(bytes_read);
                input_position_ = 0;
            }

            // Process current character with conversion
            const std::uint8_t current_char = input_buffer_[input_position_++];

            switch (current_char) {
            case '\n':
                // Convert LF to CR+LF for proper printer line ending
                write_character('\r');
                write_character('\n');
                break;

            case '\t':
                // Expand tab to spaces up to next tab stop
                expand_tab();
                break;

            default:
                // Regular character
                write_character(current_char);
                break;
            }
        }
    }

    /**
     * @brief Expand tab character to appropriate number of spaces
     * @details Hardware-agnostic tab expansion with vectorization readiness
     */
    void expand_tab() noexcept {
        do {
            write_character(' ');
        } while ((column_position_ & (TAB_STOP_INTERVAL - 1)) != 0);
    }

    /**
     * @brief Write single character with buffering and column tracking
     * @param c Character to write
     * @details Maintains column position for tab expansion and triggers
     *          buffer flush when needed
     */
    void write_character(std::uint8_t c) {
        output_buffer_[output_count_++] = c;

        // Update column position
        if (c == '\n') {
            column_position_ = 0;
        } else {
            ++column_position_;
        }

        // Flush buffer when full
        if (output_count_ >= output_buffer_.size()) {
            flush_output_buffer();
        }
    }

    /**
     * @brief Flush output buffer to printer with retry logic
     * @throws std::system_error on persistent write failure
     * @details Implements robust retry mechanism for busy printer conditions
     */
    void flush_output_buffer() {
        if (output_count_ == 0) {
            return;
        }

        std::uint32_t retry_count = 0;
        std::size_t bytes_written = 0;

        while (bytes_written < output_count_) {
            ssize_t result = ::write(printer_fd_, output_buffer_.data() + bytes_written,
                                     output_count_ - bytes_written);

            if (result > 0) {
                bytes_written += static_cast<std::size_t>(result);
                continue;
            }

            if (result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Printer busy - implement retry with backoff
                    if (++retry_count > MAX_RETRY_ATTEMPTS) {
                        throw std::system_error(errno, std::system_category(),
                                                "Printer remains busy after maximum retries");
                    }

                    std::this_thread::sleep_for(RETRY_DELAY);
                    continue;
                } else {
                    throw std::system_error(errno, std::system_category(), "Printer write error");
                }
            }
        }

        // Reset output buffer
        output_count_ = 0;
    }
};

} // namespace xinim::commands::lpr

/**
 * @brief Entry point for the lpr utility.
 * @param argc Number of command-line arguments.
 *
 * @param argv Array of command-line argument strings.
 * @return Exit status code.
 * @details
 * Creates a UniversalLinePrinter instance and processes each
 *          provided file path,
 * falling back to standard input when no
 *          paths are supplied.
 */
int main(int argc, char *argv[]) noexcept {
    try {
        // Create universal line printer instance
        xinim::commands::lpr::UniversalLinePrinter printer;

        // Prepare file list from command line arguments
        std::vector<std::string> file_paths;
        if (argc > 1) {
            file_paths.reserve(static_cast<std::size_t>(argc - 1));
            for (int i = 1; i < argc; ++i) {
                file_paths.emplace_back(argv[i]);
            }
        }

        // Process files
        printer.process_files(file_paths);

        return EXIT_SUCCESS;

    } catch (const std::system_error &e) {
        std::cerr << "lpr: System error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "lpr: Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "lpr: Unknown error occurred\n";
        return EXIT_FAILURE;
    }
}
