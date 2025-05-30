/* This file handles the 4 system calls that get and set uids and gids.
 * It also handles getpid().  The code for each one is so tiny that it hardly
 * seemed worthwhile to make each a separate function.
 */

#include "../h/callnr.h"
#include "../h/const.h"
#include "../h/error.h"
#include "../h/type.h"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"

/*===========================================================================*
 *				do_getset				     *
 *===========================================================================*/
PUBLIC int do_getset() {
    /* Handle GETUID, GETGID, GETPID, SETUID, SETGID.  The three GETs return
     * their primary results in 'r'.  GETUID and GETGID also return secondary
     * results (the effective IDs) in 'result2', which is returned to the user.
     */

    register struct mproc *rmp = mp;
    register int r;

    switch (mm_call) {
    case static_cast<int>(SysCall::GETUID):
        r = rmp->mp_realuid;
        result2 = rmp->mp_effuid;
        break;

    case static_cast<int>(SysCall::GETGID):
        r = rmp->mp_realgid;
        result2 = rmp->mp_effgid;
        break;

    case static_cast<int>(SysCall::GETPID):
        r = mproc[who].mp_pid;
        result2 = mproc[rmp->mp_parent].mp_pid;
        break;

    case static_cast<int>(SysCall::SETUID):
        if (rmp->mp_realuid != usr_id && rmp->mp_effuid != SUPER_USER)
            return (ErrorCode::EPERM);
        rmp->mp_realuid = usr_id;
        rmp->mp_effuid = usr_id;
        tell_fs(static_cast<int>(SysCall::SETUID), who, usr_id, usr_id);
        r = OK;
        break;

    case static_cast<int>(SysCall::SETGID):
        if (rmp->mp_realgid != grpid && rmp->mp_effuid != SUPER_USER)
            return (ErrorCode::EPERM);
        rmp->mp_realgid = grpid;
        rmp->mp_effgid = grpid;
        tell_fs(static_cast<int>(SysCall::SETGID), who, grpid, grpid);
        r = OK;
        break;
    }

    return (r);
}
