/**
 * @file
 * @brief Handle simple get and set system calls for user and group IDs.
 *
 * This translation unit manages the five system calls that either obtain or
 * modify user and group identifiers in addition to retrieving process IDs.
 * Historically each case was implemented as a standalone routine, but the
 * logic is compact enough to justify a single function.
 */

#include "sys/callnr.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include <cstddef> // For std::size_t (not directly used but good include)
#include <cstdint> // For uint16_t, uint8_t

/*===========================================================================*
 *				do_getset				     *
 *===========================================================================*/
/**
 * @brief Dispatch basic identity management system calls.
 *
 * The routine accepts no parameters. The requested action is indicated by the
 * global ::mm_call variable. Some operations also place a secondary result in
 * ::result2.
 *
 * @return Integer identifier or ErrorCode depending on the invoked call.
 */
[[nodiscard]] PUBLIC int do_getset() noexcept {
    // Handle GETUID, GETGID, GETPID, SETUID and SETGID. The three GET calls
    // place their primary results in 'r'. GETUID and GETGID additionally store
    // the effective IDs in 'result2'.

    auto *rmp = mp;
    int result{};

    switch (mm_call) {
    case GETUID:
        // rmp->mp_realuid and mp_effuid are uid (uint16_t). r and result2 are int. Implicit
        // promotion is fine.
        result = rmp->mp_realuid;
        result2 = rmp->mp_effuid;
        break;

    case GETGID:
        // rmp->mp_realgid and mp_effgid are gid (uint8_t). r and result2 are int. Implicit
        // promotion is fine.
        result = rmp->mp_realgid;
        result2 = rmp->mp_effgid;
        break;

    case GETPID:
        // mp_pid is int. r and result2 are int. Fine.
        result = mproc[who].mp_pid;
        result2 = mproc[rmp->mp_parent].mp_pid;
        break;

    case SETUID:
        // usr_id is a macro for mm_in.m1_i1() (int). rmp->mp_realuid and mp_effuid are uid
        // (uint16_t). SUPER_USER is uid (uint16_t).
        if (rmp->mp_realuid != static_cast<uid>(usr_id) && rmp->mp_effuid != SUPER_USER)
            return (ErrorCode::EPERM);
        rmp->mp_realuid = static_cast<uid>(usr_id);
        rmp->mp_effuid = static_cast<uid>(usr_id);
        tell_fs(SETUID, who, usr_id,
                usr_id); // tell_fs likely expects int for uid/gid. usr_id is int.
        result = OK;
        break;

    case SETGID:
        // grpid is a macro for (gid)mm_in.m1_i1() (effectively uint8_t). rmp->mp_realgid and
        // mp_effgid are gid (uint8_t). SUPER_USER is uid (uint16_t), comparison with mp_effuid
        // (uint16_t) is fine.
        if (rmp->mp_realgid != grpid &&
            rmp->mp_effuid != SUPER_USER) // Implicit promotion of uint8_t to int for comparison
            return (ErrorCode::EPERM);
        rmp->mp_realgid = grpid;
        rmp->mp_effgid = grpid;
        tell_fs(SETGID, who, static_cast<int>(grpid),
                static_cast<int>(grpid)); // tell_fs likely expects int. grpid (uint8_t) promotes.
        result = OK;
        break;
    }

    return result;
}
