/* This file handles the 4 system calls that get and set uids and gids.
 * It also handles getpid().  The code for each one is so tiny that it hardly
 * seemed worthwhile to make each a separate function.
 */

#include "../h/callnr.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include <cstdint> // For uint16_t, uint8_t
#include <cstddef> // For std::size_t (not directly used but good include)

/*===========================================================================*
 *				do_getset				     *
 *===========================================================================*/
PUBLIC int do_getset() noexcept { // Added noexcept
    /* Handle GETUID, GETGID, GETPID, SETUID, SETGID.  The three GETs return
     * their primary results in 'r'.  GETUID and GETGID also return secondary
     * results (the effective IDs) in 'result2', which is returned to the user.
     */

    register struct mproc *rmp = mp;
    register int r;

    switch (mm_call) {
    case GETUID:
        // rmp->mp_realuid and mp_effuid are uid (uint16_t). r and result2 are int. Implicit promotion is fine.
        r = rmp->mp_realuid;
        result2 = rmp->mp_effuid;
        break;

    case GETGID:
        // rmp->mp_realgid and mp_effgid are gid (uint8_t). r and result2 are int. Implicit promotion is fine.
        r = rmp->mp_realgid;
        result2 = rmp->mp_effgid;
        break;

    case GETPID:
        // mp_pid is int. r and result2 are int. Fine.
        r = mproc[who].mp_pid;
        result2 = mproc[rmp->mp_parent].mp_pid;
        break;

    case SETUID:
        // usr_id is a macro for mm_in.m1_i1() (int). rmp->mp_realuid and mp_effuid are uid (uint16_t).
        // SUPER_USER is uid (uint16_t).
        if (rmp->mp_realuid != static_cast<uid>(usr_id) && rmp->mp_effuid != SUPER_USER)
            return (ErrorCode::EPERM);
        rmp->mp_realuid = static_cast<uid>(usr_id);
        rmp->mp_effuid = static_cast<uid>(usr_id);
        tell_fs(SETUID, who, usr_id, usr_id); // tell_fs likely expects int for uid/gid. usr_id is int.
        r = OK;
        break;

    case SETGID:
        // grpid is a macro for (gid)mm_in.m1_i1() (effectively uint8_t). rmp->mp_realgid and mp_effgid are gid (uint8_t).
        // SUPER_USER is uid (uint16_t), comparison with mp_effuid (uint16_t) is fine.
        if (rmp->mp_realgid != grpid && rmp->mp_effuid != SUPER_USER) // Implicit promotion of uint8_t to int for comparison
            return (ErrorCode::EPERM);
        rmp->mp_realgid = grpid;
        rmp->mp_effgid = grpid;
        tell_fs(SETGID, who, static_cast<int>(grpid), static_cast<int>(grpid)); // tell_fs likely expects int. grpid (uint8_t) promotes.
        r = OK;
        break;
    }

    return (r);
}
