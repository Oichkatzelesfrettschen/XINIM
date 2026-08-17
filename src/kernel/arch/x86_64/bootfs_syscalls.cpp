#include "bootfs_syscalls.hpp"

#include "../../i486/bootfs.hpp"
#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../uaccess.hpp"
#include "process_syscalls.hpp"
#include "serial_terminal.hpp"
#include "socket_syscalls.hpp"
#include "userspace_abi.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" void *malloc(size_t size);
extern "C" void free(void *pointer);

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr size_t kPathCapacity = 256U;
        constexpr size_t kTransferCapacity = bootfs::kPipeBufferSize;
        constexpr int64_t kBadAddress = -14;
        constexpr uint32_t kCloseOnExec = 0x80000U;
        constexpr uint32_t kDescriptorCloseOnExec = 1U;
        constexpr uint32_t kStatusAppend = 0x0400U;
        constexpr uint32_t kStatusNonBlock = 0x0800U;
        constexpr uint32_t kStatusAccessMode = 0x0003U;
        constexpr uint32_t kStatusReadOnly = 0x0000U;
        constexpr int kFcntlDuplicate = 0;
        constexpr int kFcntlGetDescriptorFlags = 1;
        constexpr int kFcntlSetDescriptorFlags = 2;
        constexpr int kFcntlGetStatusFlags = 3;
        constexpr int kFcntlSetStatusFlags = 4;
        constexpr uint8_t kDirectoryType = 4U;
        constexpr uint8_t kRegularType = 8U;
        constexpr uint8_t kSymbolicLinkType = 10U;
        constexpr size_t kMaximumDirectoryRecordSize = 288U;

        struct BootfsOpenDescription {
            int backend_descriptor;
            size_t reference_count;
            uint32_t status_flags;
            int lock_mode;
        };

        struct DirectoryReadContext {
            const char *directory_path;
            uintptr_t user_buffer;
            uint32_t capacity;
            uint32_t bytes_written;
            uint64_t current_cookie;
            uint64_t next_cookie;
            bool use_dirent64;
            bool copy_failed;
        };

        uint8_t g_terminal_input;
        uint8_t g_terminal_output;

        [[nodiscard]] size_t string_length(const char *text) noexcept {
            size_t length = 0U;
            while (text[length] != '\0') {
                ++length;
            }
            return length;
        }

        [[nodiscard]] ProcessControlBlock *current_process() noexcept {
            return get_current_process();
        }

        [[nodiscard]] bool decode_ownership_id(int64_t raw_id, int64_t &decoded_id) noexcept {
            if (raw_id == -1 || raw_id == static_cast<int64_t>(UINT32_MAX)) {
                decoded_id = -1;
                return true;
            }
            if (raw_id < 0 || raw_id >= static_cast<int64_t>(UINT32_MAX)) {
                return false;
            }
            decoded_id = raw_id;
            return true;
        }

        [[nodiscard]] BootfsOpenDescription *open_description(FileDescriptor &descriptor) noexcept {
            return static_cast<BootfsOpenDescription *>(descriptor.private_data);
        }

        [[nodiscard]] const BootfsOpenDescription *
        open_description(const FileDescriptor &descriptor) noexcept {
            return static_cast<const BootfsOpenDescription *>(descriptor.private_data);
        }

        [[nodiscard]] bool is_terminal_input(const FileDescriptor &descriptor) noexcept {
            return descriptor.inode == &g_terminal_input;
        }

        [[nodiscard]] bool is_terminal_output(const FileDescriptor &descriptor) noexcept {
            return descriptor.inode == &g_terminal_output;
        }

        [[nodiscard]] int close_process_descriptor(ProcessControlBlock &process,
                                                   int descriptor_number) noexcept {
            if (!process.fd_table.is_valid_fd(descriptor_number)) {
                return -EBADF;
            }
            FileDescriptor *descriptor = process.fd_table.get_fd(descriptor_number);

            int close_result = 0;
            if (socket_descriptor_is_socket(*descriptor)) {
                socket_descriptor_release(*descriptor);
            } else {
                BootfsOpenDescription *description = open_description(*descriptor);
                if (description != nullptr) {
                    if (description->reference_count == 0U) {
                        return -EIO;
                    }
                    --description->reference_count;
                    if (description->reference_count == 0U) {
                        if (description->lock_mode != 0) {
                            static_cast<void>(bootfs::update_file_lock(
                                description->backend_descriptor,
                                reinterpret_cast<uintptr_t>(description), description->lock_mode,
                                0));
                            description->lock_mode = 0;
                            wake_io_waiters();
                        }
                        close_result = bootfs::close(description->backend_descriptor);
                        free(description);
                    }
                }
            }
            const int descriptor_close_result = process.fd_table.close_fd(descriptor_number);
            wake_io_waiters();
            return descriptor_close_result == 0 ? close_result : -EBADF;
        }

        [[nodiscard]] int install_backend_descriptor(ProcessControlBlock &process,
                                                     int backend_descriptor,
                                                     uint32_t flags) noexcept {
            const int descriptor_number = process.fd_table.allocate_fd();
            if (descriptor_number < 0 ||
                static_cast<uint64_t>(descriptor_number) >= process.descriptor_limit) {
                if (descriptor_number >= 0) {
                    static_cast<void>(process.fd_table.close_fd(descriptor_number));
                }
                bootfs::close(backend_descriptor);
                return descriptor_number < 0 ? descriptor_number : -EMFILE;
            }

            auto *description =
                static_cast<BootfsOpenDescription *>(malloc(sizeof(BootfsOpenDescription)));
            if (description == nullptr) {
                process.fd_table.close_fd(descriptor_number);
                bootfs::close(backend_descriptor);
                return -ENOMEM;
            }
            description->backend_descriptor = backend_descriptor;
            description->reference_count = 1U;
            description->status_flags = flags;
            description->lock_mode = 0;

            FileDescriptor *descriptor = process.fd_table.get_fd(descriptor_number);
            descriptor->flags =
                (flags & kCloseOnExec) != 0U ? static_cast<uint32_t>(FdFlags::CLOEXEC) : 0U;
            descriptor->file_flags = flags;
            descriptor->private_data = description;
            return descriptor_number;
        }

        [[nodiscard]] bool extract_child_name(const char *directory_path, const char *entry_path,
                                              char (&name)[kPathCapacity]) noexcept {
            if (directory_path == nullptr || entry_path == nullptr || directory_path[0] != '/' ||
                entry_path[0] != '/') {
                return false;
            }
            size_t directory_length = string_length(directory_path);
            const char *remainder = entry_path;
            if (directory_length == 1U) {
                ++remainder;
            } else {
                for (size_t index = 0U; index < directory_length; ++index) {
                    if (entry_path[index] != directory_path[index]) {
                        return false;
                    }
                }
                if (entry_path[directory_length] != '/') {
                    return false;
                }
                remainder += directory_length + 1U;
            }
            if (remainder[0] == '\0') {
                return false;
            }
            size_t length = 0U;
            while (remainder[length] != '\0' && remainder[length] != '/') {
                if (length + 1U >= sizeof(name)) {
                    return false;
                }
                name[length] = remainder[length];
                ++length;
            }
            if (remainder[length] == '/' && remainder[length + 1U] != '\0') {
                return false;
            }
            name[length] = '\0';
            return length != 0U;
        }

        [[nodiscard]] bool emit_directory_record(DirectoryReadContext &context, const char *name,
                                                 uint8_t type) noexcept {
            uint8_t record[kMaximumDirectoryRecordSize]{};
            size_t record_size = 0U;
            if (!serialize_directory_record(record, sizeof(record), name,
                                            context.current_cookie + 1U,
                                            static_cast<int64_t>(context.current_cookie + 1U), type,
                                            context.use_dirent64, record_size)) {
                context.copy_failed = true;
                return false;
            }
            if (record_size > context.capacity - context.bytes_written) {
                return false;
            }
            if (copy_to_user(context.user_buffer + context.bytes_written, record, record_size) !=
                0) {
                context.copy_failed = true;
                return false;
            }
            context.bytes_written += static_cast<uint32_t>(record_size);
            context.next_cookie = context.current_cookie + 1U;
            return true;
        }

        [[nodiscard]] bool visit_directory_entry(const bootfs::FileRecord &file,
                                                 void *opaque_context) noexcept {
            auto *context = static_cast<DirectoryReadContext *>(opaque_context);
            char name[kPathCapacity]{};
            if (context == nullptr ||
                !extract_child_name(context->directory_path, file.path, name)) {
                return true;
            }
            const uint64_t entry_cookie = context->current_cookie++;
            if (entry_cookie < context->next_cookie) {
                return true;
            }
            context->current_cookie = entry_cookie;
            const uint8_t entry_type = file.is_symlink
                                           ? kSymbolicLinkType
                                           : (file.is_directory ? kDirectoryType : kRegularType);
            if (!emit_directory_record(*context, name, entry_type)) {
                return false;
            }
            context->current_cookie = entry_cookie + 1U;
            return true;
        }

        [[nodiscard]] bootfs::FileStatus terminal_status() noexcept {
            bootfs::FileStatus record{};
            record.mode = 0020000U | 0666U;
            record.link_count = 1U;
            record.block_size = 512;
            return record;
        }

        [[nodiscard]] bool copy_path(uintptr_t user_path, char (&path)[kPathCapacity]) noexcept {
            const ProcessControlBlock *process = current_process();
            if (process == nullptr) {
                return false;
            }
            char input[kPathCapacity]{};
            if (copy_string_from_user(input, user_path, sizeof(input)) != 0) {
                return false;
            }
            if (input[0] == '/') {
                for (size_t index = 0U; index < sizeof(path); ++index) {
                    path[index] = input[index];
                    if (input[index] == '\0') {
                        return true;
                    }
                }
                return false;
            }

            size_t output = 0U;
            for (; process->current_directory[output] != '\0' && output + 1U < sizeof(path);
                 ++output) {
                path[output] = process->current_directory[output];
            }
            if (output == 0U || path[output - 1U] != '/') {
                if (output + 1U >= sizeof(path)) {
                    return false;
                }
                path[output++] = '/';
            }
            for (size_t input_index = 0U; input[input_index] != '\0'; ++input_index) {
                if (output + 1U >= sizeof(path)) {
                    return false;
                }
                path[output++] = input[input_index];
            }
            path[output] = '\0';
            return true;
        }

    } // namespace

    void bootfs_initialize_process(ProcessControlBlock &process) noexcept {
        process.fd_table.initialize();
        process.current_directory.fill('\0');
        process.current_directory[0] = '/';

        const int input = process.fd_table.allocate_fd();
        const int output = process.fd_table.allocate_fd();
        const int error = process.fd_table.allocate_fd();
        if (input == 0) {
            FileDescriptor *descriptor = process.fd_table.get_fd(input);
            descriptor->inode = &g_terminal_input;
            descriptor->file_flags = 0U;
        }
        if (output == 1) {
            FileDescriptor *descriptor = process.fd_table.get_fd(output);
            descriptor->inode = &g_terminal_output;
            descriptor->file_flags = 1U;
        }
        if (error == 2) {
            FileDescriptor *descriptor = process.fd_table.get_fd(error);
            descriptor->inode = &g_terminal_output;
            descriptor->file_flags = 1U;
        }
    }

    void bootfs_clone_process(const ProcessControlBlock &parent,
                              ProcessControlBlock &child) noexcept {
        parent.fd_table.clone_to(&child.fd_table);
        child.current_directory = parent.current_directory;
        for (size_t index = 0U; index < MAX_FDS_PER_PROCESS; ++index) {
            if (child.fd_table.is_valid_fd(static_cast<int>(index))) {
                FileDescriptor *descriptor = child.fd_table.get_fd(static_cast<int>(index));
                if (socket_descriptor_is_socket(*descriptor)) {
                    socket_descriptor_retain(*descriptor);
                } else {
                    BootfsOpenDescription *description = open_description(*descriptor);
                    if (description != nullptr) {
                        ++description->reference_count;
                    }
                }
            }
        }
    }

    void bootfs_release_process(ProcessControlBlock &process) noexcept {
        for (size_t index = 0U; index < MAX_FDS_PER_PROCESS; ++index) {
            if (process.fd_table.is_valid_fd(static_cast<int>(index))) {
                static_cast<void>(close_process_descriptor(process, static_cast<int>(index)));
            }
        }
    }

    void bootfs_close_on_exec(ProcessControlBlock &process) noexcept {
        for (size_t index = 0U; index < MAX_FDS_PER_PROCESS; ++index) {
            const int descriptor_number = static_cast<int>(index);
            if (process.fd_table.is_valid_fd(descriptor_number)) {
                const FileDescriptor *descriptor = process.fd_table.get_fd(descriptor_number);
                if ((descriptor->flags & static_cast<uint32_t>(FdFlags::CLOEXEC)) != 0U) {
                    static_cast<void>(close_process_descriptor(process, descriptor_number));
                }
            }
        }
    }

    int64_t bootfs_open(uintptr_t pathname, uint32_t flags, uint32_t mode) noexcept {
        ProcessControlBlock *process = current_process();
        char path[kPathCapacity]{};
        if (process == nullptr || !copy_path(pathname, path)) {
            return kBadAddress;
        }
        const int backend_descriptor = bootfs::open(path, flags, mode);
        return backend_descriptor >= 0
                   ? install_backend_descriptor(*process, backend_descriptor, flags)
                   : backend_descriptor;
    }

    int64_t bootfs_close(int descriptor) noexcept {
        ProcessControlBlock *process = current_process();
        return process != nullptr ? close_process_descriptor(*process, descriptor) : -EBADF;
    }

    int64_t bootfs_read(int descriptor, uintptr_t buffer, uint32_t count) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return socket_read(descriptor, buffer, count);
        }
        if (is_terminal_input(*process_descriptor)) {
            return serial_terminal_read(0U, buffer, count);
        }
        if (is_terminal_output(*process_descriptor)) {
            return -EBADF;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        if (description == nullptr) {
            return -EBADF;
        }
        if (!is_user_address(buffer, count)) {
            return kBadAddress;
        }
        uint8_t transfer[kTransferCapacity]{};
        uint32_t total = 0U;
        while (total < count) {
            const uint32_t remaining = count - total;
            const uint32_t chunk = remaining < kTransferCapacity
                                       ? remaining
                                       : static_cast<uint32_t>(kTransferCapacity);
            const int read_count = bootfs::read(description->backend_descriptor, transfer, chunk);
            if (read_count == bootfs::kReadWouldBlock) {
                if (total != 0U) {
                    return total;
                }
                process_block_for_io();
            }
            if (read_count < 0) {
                return total != 0U ? total : read_count;
            }
            if (read_count == 0) {
                break;
            }
            if (copy_to_user(buffer + total, transfer, static_cast<size_t>(read_count)) != 0) {
                return kBadAddress;
            }
            total += static_cast<uint32_t>(read_count);
            if (read_count > 0) {
                wake_io_waiters();
            }
            if (static_cast<uint32_t>(read_count) < chunk) {
                break;
            }
        }
        return total;
    }

    int64_t bootfs_write(int descriptor, uintptr_t buffer, uint32_t count) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return socket_write(descriptor, buffer, count);
        }
        if (is_terminal_output(*process_descriptor)) {
            return serial_terminal_write(1U, buffer, count);
        }
        if (is_terminal_input(*process_descriptor)) {
            return -EBADF;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        if (description == nullptr) {
            return -EBADF;
        }
        if (!is_user_address(buffer, count)) {
            return kBadAddress;
        }
        uint8_t transfer[kTransferCapacity]{};
        uint32_t total = 0U;
        while (total < count) {
            const uint32_t remaining = count - total;
            const uint32_t chunk = remaining < kTransferCapacity
                                       ? remaining
                                       : static_cast<uint32_t>(kTransferCapacity);
            if (copy_from_user(transfer, buffer + total, chunk) != 0) {
                return kBadAddress;
            }
            const int write_count = bootfs::write(description->backend_descriptor, transfer, chunk);
            if (write_count == bootfs::kWriteWouldBlock) {
                if (total != 0U) {
                    return total;
                }
                process_block_for_io();
            }
            if (write_count < 0) {
                return total != 0U ? total : write_count;
            }
            total += static_cast<uint32_t>(write_count);
            if (write_count > 0) {
                wake_io_waiters();
            }
            if (static_cast<uint32_t>(write_count) < chunk) {
                break;
            }
        }
        return total;
    }

    int64_t bootfs_seek(int descriptor, int64_t offset, int whence) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return -ESPIPE;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        return description != nullptr
                   ? bootfs::seek(description->backend_descriptor, offset, whence)
                   : -ESPIPE;
    }

    int64_t bootfs_access(uintptr_t pathname) noexcept {
        char path[kPathCapacity]{};
        return copy_path(pathname, path) ? bootfs::access(path) : kBadAddress;
    }

    int64_t bootfs_chdir(uintptr_t pathname) noexcept {
        ProcessControlBlock *process = current_process();
        char path[kPathCapacity]{};
        if (process == nullptr || !copy_path(pathname, path)) {
            return kBadAddress;
        }
        if (!bootfs::is_directory(path)) {
            return -2;
        }
        const size_t length = string_length(path);
        for (size_t index = 0U; index <= length; ++index) {
            process->current_directory[index] = path[index];
        }
        return 0;
    }

    int64_t bootfs_getcwd(uintptr_t buffer, uint32_t size) noexcept {
        const ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return kBadAddress;
        }
        const size_t required = string_length(process->current_directory.data()) + 1U;
        if (size < required ||
            copy_to_user(buffer, process->current_directory.data(), required) != 0) {
            return kBadAddress;
        }
        return static_cast<int64_t>(required);
    }

    static int64_t duplicate_process_descriptor(int old_descriptor, int new_descriptor) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -EBADF;
        }
        if (!process->fd_table.is_valid_fd(old_descriptor)) {
            return -EBADF;
        }
        FileDescriptor *source = process->fd_table.get_fd(old_descriptor);
        if (old_descriptor == new_descriptor) {
            return old_descriptor;
        }
        if (new_descriptor < -1 || new_descriptor >= static_cast<int>(MAX_FDS_PER_PROCESS)) {
            return -EBADF;
        }
        if (new_descriptor >= 0 && process->fd_table.is_valid_fd(new_descriptor)) {
            static_cast<void>(close_process_descriptor(*process, new_descriptor));
        }

        const int duplicate = new_descriptor >= 0
                                  ? process->fd_table.allocate_specific_fd(new_descriptor)
                                  : process->fd_table.allocate_fd();
        if (duplicate < 0) {
            return duplicate;
        }
        FileDescriptor *destination = process->fd_table.get_fd(duplicate);
        *destination = *source;
        destination->flags = 0U;
        if (socket_descriptor_is_socket(*destination)) {
            socket_descriptor_retain(*destination);
        } else {
            BootfsOpenDescription *description = open_description(*destination);
            if (description != nullptr) {
                ++description->reference_count;
            }
        }
        return duplicate;
    }

    int64_t bootfs_duplicate(int old_descriptor) noexcept {
        return duplicate_process_descriptor(old_descriptor, -1);
    }

    int64_t bootfs_duplicate_to(int old_descriptor, int new_descriptor) noexcept {
        if (new_descriptor < 0) {
            return -EBADF;
        }
        return duplicate_process_descriptor(old_descriptor, new_descriptor);
    }

    int64_t bootfs_fcntl(int descriptor, int command, uint64_t argument) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }

        switch (command) {
        case kFcntlDuplicate: {
            if (argument >= MAX_FDS_PER_PROCESS) {
                return -EINVAL;
            }
            const int descriptor_limit = static_cast<int>(process->descriptor_limit);
            for (int candidate = static_cast<int>(argument); candidate < descriptor_limit;
                 ++candidate) {
                if (!process->fd_table.is_valid_fd(candidate)) {
                    return duplicate_process_descriptor(descriptor, candidate);
                }
            }
            return -EMFILE;
        }
        case kFcntlGetDescriptorFlags:
            return (process_descriptor->flags & static_cast<uint32_t>(FdFlags::CLOEXEC)) != 0U
                       ? static_cast<int64_t>(kDescriptorCloseOnExec)
                       : 0;
        case kFcntlSetDescriptorFlags:
            process_descriptor->flags = (argument & kDescriptorCloseOnExec) != 0U
                                            ? static_cast<uint32_t>(FdFlags::CLOEXEC)
                                            : 0U;
            return 0;
        case kFcntlGetStatusFlags: {
            if (socket_descriptor_is_socket(*process_descriptor)) {
                return process_descriptor->file_flags;
            }
            const BootfsOpenDescription *description = open_description(*process_descriptor);
            return description != nullptr ? description->status_flags
                                          : process_descriptor->file_flags;
        }
        case kFcntlSetStatusFlags: {
            const uint32_t mutable_flags =
                static_cast<uint32_t>(argument) & (kStatusAppend | kStatusNonBlock);
            if (socket_descriptor_is_socket(*process_descriptor)) {
                process_descriptor->file_flags =
                    (process_descriptor->file_flags & ~kStatusNonBlock) | mutable_flags;
                return 0;
            }
            BootfsOpenDescription *description = open_description(*process_descriptor);
            if (description == nullptr) {
                process_descriptor->file_flags =
                    (process_descriptor->file_flags & 0x3U) | mutable_flags;
                return 0;
            }
            const uint32_t new_flags =
                (description->status_flags & ~(kStatusAppend | kStatusNonBlock)) | mutable_flags;
            if (bootfs::set_status_flags(description->backend_descriptor,
                                         static_cast<int>(new_flags)) != 0) {
                return -EINVAL;
            }
            description->status_flags = new_flags;
            return 0;
        }
        default:
            return -EINVAL;
        }
    }

    int64_t bootfs_ioctl(int descriptor, uint64_t request, uintptr_t argument) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (!is_terminal_input(*process_descriptor) && !is_terminal_output(*process_descriptor)) {
            return -ENOTTY;
        }
        return serial_terminal_ioctl(request, argument);
    }

    bool bootfs_descriptor_read_ready(int descriptor) noexcept {
        ProcessControlBlock *process = current_process();
        const FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return false;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return socket_descriptor_read_ready(descriptor);
        }
        if (is_terminal_input(*process_descriptor)) {
            return serial_terminal_has_input();
        }
        if (is_terminal_output(*process_descriptor)) {
            return false;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        return description != nullptr &&
               bootfs::can_read_without_block(description->backend_descriptor);
    }

    bool bootfs_descriptor_write_ready(int descriptor) noexcept {
        ProcessControlBlock *process = current_process();
        const FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return false;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return socket_descriptor_write_ready(descriptor);
        }
        if (is_terminal_output(*process_descriptor)) {
            return true;
        }
        if (is_terminal_input(*process_descriptor)) {
            return false;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        return description != nullptr &&
               bootfs::can_write_without_block(description->backend_descriptor);
    }

    int64_t bootfs_pipe(uintptr_t descriptors) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -EBADF;
        }
        int backend_descriptors[2]{};
        const int result = bootfs::make_pipe(backend_descriptors);
        if (result != 0) {
            return result;
        }
        int process_descriptors[2]{};
        process_descriptors[0] = install_backend_descriptor(*process, backend_descriptors[0], 0U);
        if (process_descriptors[0] < 0) {
            bootfs::close(backend_descriptors[1]);
            return process_descriptors[0];
        }
        process_descriptors[1] = install_backend_descriptor(*process, backend_descriptors[1], 1U);
        if (process_descriptors[1] < 0) {
            static_cast<void>(close_process_descriptor(*process, process_descriptors[0]));
            return process_descriptors[1];
        }
        if (copy_to_user(descriptors, process_descriptors, sizeof(process_descriptors)) != 0) {
            static_cast<void>(close_process_descriptor(*process, process_descriptors[0]));
            static_cast<void>(close_process_descriptor(*process, process_descriptors[1]));
            return kBadAddress;
        }
        return 0;
    }

    int64_t bootfs_stat(uintptr_t pathname, uintptr_t output) noexcept {
        char path[kPathCapacity]{};
        bootfs::FileStatus record{};
        if (!copy_path(pathname, path)) {
            return kBadAddress;
        }
        const int result = bootfs::status_path(path, &record);
        if (result != 0) {
            return -ENOENT;
        }
        const UserspaceStat64 serialized = serialize_stat64(record);
        return copy_to_user(output, &serialized, sizeof(serialized)) == 0 ? 0 : kBadAddress;
    }

    int64_t bootfs_lstat(uintptr_t pathname, uintptr_t output) noexcept {
        char path[kPathCapacity]{};
        bootfs::FileStatus record{};
        if (!copy_path(pathname, path)) {
            return kBadAddress;
        }
        if (bootfs::status_path_no_follow(path, &record) != 0) {
            return -ENOENT;
        }
        const UserspaceStat64 serialized = serialize_stat64(record);
        return copy_to_user(output, &serialized, sizeof(serialized)) == 0 ? 0 : kBadAddress;
    }

    int64_t bootfs_symlink(uintptr_t target, uintptr_t link_pathname) noexcept {
        char raw_target[kPathCapacity]{};
        char link_path[kPathCapacity]{};
        if (copy_string_from_user(raw_target, target, sizeof(raw_target)) != 0 ||
            !copy_path(link_pathname, link_path)) {
            return kBadAddress;
        }
        bootfs::FileStatus existing{};
        if (bootfs::status_path_no_follow(link_path, &existing) == 0) {
            return -EEXIST;
        }
        return bootfs::create_symbolic_link(raw_target, link_path) == 0 ? 0 : -ENOENT;
    }

    int64_t bootfs_readlink(uintptr_t pathname, uintptr_t output, uint32_t capacity) noexcept {
        if (capacity == 0U) {
            return -EINVAL;
        }
        char path[kPathCapacity]{};
        if (!copy_path(pathname, path)) {
            return kBadAddress;
        }
        char target[kPathCapacity]{};
        const uint32_t transfer_capacity = capacity < static_cast<uint32_t>(sizeof(target))
                                               ? capacity
                                               : static_cast<uint32_t>(sizeof(target));
        const int result = bootfs::read_symbolic_link(path, target, transfer_capacity);
        if (result < 0) {
            return -EINVAL;
        }
        return copy_to_user(output, target, static_cast<size_t>(result)) == 0 ? result
                                                                              : kBadAddress;
    }

    int64_t bootfs_fstat(int descriptor, uintptr_t output) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return -EBADF;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        bootfs::FileStatus record{};
        int result = 0;
        if (description != nullptr) {
            result = bootfs::status_fd(description->backend_descriptor, &record);
        } else if (is_terminal_input(*process_descriptor) ||
                   is_terminal_output(*process_descriptor)) {
            record = terminal_status();
        } else {
            result = -1;
        }
        if (result != 0) {
            return -EBADF;
        }
        const UserspaceStat64 serialized = serialize_stat64(record);
        return copy_to_user(output, &serialized, sizeof(serialized)) == 0 ? 0 : kBadAddress;
    }

    int64_t bootfs_getdents(int descriptor, uintptr_t output, uint32_t count,
                            bool use_dirent64) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return -ENOTDIR;
        }
        BootfsOpenDescription *description = open_description(*process_descriptor);
        if (description == nullptr) {
            return -ENOTDIR;
        }
        const char *directory_path = bootfs::directory_path_for_fd(description->backend_descriptor);
        if (directory_path == nullptr) {
            return -ENOTDIR;
        }
        if (count < directory_record_size(1U, use_dirent64)) {
            return -EINVAL;
        }
        const int64_t starting_cookie = bootfs::seek(description->backend_descriptor, 0, 1);
        if (starting_cookie < 0) {
            return -EIO;
        }

        DirectoryReadContext context{
            directory_path, output, count, 0U, 0U, static_cast<uint64_t>(starting_cookie),
            use_dirent64,   false,
        };
        const char *dot_names[2] = {".", ".."};
        for (uint64_t cookie = 0U; cookie < 2U; ++cookie) {
            context.current_cookie = cookie;
            if (cookie < context.next_cookie) {
                continue;
            }
            if (!emit_directory_record(context, dot_names[cookie], kDirectoryType)) {
                break;
            }
        }
        if (!context.copy_failed && context.bytes_written < count) {
            context.current_cookie = 2U;
            bootfs::for_each_entry(visit_directory_entry, &context);
        }
        if (context.copy_failed && context.bytes_written == 0U) {
            return kBadAddress;
        }
        if (bootfs::seek(description->backend_descriptor, static_cast<int64_t>(context.next_cookie),
                         0) < 0) {
            return context.bytes_written != 0U ? context.bytes_written : -EIO;
        }
        return context.bytes_written;
    }

    int64_t bootfs_mkdir(uintptr_t pathname, uint32_t mode) noexcept {
        char path[kPathCapacity]{};
        return copy_path(pathname, path) ? bootfs::mkdir(path, mode) : kBadAddress;
    }

    int64_t bootfs_unlink(uintptr_t pathname) noexcept {
        char path[kPathCapacity]{};
        return copy_path(pathname, path) ? bootfs::unlink(path) : kBadAddress;
    }

    int64_t bootfs_rmdir(uintptr_t pathname) noexcept {
        char path[kPathCapacity]{};
        return copy_path(pathname, path) ? bootfs::rmdir(path) : kBadAddress;
    }

    int64_t bootfs_rename(uintptr_t old_pathname, uintptr_t new_pathname) noexcept {
        char old_path[kPathCapacity]{};
        char new_path[kPathCapacity]{};
        return copy_path(old_pathname, old_path) && copy_path(new_pathname, new_path)
                   ? bootfs::rename(old_path, new_path)
                   : kBadAddress;
    }

    int64_t bootfs_truncate(uintptr_t pathname, int64_t length) noexcept {
        char path[kPathCapacity]{};
        if (!copy_path(pathname, path)) {
            return kBadAddress;
        }
        if (length < 0) {
            return -EINVAL;
        }
        if (static_cast<uint64_t>(length) > UINT32_MAX) {
            return -EFBIG;
        }
        bootfs::FileStatus status{};
        if (bootfs::status_path(path, &status) != 0) {
            return -ENOENT;
        }
        if ((status.mode & 0170000U) == 0040000U) {
            return -EISDIR;
        }
        return bootfs::truncate_path(path, static_cast<uint64_t>(length)) == 0 ? 0 : -EIO;
    }

    int64_t bootfs_ftruncate(int descriptor, int64_t length) noexcept {
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor == nullptr) {
            return -EBADF;
        }
        if (length < 0) {
            return -EINVAL;
        }
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return -EBADF;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        if (description == nullptr) {
            return -EBADF;
        }
        if ((description->status_flags & kStatusAccessMode) == kStatusReadOnly) {
            return -EBADF;
        }
        if (static_cast<uint64_t>(length) > UINT32_MAX) {
            return -EFBIG;
        }
        bootfs::FileStatus status{};
        if (bootfs::status_fd(description->backend_descriptor, &status) != 0) {
            return -EBADF;
        }
        if ((status.mode & 0170000U) == 0040000U) {
            return -EISDIR;
        }
        return bootfs::truncate_fd(description->backend_descriptor,
                                   static_cast<uint64_t>(length)) == 0
                   ? 0
                   : -EIO;
    }

    int64_t bootfs_chown(uintptr_t pathname, int64_t user_id, int64_t group_id) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (process->effective_user_id != 0U) {
            return -EPERM;
        }
        int64_t decoded_user_id = 0;
        int64_t decoded_group_id = 0;
        if (!decode_ownership_id(user_id, decoded_user_id) ||
            !decode_ownership_id(group_id, decoded_group_id)) {
            return -EINVAL;
        }
        char path[kPathCapacity]{};
        if (!copy_path(pathname, path)) {
            return kBadAddress;
        }
        return bootfs::chown_path(path, decoded_user_id, decoded_group_id) == 0 ? 0 : -ENOENT;
    }

    int64_t bootfs_fchown(int descriptor, int64_t user_id, int64_t group_id) noexcept {
        ProcessControlBlock *process = current_process();
        if (process == nullptr || !process->fd_table.is_valid_fd(descriptor)) {
            return -EBADF;
        }
        if (process->effective_user_id != 0U) {
            return -EPERM;
        }
        int64_t decoded_user_id = 0;
        int64_t decoded_group_id = 0;
        if (!decode_ownership_id(user_id, decoded_user_id) ||
            !decode_ownership_id(group_id, decoded_group_id)) {
            return -EINVAL;
        }
        const FileDescriptor *process_descriptor = process->fd_table.get_fd(descriptor);
        if (socket_descriptor_is_socket(*process_descriptor)) {
            return -EBADF;
        }
        const BootfsOpenDescription *description = open_description(*process_descriptor);
        return description != nullptr && bootfs::chown_fd(description->backend_descriptor,
                                                          decoded_user_id, decoded_group_id) == 0
                   ? 0
                   : -EINVAL;
    }

    int64_t bootfs_flock(int descriptor, int operation) noexcept {
        constexpr int kLockShared = 1;
        constexpr int kLockExclusive = 2;
        constexpr int kLockNonBlock = 4;
        constexpr int kLockUnlock = 8;
        const int operation_without_nonblock = operation & ~kLockNonBlock;
        if (operation_without_nonblock != kLockShared &&
            operation_without_nonblock != kLockExclusive &&
            operation_without_nonblock != kLockUnlock) {
            return -EINVAL;
        }
        ProcessControlBlock *process = current_process();
        FileDescriptor *process_descriptor =
            process != nullptr && process->fd_table.is_valid_fd(descriptor)
                ? process->fd_table.get_fd(descriptor)
                : nullptr;
        if (process_descriptor != nullptr && socket_descriptor_is_socket(*process_descriptor)) {
            return -EBADF;
        }
        BootfsOpenDescription *description =
            process_descriptor != nullptr ? open_description(*process_descriptor) : nullptr;
        if (description == nullptr) {
            return -EBADF;
        }
        const int requested_mode =
            operation_without_nonblock == kLockUnlock ? 0 : operation_without_nonblock;
        const int result = bootfs::update_file_lock(description->backend_descriptor,
                                                    reinterpret_cast<uintptr_t>(description),
                                                    description->lock_mode, requested_mode);
        if (result == bootfs::kLockWouldBlock) {
            if ((operation & kLockNonBlock) != 0) {
                return -EWOULDBLOCK;
            }
            process_block_for_io();
        }
        if (result != 0) {
            return -EINVAL;
        }
        description->lock_mode = requested_mode;
        if (requested_mode == 0) {
            wake_io_waiters();
        }
        return 0;
    }

} // namespace xinim::kernel::x86_64
