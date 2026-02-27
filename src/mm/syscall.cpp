/**
 * @file syscall.cpp
 * @brief MM wrapper implementations for SYS_* calls and FS notifications.
 */

#include "sys/error.hpp"
#include "syscall.hpp"

[[nodiscard]] int tell_fs(int call, int p1, int p2, int p3) noexcept {
    message m{};
    m.m_type = call;
    m.m1_i1() = p1;
    m.m1_i2() = p2;
    m.m1_i3() = p3;
    return sendrec(FS_PROC_NR, &m);
}

[[nodiscard]] int sys_fork(int parent, int child, int child_pid, std::uint64_t token) noexcept {
    message m{};
    m.m_type = SYS_FORK;
    proc1(m) = parent;
    proc2(m) = child;
    ::pid(m) = child_pid;
    set_token(m, token);
    return sendrec(SYSTASK, &m);
}

[[nodiscard]] int sys_newmap(int proc_nr, const mem_map *map) noexcept {
    message m{};
    m.m_type = SYS_NEWMAP;
    proc1(m) = proc_nr;
    mem_ptr(m) = reinterpret_cast<char *>(const_cast<mem_map *>(map));
    return sendrec(SYSTASK, &m);
}

[[nodiscard]] int sys_exec(int proc_nr, std::size_t new_sp, std::uint64_t token) noexcept {
    message m{};
    m.m_type = SYS_EXEC;
    proc1(m) = proc_nr;
    stack_ptr(m) = reinterpret_cast<char *>(static_cast<uintptr_t>(new_sp));
    set_token(m, token);
    return sendrec(SYSTASK, &m);
}

[[nodiscard]] int sys_xit(int parent, int proc) noexcept {
    message m{};
    m.m_type = SYS_XIT;
    proc1(m) = parent;
    proc2(m) = proc;
    return sendrec(SYSTASK, &m);
}

[[nodiscard]] int sys_getsp(int proc_nr, std::size_t *new_sp) noexcept {
    message m{};
    m.m_type = SYS_GETSP;
    proc1(m) = proc_nr;
    int r = sendrec(SYSTASK, &m);
    if (r == OK && new_sp != nullptr) {
        *new_sp = reinterpret_cast<std::size_t>(stack_ptr(m));
    }
    return r;
}

[[nodiscard]] int sys_sig(int proc_nr, int sig_nr, int (*handler)(), std::uint64_t token) noexcept {
    message m{};
    m.m_type = SYS_SIG;
    pr(m) = proc_nr;
    signum(m) = sig_nr;
    func(m) = handler;
    set_token(m, token);
    return sendrec(SYSTASK, &m);
}

[[nodiscard]] int sys_copy(message *m_ptr) noexcept {
    if (m_ptr == nullptr) {
        return static_cast<int>(ErrorCode::EFAULT);
    }
    m_ptr->m_type = SYS_COPY;
    return sendrec(SYSTASK, m_ptr);
}

[[nodiscard]] int sys_abort() noexcept {
    message m{};
    m.m_type = SYS_ABORT;
    return sendrec(SYSTASK, &m);
}
