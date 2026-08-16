#include "src/kernel/fd_table.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <print>

namespace {

    using xinim::kernel::FdFlags;
    using xinim::kernel::FileDescriptorTable;
    using xinim::kernel::MAX_FDS_PER_PROCESS;

    bool expect(bool condition, const char *message, int &failures) {
        if (!condition) {
            std::println(std::cerr, "FAIL: {}", message);
            ++failures;
            return false;
        }
        return true;
    }

} // namespace

int main() {
    int failures = 0;

    FileDescriptorTable table{};
    table.initialize();
    expect(table.count_open_fds() == 0, "initialize should clear the table", failures);

    const int fd0 = table.allocate_fd();
    const int fd1 = table.allocate_fd();
    expect(fd0 == 0, "first allocation should return fd 0", failures);
    expect(fd1 == 1, "second allocation should return fd 1", failures);
    expect(table.count_open_fds() == 2, "two fds should be open", failures);

    auto *entry0 = table.get_fd(fd0);
    expect(entry0 != nullptr, "allocated fd should be accessible", failures);
    if (entry0 != nullptr) {
        entry0->inode = reinterpret_cast<void *>(0x1234);
        entry0->offset = 99;
        entry0->flags = static_cast<uint32_t>(FdFlags::CLOEXEC);
    }

    const int dupfd = table.dup_fd(fd0, -1);
    expect(dupfd == 2, "dup_fd should allocate the next free fd", failures);
    auto *dup_entry = table.get_fd(dupfd);
    expect(dup_entry != nullptr, "duplicated fd should exist", failures);
    if (dup_entry != nullptr && entry0 != nullptr) {
        expect(dup_entry->inode == entry0->inode, "dup_fd should preserve inode pointer", failures);
        expect(dup_entry->offset == entry0->offset, "dup_fd should preserve file offset", failures);
        expect(dup_entry->flags == 0, "dup_fd should clear cloexec flags on the new fd", failures);
    }
    expect(table.dup_fd(fd0, fd0) == fd0,
           "dup_fd to the same descriptor should preserve the source", failures);
    expect(table.is_valid_fd(fd0), "dup_fd to the same descriptor should leave it open", failures);
    entry0 = table.get_fd(fd0);
    if (entry0 != nullptr) {
        expect(entry0->inode == reinterpret_cast<void *>(0x1234),
               "dup_fd to the same descriptor should preserve its description", failures);
    }

    expect(table.close_fd(fd1) == 0, "close_fd should close an allocated fd", failures);
    expect(table.get_fd(fd1) == nullptr, "get_fd should reject a closed descriptor", failures);
    expect(table.close_fd(fd1) == -EBADF, "close_fd should reject an already closed descriptor",
           failures);
    expect(table.allocate_specific_fd(10) == 10, "allocate_specific_fd should honor requested fd",
           failures);
    expect(table.close_fd(-1) == -EBADF, "close_fd should reject invalid negative fd", failures);
    expect(table.allocate_specific_fd(-1) == -EBADF,
           "allocate_specific_fd should reject a negative descriptor", failures);
    expect(table.allocate_specific_fd(static_cast<int>(MAX_FDS_PER_PROCESS)) == -EBADF,
           "allocate_specific_fd should reject an out-of-range descriptor", failures);
    expect(table.dup_fd(fd0, static_cast<int>(MAX_FDS_PER_PROCESS)) == -EBADF,
           "dup_fd should reject an out-of-range destination", failures);

    FileDescriptorTable invalid_source_table{};
    invalid_source_table.initialize();
    const int closed_source = invalid_source_table.allocate_fd();
    const int preserved_target = invalid_source_table.allocate_specific_fd(7);
    auto *preserved_entry = invalid_source_table.get_fd(preserved_target);
    expect(preserved_entry != nullptr,
           "specific target should be allocated before the failed duplicate", failures);
    if (preserved_entry != nullptr) {
        preserved_entry->inode = reinterpret_cast<void *>(0x5678);
    }
    expect(invalid_source_table.close_fd(closed_source) == 0,
           "source setup should close the descriptor", failures);
    expect(invalid_source_table.dup_fd(closed_source, preserved_target) == -EBADF,
           "dup_fd should reject a closed source", failures);
    preserved_entry = invalid_source_table.get_fd(preserved_target);
    expect(preserved_entry != nullptr, "a failed duplicate should preserve the target descriptor",
           failures);
    if (preserved_entry != nullptr) {
        expect(preserved_entry->inode == reinterpret_cast<void *>(0x5678),
               "a failed duplicate should preserve the target description", failures);
    }

    FileDescriptorTable child{};
    expect(table.clone_to(&child) == 0, "clone_to should succeed", failures);
    expect(child.is_valid_fd(fd0), "clone_to should copy open fds", failures);
    expect(child.is_valid_fd(dupfd), "clone_to should copy duplicated fds", failures);
    expect(child.is_valid_fd(10), "clone_to should copy specifically allocated fds", failures);

    table.close_on_exec();
    expect(!table.is_valid_fd(fd0), "close_on_exec should close CLOEXEC fds", failures);
    expect(table.is_valid_fd(dupfd), "close_on_exec should preserve non-CLOEXEC dup fd", failures);

    if (failures != 0) {
        std::println(std::cerr, "{} kernel fd_table test(s) failed.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "ALL kernel fd_table tests passed.");
    return EXIT_SUCCESS;
}
