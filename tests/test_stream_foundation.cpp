// tests/test_stream_foundation.cpp
// Minimal Stream architecture verification

#include "xinim/io/file_operations.hpp" // open_stream declarations
#include "xinim/io/file_stream.hpp"     // FileStream definition
#include "xinim/io/standard_streams.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

// Helper for printing detailed failure messages
static void report_error(const char *name, const std::error_code &ec) {
    std::cerr << "\n" << name << " failed: " << ec.message() << '\n';
}

int main() {
    std::cout << "=== MINIX Stream Foundation Verification ===\n";

    // Test 1: Write to stdout using Stream interface
    std::cout << "Test 1: stdout write... ";
    {
        const char *msg = "Hello from MINIX Stream!\n";
        auto res = minix::io::stdout().write(std::as_bytes(std::span{msg, std::strlen(msg)}));
        if (!res) {
            report_error("stdout write", res.error);
            return 1;
        }
        if (*res != std::strlen(msg)) {
            std::cerr << "wrong byte count\n";
            return 1;
        }
        std::cout << "OK" << std::endl;
    }

    // Test 2: Create a file and write data
    std::cout << "Test 2: create file... ";
    {
        auto file = minix::io::open_stream("stream_test.txt", minix::io::OpenMode::write |
                                                                  minix::io::OpenMode::create |
                                                                  minix::io::OpenMode::truncate);
        if (!file) {
            report_error("file create", file.error);
            return 1;
        }
        const char *content = "This file validates the Stream implementation.\n";
        auto *s = file.value.value().get();
        auto wr = s->write(std::as_bytes(std::span{content, std::strlen(content)}));
        if (!wr) {
            report_error("file write", wr.error);
            return 1;
        }
        std::cout << "OK" << std::endl;
    }

    // Test 3: Read file back and verify contents
    std::cout << "Test 3: read file... ";
    {
        auto file = minix::io::open_stream("stream_test.txt", minix::io::OpenMode::read);
        if (!file) {
            report_error("file open", file.error);
            return 1;
        }
        auto *s = file.value.value().get();
        std::array<std::byte, 256> buf{};
        auto rd = s->read(buf);
        if (!rd) {
            report_error("file read", rd.error);
            return 1;
        }
        std::string_view data(reinterpret_cast<const char *>(buf.data()), *rd);
        if (data != "This file validates the Stream implementation.\n") {
            std::cerr << "mismatched file content\n";
            return 1;
        }
        std::cout << "OK" << std::endl;
    }

    // Test 4: Opening a non-existent file should return error
    std::cout << "Test 4: missing file error... ";
    {
        auto file = minix::io::open_stream("no_such_file.abc", minix::io::OpenMode::read);
        if (file) {
            std::cerr << "unexpected success\n";
            return 1;
        }
        if (file.error != std::errc::no_such_file_or_directory) {
            std::cerr << "wrong error code\n";
            return 1;
        }
        std::cout << "OK" << std::endl;
    }

    std::remove("stream_test.txt");
    std::cout << "\nAll Stream foundation tests passed!\n";
    return 0;
}
