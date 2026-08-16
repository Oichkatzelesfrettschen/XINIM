// Bare-metal runtime support for symbols that a hosted libc or libc++ would
// otherwise provide.

#include "early/serial_16550.hpp"
#include "freestanding_format.hpp"
#include "panic.hpp"
#include "scheduler.hpp"
#include "unified_scheduler.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

// Include message type before C stubs to avoid redefinition conflicts.
// sys/type.hpp defines the message struct used by lattice IPC.
#include "sys/type.hpp"

// VFS server implementation (ADR-0009 bare-metal VFS)
#include "../vfs/vfs_server.hpp"

// Pull in the implementation's FILE type only after project headers that
// define names also exposed as hosted stdio macros.
#include <cstdio>

// Forward declarations for lattice IPC functions (defined in lattice_ipc.cpp).
// Cannot include lattice_ipc.hpp here due to conflicts with the C stubs below.
namespace lattice {
    enum class IpcFlags : uint32_t { NONE = 0 };
    int lattice_send(int src, int dst, const message &msg, IpcFlags flags);
    int lattice_recv(int pid, message *out, IpcFlags flags);
} // namespace lattice

// (STUB_SYS_WRITE and STUB_OK removed: vfs_server_main() now delegates to
//  vfs_server_loop() in src/vfs/vfs_server.cpp which handles all VFS messages.)

extern xinim::early::Serial16550 early_serial;

namespace {

    void write_serial_character(void *, char character) noexcept {
        early_serial.write_char(character);
    }

} // namespace

// ============================================================================
// C library stubs (bare-metal has no libc)
// ============================================================================

extern "C" {

int vsnprintf(char *buffer, size_t size, const char *format, va_list arguments) {
    return xinim::kernel::format::to_buffer(buffer, size, format, arguments);
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}

int vprintf(const char *format, va_list arguments) {
    return xinim::kernel::format::to_sink(write_serial_character, nullptr, format, arguments);
}

int printf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vprintf(format, arguments);
    va_end(arguments);
    return result;
}

int puts(const char *s) {
    early_serial.write(s);
    early_serial.write("\n");
    return 0;
}

int putchar(int c) {
    const auto character = static_cast<unsigned char>(c);
    early_serial.write_char(static_cast<char>(character));
    return static_cast<int>(character);
}

int vfprintf(FILE *, const char *format, va_list arguments) {
    return vprintf(format, arguments);
}

FILE *stdout = nullptr;

char *strstr(const char *haystack, const char *needle) {
    if (!*needle)
        return const_cast<char *>(haystack);
    for (const char *h = haystack; *h; ++h) {
        const char *p = h;
        const char *n = needle;
        while (*p && *n && *p == *n) {
            ++p;
            ++n;
        }
        if (!*n)
            return const_cast<char *>(h);
    }
    return nullptr;
}

// Ring-0 server entry points for requests routed through lattice IPC.

// early_serial write_char: exposed for vfs_server.cpp serial fallback
void early_serial_write_char(char c) {
    early_serial.write_char(c);
}

// VFS server backed by the bare-metal ramfs implementation from ADR-0009.
// vfs_server_init() sets up inodes, dirents, FD table, mount table, root dir.
// vfs_server_loop() is the IPC dispatch loop (blocks on lattice_recv).
void vfs_server_main() {
    early_serial.write("[VFS] Server started (bare-metal ramfs)\n");
    vfs_server_init();
    early_serial.write("[VFS] Init complete: root, /bin, /dev, /proc, /tmp\n");
    vfs_server_loop();
}

void proc_mgr_main() {
    constexpr int PM_PID = 3;
    early_serial.write("[PM] Server started\n");
    for (;;) {
        message msg{};
        int rc = lattice::lattice_recv(PM_PID, &msg, lattice::IpcFlags::NONE);
        if (rc != 0)
            continue;

        message reply{};
        reply.m_type = -1; // Process-manager request is unsupported.

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
        if (rc != 0)
            continue;

        message reply{};
        reply.m_type = -1; // Memory-manager request is unsupported.

        int caller = static_cast<int>(msg.m_u.m_m1.m1i1);
        if (caller > 0)
            lattice::lattice_send(MM_PID, caller, reply, lattice::IpcFlags::NONE);
    }
}

} // extern "C"

namespace std {

    void __glibcxx_assert_fail(const char *, int, const char *, const char *condition) noexcept {
        kpanic(condition != nullptr ? condition : "libstdc++ precondition failure");
    }

} // namespace std

// ============================================================================
// Kernel subsystem stubs
// ============================================================================

namespace net {
    int local_node() {
        return 0;
    }
} // namespace net

// Forward declare the types we need without pulling in complex headers
namespace xinim::kernel {
    struct ProcessControlBlock;
    struct FileDescriptor;
    struct FileDescriptorTable;
} // namespace xinim::kernel

// These definitions must match the declarations in the headers.
// Using the actual headers to ensure ABI compatibility.
#include "fd_table.hpp"
#include "pcb.hpp"
#include "vfs_interface.hpp"

namespace xinim::kernel {

    // VFS lookup returns no legacy object; callers use the ramfs server.
    void *vfs_lookup(const char *) {
        return nullptr;
    }

    // scheduler_add_process() is defined by the unified scheduler path.

} // namespace xinim::kernel

namespace xinim::time {
    void monotonic_install(uint64_t (*)()) {}
} // namespace xinim::time
