#include "procfs.hpp"

#include "ring3_internal.hpp"
#include "kutil.hpp"

namespace xinim::i486::procfs {
namespace {

using UserspaceStat = ::xinim::userland::UserspaceStat;
using ring3::Process;
using ring3::ProcessState;
using ring3::g_processes;
using ring3::g_current_process;
using ring3::kMaxProcesses;

bool str_eq(const char* a, const char* b) noexcept {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        ++a; ++b;
    }
    return *a == *b;
}

bool starts_with(const char* s, const char* prefix) noexcept {
    while (*prefix != '\0') {
        if (*s != *prefix) {
            return false;
        }
        ++s; ++prefix;
    }
    return true;
}

uint32_t parse_uint(const char* s, const char** end) noexcept {
    uint32_t v = 0U;
    while (*s >= '0' && *s <= '9') {
        v = v * 10U + static_cast<uint32_t>(*s - '0');
        ++s;
    }
    if (end != nullptr) {
        *end = s;
    }
    return v;
}

uint32_t uint_to_str(uint32_t v, char* buf, uint32_t cap) noexcept {
    char tmp[12]{};
    uint32_t n = 0U;
    if (v == 0U) {
        tmp[n++] = '0';
    } else {
        while (v > 0U && n < 11U) {
            tmp[n++] = static_cast<char>('0' + (v % 10U));
            v /= 10U;
        }
    }
    uint32_t written = 0U;
    for (uint32_t i = n; i > 0U && written + 1U < cap; --i) {
        buf[written++] = tmp[i - 1U];
    }
    if (written < cap) {
        buf[written] = '\0';
    }
    return written;
}

// Resolve /proc path -> (pid, subpath).
// /proc/self/* maps to current pid.
// /proc/PID/*  maps to that pid.
// Returns the Process* or nullptr.
Process* resolve_proc_path(const char* path, const char** subpath) noexcept {
    if (path == nullptr || path[0] != '/') {
        return nullptr;
    }
    ++path; // skip leading '/'

    uint32_t pid = 0U;
    if (starts_with(path, "self")) {
        pid = (g_current_process != nullptr) ? g_current_process->pid : 1U;
        path += 4U;
    } else if (path[0] >= '0' && path[0] <= '9') {
        const char* end = nullptr;
        pid = parse_uint(path, &end);
        path = end;
    } else {
        return nullptr;
    }

    if (*path == '/') {
        ++path;
    }
    if (subpath != nullptr) {
        *subpath = path;
    }

    for (auto& p : g_processes) {
        if (p.in_use && p.pid == pid) {
            return &p;
        }
    }
    return nullptr;
}

// Per-process virtual file slots
constexpr int kMaxProcFds = 8;
struct ProcFd {
    bool in_use;
    uint32_t pid;
    char content[512];
    uint32_t content_len;
    uint32_t read_pos;
};
ProcFd g_proc_fds[kMaxProcFds]{};

int alloc_proc_fd() noexcept {
    for (int i = 0; i < kMaxProcFds; ++i) {
        if (!g_proc_fds[i].in_use) {
            g_proc_fds[i] = {};
            g_proc_fds[i].in_use = true;
            return i;
        }
    }
    return -1;
}

void generate_status(Process* proc, char* buf, uint32_t cap, uint32_t& len) noexcept {
    len = 0U;
    auto append = [&](const char *s) {
        while (*s != '\0' && len + 1U < cap) {
            buf[len++] = *s++;
        }
    };
    auto append_uint = [&](uint32_t v) {
        char tmp[12]{};
        const uint32_t n = uint_to_str(v, tmp, sizeof(tmp));
        for (uint32_t i = 0U; i < n && len + 1U < cap; ++i) {
            buf[len++] = tmp[i];
        }
    };

    append("Name:\tinit\n");
    append("Pid:\t"); append_uint(proc->pid); append("\n");
    append("PPid:\t"); append_uint(proc->ppid); append("\n");
    append("Uid:\t"); append_uint(proc->cred.uid); append("\t");
    append_uint(proc->cred.euid); append("\t");
    append_uint(proc->cred.suid); append("\t");
    append_uint(proc->cred.uid); append("\n");
    append("Gid:\t"); append_uint(proc->cred.gid); append("\t");
    append_uint(proc->cred.egid); append("\t");
    append_uint(proc->cred.sgid); append("\t");
    append_uint(proc->cred.gid); append("\n");
    const char* state_str = "R (running)";
    if (proc->state == ProcessState::Waiting) {
        state_str = "S (sleeping)";
    } else if (proc->state == ProcessState::Exited) {
        state_str = "Z (zombie)";
    }
    append("State:\t"); append(state_str); append("\n");
    if (len < cap) {
        buf[len] = '\0';
    }
}

// -- VfsOps implementation ------------------------------------------------

int procfs_open(const char* path, uint32_t /*flags*/, uint32_t /*mode*/) noexcept {
    const char* sub = nullptr;
    Process* proc = resolve_proc_path(path, &sub);
    if (proc == nullptr) {
        return -2; // ENOENT
    }

    int slot = alloc_proc_fd();
    if (slot < 0) {
        return -12; // ENOMEM
    }

    auto& fd = g_proc_fds[slot];
    fd.pid = proc->pid;

    if (str_eq(sub, "status")) {
        generate_status(proc, fd.content, sizeof(fd.content), fd.content_len);
    } else if (str_eq(sub, "exe")) {
        // Symlink content -- return the executable path recorded at last execve
        const char* exe = (proc->exe_path[0] != '\0') ? proc->exe_path : "/";
        uint32_t n = 0U;
        while (exe[n] != '\0' && n + 1U < sizeof(fd.content)) {
            fd.content[n] = exe[n]; ++n;
        }
        fd.content[n] = '\0';
        fd.content_len = n;
    } else if (str_eq(sub, "cmdline")) {
        // NUL-separated argv -- simplified
        const char* cmd = "init";
        uint32_t n = 0U;
        while (cmd[n] != '\0' && n + 1U < sizeof(fd.content)) {
            fd.content[n] = cmd[n]; ++n;
        }
        fd.content[n] = '\0';
        fd.content_len = n + 1U; // include trailing NUL
    } else {
        fd.in_use = false;
        return -2; // ENOENT
    }
    return slot;
}

int procfs_read(int slot, void* buf, uint32_t count) noexcept {
    if (slot < 0 || slot >= kMaxProcFds || !g_proc_fds[slot].in_use) {
        return -9;
    }
    auto& fd = g_proc_fds[slot];
    if (fd.read_pos >= fd.content_len) {
        return 0; // EOF
    }

    uint32_t avail = fd.content_len - fd.read_pos;
    if (count > avail) {
        count = avail;
    }
    auto* dst = static_cast<uint8_t*>(buf);
    for (uint32_t i = 0U; i < count; ++i) {
        dst[i] = static_cast<uint8_t>(fd.content[fd.read_pos + i]);
    }
    fd.read_pos += count;
    return static_cast<int>(count);
}

int procfs_write(int /*slot*/, const void* /*buf*/, uint32_t /*count*/) noexcept {
    return -30; // EROFS
}

int procfs_close(int slot) noexcept {
    if (slot >= 0 && slot < kMaxProcFds) {
        g_proc_fds[slot].in_use = false;
    }
    return 0;
}

int procfs_stat(const char* path, UserspaceStat* buf) noexcept {
    const char* sub = nullptr;
    Process* proc = resolve_proc_path(path, &sub);

    *buf = {};
    buf->st_dev = 3U; // procfs device
    buf->st_mode = 0444U; // read-only

    if (proc == nullptr) {
        // Maybe it's /proc itself
        if (str_eq(path, "/") || str_eq(path, "")) {
            buf->st_mode = 040555U; // directory
            buf->st_ino = 1U;
            return 0;
        }
        return -2;
    }

    // /proc/PID is a directory
    if (sub[0] == '\0') {
        buf->st_mode = 040555U;
        buf->st_ino = proc->pid + 100U;
        return 0;
    }

    if (str_eq(sub, "status") || str_eq(sub, "cmdline")) {
        buf->st_mode = 0100444U; // regular file, read-only
        buf->st_ino = proc->pid * 10U + 1U;
        buf->st_size = 128U;
        return 0;
    }
    if (str_eq(sub, "exe")) {
        buf->st_mode = 0120777U; // symlink
        buf->st_ino = proc->pid * 10U + 2U;
        return 0;
    }

    return -2;
}

int procfs_access(const char* path) noexcept {
    UserspaceStat st{};
    return procfs_stat(path, &st);
}

int procfs_readlink(const char* path, char* buf, uint32_t size) noexcept {
    const char* sub = nullptr;
    Process* proc = resolve_proc_path(path, &sub);
    if (proc == nullptr) {
        return -2;
    }
    if (!str_eq(sub, "exe")) {
        return -22;
    }

    const char* exe = (proc->exe_path[0] != '\0') ? proc->exe_path : "/";
    uint32_t n = 0U;
    while (exe[n] != '\0' && n + 1U < size) {
        buf[n] = exe[n]; ++n;
    }
    buf[n] = '\0';
    return static_cast<int>(n);
}

vfs::VfsOps g_procfs_ops = {
    procfs_open,
    procfs_read,
    procfs_write,
    procfs_close,
    procfs_stat,
    nullptr, // mkdir
    nullptr, // unlink
    nullptr, // rename
    nullptr, // readdir
    nullptr, // chmod
    nullptr, // symlink
    procfs_readlink,
    nullptr, // rmdir
    procfs_access,
    nullptr, // truncate
};

} // namespace

vfs::VfsOps* ops() noexcept {
    return &g_procfs_ops;
}

} // namespace xinim::i486::procfs
