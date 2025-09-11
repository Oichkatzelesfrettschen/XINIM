/**
 * @file time.cpp
 * @brief Implementation of time-related system calls for the file system server.
 *
 * This translation unit contains implementations of the utime, time, stime, and
 * times system calls. The logic has been modernized to employ C++23 features.
 */

#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "type.hpp"
#include <array>

namespace minix::fs {
namespace {
/** @brief Scratch message for clock task communication. */
message clock_mess{};
} // namespace

/**
 * @brief Handle the utime system call.
 * @return ::OK on success or an error code.
 */
[[nodiscard]] auto do_utime() -> int {
    extern struct inode *eat_path();
    struct inode *rip{};
    auto r = int{};

    if (fetch_name(utime_file, utime_length, M1) != OK)
        return err_code;
    if ((rip = eat_path(user_path)) == NIL_INODE)
        return err_code;

    r = OK;
    if (rip->i_uid != fp->fp_effuid && !super_user)
        r = ErrorCode::EPERM;
    if (r == OK) {
        rip->i_modtime = update_time;
        rip->i_dirt = DIRTY;
    }
    put_inode(rip);
    return r;
}

/**
 * @brief Handle the time system call.
 * @return ::OK on success.
 */
[[nodiscard]] auto do_time() -> int {
    extern real_time clock_time();
    reply_l1 = clock_time();
    return OK;
}

/**
 * @brief Handle the stime system call.
 * @return ::OK on success or an error code.
 */
[[nodiscard]] auto do_stime() -> int {
    if (!super_user)
        return ErrorCode::EPERM;
    clock_mess.m_type = SET_TIME;
    new_time(clock_mess) = static_cast<long>(tp);
    if (auto k = sendrec(CLOCK, &clock_mess); k != OK) {
        panic("do_stime error", k);
    }
    return OK;
}

/**
 * @brief Handle the times system call.
 * @return ::OK on success.
 */
[[nodiscard]] auto do_tims() -> int {
    std::array<real_time, 4> t{};
    sys_times(who, t.data());
    reply_t1 = t[0];
    reply_t2 = t[1];
    reply_t3 = t[2];
    reply_t4 = t[3];
    return OK;
}

} // namespace minix::fs
