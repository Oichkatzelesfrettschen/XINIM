#include "../include/lib.hpp" // C++23 header
#include "../include/stdio.hpp"
#include <cerrno>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {
constexpr auto kFileMode = 0644;

[[nodiscard]] std::expected<int, std::errc> open_file(const std::filesystem::path &path,
                                                      std::string_view mode) noexcept {
    int fd = -1;
    if (mode == "w") {
        fd = ::creat(path.c_str(), kFileMode);
    } else if (mode == "a") {
        fd = ::open(path.c_str(), O_WRONLY | O_CREAT, kFileMode);
        if (fd >= 0) {
            ::lseek(fd, 0L, SEEK_END);
        }
    } else if (mode == "r") {
        fd = ::open(path.c_str(), O_RDONLY);
    } else {
        return std::unexpected(std::errc::invalid_argument);
    }
    if (fd < 0) {
        return std::unexpected(static_cast<std::errc>(errno));
    }
    return fd;
}
} // namespace

[[nodiscard]] FILE *fopen(const char *name, const char *mode) {
    int i = 0;
    for (; _io_table[i] != nullptr; ++i) {
        if (i >= NFILES) {
            return nullptr;
        }
    }

    const std::filesystem::path path{name};
    const std::string_view mode_sv{mode ? mode : ""};
    const int flags = (mode_sv == "r") ? READMODE : WRITEMODE;

    auto fd_res = open_file(path, mode_sv);
    if (!fd_res) {
        return nullptr;
    }

    auto fp = std::make_unique<FILE>();
    auto buffer = std::make_unique<char[]>(BUFSIZ);

    fp->_count = 0;
    fp->_fd = fd_res.value();
    fp->_flags = flags | IOMYBUF;
    fp->_buf = buffer.get();
    fp->_ptr = fp->_buf;

    _io_table[i] = fp.get();
    buffer.release();
    return fp.release();
}
