#pragma once

#include <cstdint>

namespace xinim::kernel {
    struct ProcessControlBlock;
}

namespace xinim::kernel::x86_64 {

    void bootfs_initialize_process(ProcessControlBlock &process) noexcept;
    void bootfs_clone_process(const ProcessControlBlock &parent,
                              ProcessControlBlock &child) noexcept;
    void bootfs_release_process(ProcessControlBlock &process) noexcept;
    void bootfs_close_on_exec(ProcessControlBlock &process) noexcept;

    [[nodiscard]] int64_t bootfs_open(uintptr_t pathname, uint32_t flags, uint32_t mode) noexcept;
    [[nodiscard]] int64_t bootfs_close(int descriptor) noexcept;
    [[nodiscard]] int64_t bootfs_read(int descriptor, uintptr_t buffer, uint32_t count) noexcept;
    [[nodiscard]] int64_t bootfs_write(int descriptor, uintptr_t buffer, uint32_t count) noexcept;
    [[nodiscard]] int64_t bootfs_seek(int descriptor, int64_t offset, int whence) noexcept;
    [[nodiscard]] int64_t bootfs_access(uintptr_t pathname) noexcept;
    [[nodiscard]] int64_t bootfs_chdir(uintptr_t pathname) noexcept;
    [[nodiscard]] int64_t bootfs_getcwd(uintptr_t buffer, uint32_t size) noexcept;
    [[nodiscard]] int64_t bootfs_duplicate(int old_descriptor) noexcept;
    [[nodiscard]] int64_t bootfs_duplicate_to(int old_descriptor, int new_descriptor) noexcept;
    [[nodiscard]] int64_t bootfs_fcntl(int descriptor, int command, uint64_t argument) noexcept;
    [[nodiscard]] int64_t bootfs_ioctl(int descriptor, uint64_t request,
                                       uintptr_t argument) noexcept;
    [[nodiscard]] bool bootfs_descriptor_read_ready(int descriptor) noexcept;
    [[nodiscard]] bool bootfs_descriptor_write_ready(int descriptor) noexcept;
    [[nodiscard]] int64_t bootfs_pipe(uintptr_t descriptors) noexcept;
    [[nodiscard]] int64_t bootfs_stat(uintptr_t pathname, uintptr_t output) noexcept;
    [[nodiscard]] int64_t bootfs_lstat(uintptr_t pathname, uintptr_t output) noexcept;
    [[nodiscard]] int64_t bootfs_symlink(uintptr_t target, uintptr_t link_pathname) noexcept;
    [[nodiscard]] int64_t bootfs_readlink(uintptr_t pathname, uintptr_t output,
                                          uint32_t capacity) noexcept;
    [[nodiscard]] int64_t bootfs_fstat(int descriptor, uintptr_t output) noexcept;
    [[nodiscard]] int64_t bootfs_getdents(int descriptor, uintptr_t output, uint32_t count,
                                          bool use_dirent64) noexcept;
    [[nodiscard]] int64_t bootfs_mkdir(uintptr_t pathname, uint32_t mode) noexcept;
    [[nodiscard]] int64_t bootfs_unlink(uintptr_t pathname) noexcept;
    [[nodiscard]] int64_t bootfs_rmdir(uintptr_t pathname) noexcept;
    [[nodiscard]] int64_t bootfs_rename(uintptr_t old_pathname, uintptr_t new_pathname) noexcept;
    [[nodiscard]] int64_t bootfs_truncate(uintptr_t pathname, int64_t length) noexcept;
    [[nodiscard]] int64_t bootfs_ftruncate(int descriptor, int64_t length) noexcept;
    [[nodiscard]] int64_t bootfs_chown(uintptr_t pathname, int64_t user_id,
                                       int64_t group_id) noexcept;
    [[nodiscard]] int64_t bootfs_fchown(int descriptor, int64_t user_id, int64_t group_id) noexcept;
    [[nodiscard]] int64_t bootfs_flock(int descriptor, int operation) noexcept;

} // namespace xinim::kernel::x86_64
