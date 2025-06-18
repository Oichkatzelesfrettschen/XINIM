/**
 * @file cat.cpp
 * @brief Concatenate files to standard output - C++23 modernized version
 * @author Andy Tanenbaum (original), Modernized for C++23
 * 
 * Modern C++23 implementation using RAII, ranges, and error handling.
 */

#include "blocksiz.hpp"
#include "stat.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <format>
#include <iostream>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

namespace cat {

constexpr std::size_t BUF_SIZE = 512;

enum class [[nodiscard]] Error : std::uint8_t {
    file_open_failed,
    read_failed,
    write_failed,
    invalid_argument
};

class OutputBuffer {
public:
    OutputBuffer() = default;
    ~OutputBuffer() { flush(); }
    
    OutputBuffer(const OutputBuffer&) = delete;
    OutputBuffer& operator=(const OutputBuffer&) = delete;
    OutputBuffer(OutputBuffer&&) = default;
    OutputBuffer& operator=(OutputBuffer&&) = default;

    [[nodiscard]] std::expected<void, Error> write_byte(char byte, int fd) noexcept;
    [[nodiscard]] std::expected<void, Error> flush(int fd = STDOUT_FILENO) noexcept;
    void set_unbuffered(bool unbuffered) noexcept { unbuffered_ = unbuffered; }

private:
    std::array<char, BUF_SIZE> buffer_{};
    std::size_t pos_{0};
    bool unbuffered_{false};
};

class FileDescriptor {
public:
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    explicit FileDescriptor(std::string_view filename) noexcept 
        : fd_(::open(filename.data(), O_RDONLY)) {}
    
    ~FileDescriptor() { 
        if (fd_ >= 0 && fd_ != STDIN_FILENO && fd_ != STDOUT_FILENO && fd_ != STDERR_FILENO) {
            ::close(fd_);
        }
    }
    
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            std::swap(fd_, other.fd_);
        }
        return *this;
    }

    [[nodiscard]] bool is_valid() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_{-1};
};

[[nodiscard]] std::expected<void, Error> copy_file(FileDescriptor& input, FileDescriptor& output, OutputBuffer& buffer) noexcept;

int main(int argc, char *argv[]) {
    // Parse options and copy each file to standard output.

    int k = 1;            // Start of file arguments
    std::string_view p{}; // Potential '-u' flag

    if (argc >= 2) {
        p = argv[1];
        if (p == "-u") {
            unbuffered = 1;
            k = 2;
        }
    }

    if (k >= argc) {
        // No files specified; default to standard input
        copyfile(0, 1);
        flush();
        return 0;
    }

    for (i = k; i < argc; i++) {
        std::string_view file = argv[i];
        int fd1;
        if (file == "-") {
            fd1 = 0; // Use standard input
        } else {
            fd1 = open(argv[i], 0);
            if (fd1 < 0) {
                std_err("cat: cannot open ");
                std_err(argv[i]);
                std_err("\n");
                continue;
            }
        }
        copyfile(fd1, 1);
        if (fd1 != 0)
            close(fd1);
    }
    flush();
    return 0;
}

static void copyfile(int fd1, int fd2) {
    /*
     * Read data from fd1 and write it to fd2.  When running in buffered
     * mode the output is collected in 'buffer' and only written when the
     * buffer is full.  In unbuffered mode each read result is written
     * immediately.
     */
    int n, j, m;
    std::array<char, BLOCK_SIZE> buf{};

    while (1) {
        n = read(fd1, buf.data(), BLOCK_SIZE);
        if (n < 0)
            quit();
        if (n == 0)
            return;
        if (unbuffered) {
            m = write(fd2, buf.data(), n);
            if (m != n)
                quit();
        } else {
            for (j = 0; j < n; j++) {
                *next++ = buf[j];
                if (next == buffer.data() + BUF_SIZE) {
                    m = write(fd2, buffer.data(), BUF_SIZE);
                    if (m != BUF_SIZE)
                        quit();
                    next = buffer.data();
                }
            }
        }
    }
}

static void flush(void) {
    /* Write any buffered output to standard output. */
    if (next != buffer.data())
        if (write(1, buffer.data(), next - buffer.data()) <= 0)
            quit();
}

static void quit(void) {
    /* Terminate the program after printing the error stored in errno. */
    perror("cat");
    exit(1);
}
