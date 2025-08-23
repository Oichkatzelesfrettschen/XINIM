// Simple MemoryStream unit test

#include "xinim/io/memory_stream.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>

int main() {
    minix::io::MemoryStream stream;
    const char *msg = "Hello MemoryStream!";
    auto write_res = stream.write(reinterpret_cast<const std::byte *>(msg), std::strlen(msg));
    if (!write_res || *write_res != std::strlen(msg)) {
        std::cout << "write failed\n";
        return 1;
    }

    stream.seek(0);

    std::array<std::byte, 64> buf{};
    auto read_res = stream.read(buf.data(), std::strlen(msg));
    if (!read_res || *read_res != std::strlen(msg)) {
        std::cout << "read failed\n";
        return 1;
    }

    std::string_view result(reinterpret_cast<const char *>(buf.data()), std::strlen(msg));
    if (result != msg) {
        std::cout << "content mismatch\n";
        return 1;
    }

    std::cout << "MemoryStream test passed!\n";
    return 0;
}
