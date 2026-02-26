// Bare-metal linker stubs for freestanding kernel.
// Provides minimal implementations for symbols referenced by kernel code
// but not available without a hosted libc/libc++.
// Each stub is documented with the Phase that will provide the real impl.

#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include "early/serial_16550.hpp"

// Include message type before C stubs to avoid redefinition conflicts.
// sys/type.hpp defines the message struct used by lattice IPC.
#include "sys/type.hpp"

// Forward declarations for lattice IPC functions (defined in lattice_ipc.cpp).
// Cannot include lattice_ipc.hpp here due to conflicts with the C stubs below.
namespace lattice {
    enum class IpcFlags : uint32_t { NONE = 0 };
    int lattice_send(int src, int dst, const message& msg, IpcFlags flags);
    int lattice_recv(int pid, message* out, IpcFlags flags);
}

// Syscall number for SYS_write (matches include/xinim/sys/syscalls.h)
static constexpr int STUB_SYS_WRITE = 6;
static constexpr int STUB_OK = 0;

extern xinim::early::Serial16550 early_serial;

// ============================================================================
// C library stubs (bare-metal has no libc)
// ============================================================================

extern "C" {

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    if (!buf || size == 0) return 0;

    va_list ap;
    va_start(ap, fmt);

    size_t pos = 0;
    auto put = [&](char c) {
        if (pos + 1 < size) buf[pos] = c;
        ++pos;
    };

    auto put_str = [&](const char* s) {
        if (!s) s = "(null)";
        while (*s) put(*s++);
    };

    auto put_uint = [&](uint64_t val, int base, int min_width) {
        char tmp[20];
        int len = 0;
        if (val == 0) {
            tmp[len++] = '0';
        } else {
            while (val > 0) {
                int d = static_cast<int>(val % static_cast<uint64_t>(base));
                tmp[len++] = d < 10 ? static_cast<char>('0' + d)
                                    : static_cast<char>('a' + d - 10);
                val /= static_cast<uint64_t>(base);
            }
        }
        while (len < min_width) tmp[len++] = '0';
        for (int i = len - 1; i >= 0; --i) put(tmp[i]);
    };

    while (*fmt) {
        if (*fmt != '%') { put(*fmt++); continue; }
        ++fmt;

        bool is_long = false;
        if (*fmt == 'l') { is_long = true; ++fmt; }

        switch (*fmt) {
            case 's': put_str(va_arg(ap, const char*)); break;
            case 'd': {
                int64_t v = is_long ? va_arg(ap, long) : va_arg(ap, int);
                if (v < 0) { put('-'); v = -v; }
                put_uint(static_cast<uint64_t>(v), 10, 0);
                break;
            }
            case 'u':
                put_uint(is_long ? va_arg(ap, unsigned long)
                                 : va_arg(ap, unsigned), 10, 0);
                break;
            case 'x':
                put_uint(is_long ? va_arg(ap, unsigned long)
                                 : va_arg(ap, unsigned), 16, 0);
                break;
            case 'p': {
                put('0'); put('x');
                put_uint(reinterpret_cast<uint64_t>(va_arg(ap, void*)), 16, 0);
                break;
            }
            case 'c': put(static_cast<char>(va_arg(ap, int))); break;
            case '%': put('%'); break;
            default: put('%'); put(*fmt); break;
        }
        ++fmt;
    }

    va_end(ap);
    if (pos < size) buf[pos] = '\0';
    else buf[size - 1] = '\0';
    return static_cast<int>(pos);
}

int printf(const char* fmt, ...) {
    // Minimal printf stub: writes format string literally to serial.
    // Full formatting is available via snprintf.
    early_serial.write(fmt);
    return 0;
}

int puts(const char* s) {
    early_serial.write(s);
    early_serial.write("\n");
    return 0;
}

int putchar(int c) {
    early_serial.write_char(static_cast<char>(c));
    return c;
}

int vfprintf(void*, const char* fmt, ...) {
    early_serial.write(fmt);
    return 0;
}

void* stdout = nullptr;

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return const_cast<char*>(haystack);
    for (const char* h = haystack; *h; ++h) {
        const char* p = h;
        const char* n = needle;
        while (*p && *n && *p == *n) { ++p; ++n; }
        if (!*n) return const_cast<char*>(h);
    }
    return nullptr;
}

// Timer and interrupt C handler stubs (assembly calls these)
void timer_interrupt_c_handler() {
    // Phase 5 (P5-T02): will call do_clocktick()
}

void handle_unhandled_interrupt() {
    early_serial.write("[INT] Unhandled interrupt\n");
}

// Server entry points: minimal Ring-0 message loops (Phase 6).
// These handle the syscall types routed to each server via lattice IPC.
// Phase 7 will replace these with full implementations.

// VFS server: handles SYS_read, SYS_write, SYS_open, SYS_close.
// Currently SYS_write to any fd echoes to the serial console.
void vfs_server_main() {
    constexpr int VFS_PID = 2;
    early_serial.write("[VFS] Server started\n");
    for (;;) {
        message msg{};
        int rc = lattice::lattice_recv(VFS_PID, &msg, lattice::IpcFlags::NONE);
        if (rc != 0) continue;

        message reply{};
        reply.m_type = STUB_OK;

        switch (msg.m_type) {
        case STUB_SYS_WRITE: {
            // m1p1 = buf pointer, m1i2 = count
            const char* buf = msg.m_u.m_m1.m1p1;
            int count       = msg.m_u.m_m1.m1i2;
            if (buf && count > 0) {
                for (int i = 0; i < count; ++i)
                    early_serial.write_char(buf[i]);
                reply.m_type = count; // bytes written
            } else {
                reply.m_type = -1;
            }
            break;
        }
        default:
            reply.m_type = -1; // ENOSYS
            break;
        }

        // Reply to caller (m_u.m_m1.m1i1 carries caller PID in our protocol)
        int caller = static_cast<int>(msg.m_u.m_m1.m1i1);
        if (caller > 0)
            lattice::lattice_send(VFS_PID, caller, reply, lattice::IpcFlags::NONE);
    }
}

