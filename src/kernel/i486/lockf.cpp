#include "lockf.hpp"

namespace xinim::i486::lockf {
namespace {

constexpr uint32_t kMaxLocks = 32U;

struct LockEntry {
    bool in_use;
    int global_fd;
    uint32_t pid;
    int16_t type;      // kFRdlck, kFWrlck, or whole-file (flock)
    int32_t start;
    int32_t end;       // 0x7FFFFFFF = EOF
    bool whole_file;   // true = BSD flock (not POSIX record lock)
};

LockEntry g_locks[kMaxLocks]{};

LockEntry* find_lock(int fd, uint32_t pid, bool whole_file) noexcept {
    for (auto& e : g_locks) {
        if (e.in_use && e.global_fd == fd && e.pid == pid &&
            e.whole_file == whole_file) {
            return &e;
        }
    }
    return nullptr;
}

LockEntry* alloc_lock() noexcept {
    for (auto& e : g_locks) {
        if (!e.in_use) {
            e = {};
            e.in_use = true;
            return &e;
        }
    }
    return nullptr;
}

bool conflicts(const LockEntry& held, int16_t req_type,
               int32_t req_start, int32_t req_end, uint32_t req_pid) noexcept {
    if (!held.in_use) {
        return false;
    }
    if (held.pid == req_pid) {
        return false; // own locks don't conflict
    }
    // Read locks don't conflict with read locks
    if (held.type == kFRdlck && req_type == kFRdlck) {
        return false;
    }
    // Check range overlap
    if (held.whole_file) {
        return true; // whole-file lock always overlaps
    }
    if (req_start > held.end || req_end < held.start) {
        return false;
    }
    return true;
}

bool any_conflict(int fd, int16_t type, int32_t start, int32_t end,
                  uint32_t pid, LockEntry** blocker) noexcept {
    for (auto& e : g_locks) {
        if (e.in_use && e.global_fd == fd && conflicts(e, type, start, end, pid)) {
            if (blocker != nullptr) {
                *blocker = &e;
            }
            return true;
        }
    }
    return false;
}

} // namespace

int do_flock(int global_fd, int operation, uint32_t pid) noexcept {
    if (operation & kLockUn) {
        LockEntry* e = find_lock(global_fd, pid, true);
        if (e != nullptr) {
            e->in_use = false;
        }
        return 0;
    }

    const int16_t type = (operation & kLockEx) ? kFWrlck : kFRdlck;
    const bool nonblock = (operation & kLockNb) != 0;

    // Check for conflicts
    if (any_conflict(global_fd, type, 0, 0x7FFFFFFF, pid, nullptr)) {
        if (nonblock) {
            return -11; // EAGAIN
        }
        return -11; // Would block (no sleep support)
    }

    // Upgrade existing lock or create new
    LockEntry* e = find_lock(global_fd, pid, true);
    if (e != nullptr) {
        e->type = type;
        return 0;
    }

    e = alloc_lock();
    if (e == nullptr) {
        return -12; // ENOMEM
    }
    e->global_fd = global_fd;
    e->pid = pid;
    e->type = type;
    e->start = 0;
    e->end = 0x7FFFFFFF;
    e->whole_file = true;
    return 0;
}

int do_fcntl_lock(int global_fd, int cmd, Flock32* fl, uint32_t pid) noexcept {
    if (fl == nullptr) {
        return -14; // EFAULT
    }

    const int32_t start = fl->l_start;
    const int32_t end = (fl->l_len == 0) ? 0x7FFFFFFF : (start + fl->l_len - 1);

    if (cmd == kFGetlk) {
        LockEntry* blocker = nullptr;
        if (any_conflict(global_fd, fl->l_type, start, end, pid, &blocker)) {
            fl->l_type = blocker->type;
            fl->l_start = blocker->start;
            fl->l_len = (blocker->end == 0x7FFFFFFF) ? 0
                         : (blocker->end - blocker->start + 1);
            fl->l_pid = static_cast<int32_t>(blocker->pid);
        } else {
            fl->l_type = kFUnlck;
        }
        return 0;
    }

    // F_SETLK or F_SETLKW
    if (fl->l_type == kFUnlck) {
        // Remove matching locks
        for (auto& e : g_locks) {
            if (e.in_use && e.global_fd == global_fd && e.pid == pid &&
                !e.whole_file && e.start <= end && e.end >= start) {
                e.in_use = false;
            }
        }
        return 0;
    }

    // Check for conflicts
    if (any_conflict(global_fd, fl->l_type, start, end, pid, nullptr)) {
        if (cmd == kFSetlk) {
            return -11; // EAGAIN
        }
        return -11; // F_SETLKW would block (no sleep support)
    }

    // Set lock (upgrade existing or create)
    for (auto& e : g_locks) {
        if (e.in_use && e.global_fd == global_fd && e.pid == pid &&
            !e.whole_file && e.start == start && e.end == end) {
            e.type = fl->l_type;
            return 0;
        }
    }

    LockEntry* e = alloc_lock();
    if (e == nullptr) {
        return -12;
    }
    e->global_fd = global_fd;
    e->pid = pid;
    e->type = fl->l_type;
    e->start = start;
    e->end = end;
    e->whole_file = false;
    return 0;
}

void release_locks_for_fd(int global_fd, uint32_t pid) noexcept {
    for (auto& e : g_locks) {
        if (e.in_use && e.global_fd == global_fd && e.pid == pid) {
            e.in_use = false;
        }
    }
}

} // namespace xinim::i486::lockf
