#pragma once

#include "stream.hpp"

namespace minix::io {

class FileStream : public Stream {
public:
    explicit FileStream(int fd, bool write) : fd_(fd), writable_(write) {}
    ~FileStream() override = default;

    Result<size_t> read(std::span<std::byte> buffer) override;
    Result<size_t> write(std::span<const std::byte> buffer) override;
    std::error_code close() override;
    int descriptor() const override { return fd_; }

private:
    int fd_{-1};
    bool writable_{false};
};

} // namespace minix::io