void proc_mgr_main() {
    constexpr int PM_PID = 3;
    early_serial.write("[PM] Server started\n");
    for (;;) {
        message msg{};
        int rc = lattice::lattice_recv(PM_PID, &msg, lattice::IpcFlags::NONE);
        if (rc != 0) continue;

        message reply{};
        reply.m_type = -1; // All PM calls unimplemented until Phase 7

        int caller = static_cast<int>(msg.m_u.m_m1.m1i1);
        if (caller > 0)
            lattice::lattice_send(PM_PID, caller, reply, lattice::IpcFlags::NONE);
    }
}

void mem_mgr_main() {
    constexpr int MM_PID = 4;
    early_serial.write("[MM] Server started\n");
    for (;;) {
        message msg{};
        int rc = lattice::lattice_recv(MM_PID, &msg, lattice::IpcFlags::NONE);
        if (rc != 0) continue;

        message reply{};
        reply.m_type = -1; // All MM calls unimplemented until Phase 7

        int caller = static_cast<int>(msg.m_u.m_m1.m1i1);
        if (caller > 0)
            lattice::lattice_send(MM_PID, caller, reply, lattice::IpcFlags::NONE);
    }
}

} // extern "C"

// ============================================================================
// Kernel subsystem stubs
// ============================================================================

namespace net {
    int local_node() { return 0; }
}

// Forward declare the types we need without pulling in complex headers
namespace xinim::kernel {
    struct ProcessControlBlock;
    struct FileDescriptor;
    struct FileDescriptorTable;
}

// These definitions must match the declarations in the headers.
// Using the actual headers to ensure ABI compatibility.
#include "fd_table.hpp"
#include "pcb.hpp"
#include "vfs_interface.hpp"

namespace xinim::kernel {

void FileDescriptorTable::initialize() {
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; ++i) {
        fds[i].reset();
    }
    next_fd = 0;
}

int FileDescriptorTable::allocate_fd() {
    for (size_t i = next_fd; i < MAX_FDS_PER_PROCESS; ++i) {
        if (!fds[i].is_open) {
            fds[i].is_open = true;
            next_fd = static_cast<uint32_t>(i + 1);
            return static_cast<int>(i);
        }
    }
    return -1; // EMFILE
}

FileDescriptor* FileDescriptorTable::get_fd(int fd) {
    if (fd < 0 || static_cast<size_t>(fd) >= MAX_FDS_PER_PROCESS) return nullptr;
    return &fds[fd];
}

const FileDescriptor* FileDescriptorTable::get_fd(int fd) const {
    if (fd < 0 || static_cast<size_t>(fd) >= MAX_FDS_PER_PROCESS) return nullptr;
    return &fds[fd];
}

bool FileDescriptorTable::is_valid_fd(int fd) const {
    auto* f = get_fd(fd);
    return f && f->is_open;
}

int FileDescriptorTable::close_fd(int fd) {
    auto* f = get_fd(fd);
    if (!f || !f->is_open) return -1;
    f->reset();
    if (static_cast<uint32_t>(fd) < next_fd) next_fd = static_cast<uint32_t>(fd);
    return 0;
}

int FileDescriptorTable::allocate_specific_fd(int fd) {
    if (fd < 0 || static_cast<size_t>(fd) >= MAX_FDS_PER_PROCESS) return -1;
    if (fds[fd].is_open) return -1;
    fds[fd].is_open = true;
    return fd;
}

int FileDescriptorTable::dup_fd(int oldfd, int newfd) {
    auto* src = get_fd(oldfd);
    if (!src || !src->is_open) return -1;
    if (newfd < 0) {
        newfd = allocate_fd();
        if (newfd < 0) return -1;
    } else {
        if (is_valid_fd(newfd)) close_fd(newfd);
        fds[newfd].is_open = true;
    }
    fds[newfd] = *src;
    return newfd;
}

void FileDescriptorTable::close_on_exec() {
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; ++i) {
        if (fds[i].is_open && (fds[i].flags & static_cast<uint32_t>(FdFlags::CLOEXEC))) {
            fds[i].reset();
        }
    }
}

size_t FileDescriptorTable::count_open_fds() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; ++i) {
        if (fds[i].is_open) ++count;
    }
    return count;
}

int FileDescriptorTable::clone_to(FileDescriptorTable* dest) const {
    if (!dest) return -1;
    for (size_t i = 0; i < MAX_FDS_PER_PROCESS; ++i) {
        dest->fds[i] = fds[i];
    }
    dest->next_fd = next_fd;
    return 0;
}

// Signal state stub (Phase 6)
void init_signal_state(ProcessControlBlock*) {}

// VFS lookup stub (Phase 6)
void* vfs_lookup(const char*) { return nullptr; }

// Scheduler add process stub (Phase 5)
void scheduler_add_process(ProcessControlBlock*) {}

} // namespace xinim::kernel

namespace xinim::time {
    void monotonic_install(uint64_t(*)()) {}
}
