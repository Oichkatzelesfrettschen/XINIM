/* This file deals with protection in the file system.  It contains the code
 * for four system calls that relate to protection.
 *
 * The entry points into this file are
 *   do_chmod:	perform the CHMOD system call
 *   do_chown:	perform the CHOWN system call
 *   do_umask:	perform the UMASK system call
 *   do_access:	perform the ACCESS system call
 *   forbidden:	check to see if a given access is allowed on a given inode
 */

#include "../../include/minix/fs_error.hpp" // For make_error_code, ErrorCode
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "super.hpp"
#include "type.hpp"
#include <cstdint>      // For uint16_t
#include <expected>     // For std::expected, std::unexpected
#include <system_error> // For std::error_code (though fs_error.hpp includes it)

// Forward declarations for static functions if needed, or ensure they are defined before use.
static std::expected<void, std::error_code> read_only(struct inode *ip);
PUBLIC std::expected<void, std::error_code> forbidden(struct inode *rip, uint16_t access_desired,
                                                      int real_uid);

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC std::expected<void, std::error_code> do_chmod() {
    /* Perform the chmod(name, mode) system call. */

    register struct inode *rip;
    register int r;
    extern struct inode *eat_path();

    /* Temporarily open the file. */
    if (fetch_name(name, name_length, M3) != OK)
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));
    if ((rip = eat_path(user_path)) == NIL_INODE) // NIL_INODE is nullptr
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));

    /* Only the owner or the super_user may change the mode of a file.
     * No one may change the mode of a file on a read-only file system.
     */
    if (rip->i_uid != fp->fp_effuid && !super_user) { // i_uid and fp_effuid are uid (uint16_t)
        put_inode(rip);
        return std::unexpected(make_error_code(ErrorCode::EPERM));
    }

    auto read_only_res = read_only(rip);
    if (!read_only_res) {
        put_inode(rip);
        return std::unexpected(read_only_res.error());
    }

    /* Now make the change. */
    rip->i_mode = (rip->i_mode & ~ALL_MODES) | (mode & ALL_MODES);
    rip->i_dirt = DIRTY;

    put_inode(rip);
    return {}; // OK
}

/*===========================================================================*
 *				do_chown				     *
 *===========================================================================*/
PUBLIC std::expected<void, std::error_code> do_chown() {
    /* Perform the chown(name, owner, group) system call. */

    register struct inode *rip;
    // register int r; // Will use expected's error state
    extern struct inode *eat_path();

    /* Only the super_user may perform the chown() call. */
    if (!super_user)
        return std::unexpected(make_error_code(ErrorCode::EPERM));

    /* Temporarily open the file. */
    if (fetch_name(name1, name1_length, M1) != OK)
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));
    if ((rip = eat_path(user_path)) == NIL_INODE) // NIL_INODE is nullptr
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));

    /* Not permitted to change the owner of a file on a read-only file sys. */
    auto read_only_res = read_only(rip);
    if (!read_only_res) {
        put_inode(rip);
        return std::unexpected(read_only_res.error());
    }

    // If read_only_res was OK:
    rip->i_uid = static_cast<uint16_t>(owner); // owner from message (int), i_uid is uid (uint16_t)
    rip->i_gid = static_cast<uint8_t>(group);  // group from message (int), i_gid is gid (uint8_t)
    rip->i_dirt = DIRTY;

    put_inode(rip);
    return {}; // OK
}

/*===========================================================================*
 *				do_umask				     *
 *===========================================================================*/
// Returns old mask (mask_bits -> uint16_t) or an error.
PUBLIC std::expected<uint16_t, std::error_code> do_umask() {
    /* Perform the umask(co_mode) system call. */
    register uint16_t r; // Was mask_bits

    r = static_cast<uint16_t>(
        ~fp->fp_umask); /* set 'r' to complement of old mask. fp_umask is mask_bits (uint16_t) */
    // co_mode from message (int). RWX_MODES is int.
    fp->fp_umask = static_cast<uint16_t>(~(co_mode & RWX_MODES));
    return r; /* return complement of old mask */
}

/*===========================================================================*
 *				do_access				     *
 *===========================================================================*/
PUBLIC std::expected<void, std::error_code> do_access() {
    /* Perform the access(name, mode) system call. */

    struct inode *rip;
    // register int r; // Will use expected's error state
    extern struct inode *eat_path();

    /* Temporarily open the file whose access is to be checked. */
    if (fetch_name(name, name_length, M3) != OK)
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));
    if ((rip = eat_path(user_path)) == NIL_INODE) // NIL_INODE is nullptr
        return std::unexpected(make_error_code(static_cast<ErrorCode>(err_code)));

    /* Now check the permissions. */
    // mode from message (int). forbidden expects mask_bits (uint16_t).
    auto forbidden_res = forbidden(rip, static_cast<uint16_t>(mode), 1);
    put_inode(rip);
    if (!forbidden_res) {
        return std::unexpected(forbidden_res.error());
    }
    return {}; // OK
}

/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
// Modernized K&R, changed return type
PUBLIC std::expected<void, std::error_code> forbidden(struct inode *rip, uint16_t access_desired,
                                                      int real_uid) {
    // rip is struct inode*
    // access_desired is mask_bits (uint16_t)
    // real_uid is int
    /* Given a pointer to an inode, 'rip', and the accessed desired, determine
     * if the access is allowed, and if not why not.  The routine looks up the
     * caller's uid in the 'fproc' table.  If the access is allowed, OK is returned
     * if it is forbidden, ErrorCode::EACCES is returned.
     */

    register uint16_t bits, perm_bits; // Were mask_bits
    uint16_t xmask;                    // Was mask_bits
    // int r; // Will use expected for return
    int shift;
    uint16_t test_uid; // uid -> uint16_t
    uint8_t test_gid;  // gid -> uint8_t

    /* Isolate the relevant rwx bits from the mode. */
    bits = rip->i_mode;                                     // i_mode is mask_bits (uint16_t)
    test_uid = (real_uid ? fp->fp_realuid : fp->fp_effuid); // fp_realuid/effuid are uid (uint16_t)
    test_gid = (real_uid ? fp->fp_realgid : fp->fp_effgid); // fp_realgid/effgid are gid (uint8_t)
    if (super_user) {
        perm_bits = 07;
    } else {
        if (test_uid == rip->i_uid)
            shift = 6; /* owner */
        else if (test_gid == rip->i_gid)
            shift = 3; /* group */
        else
            shift = 0; /* other */
        perm_bits = (bits >> shift) & 07;
    }

    /* If access desired is not a subset of what is allowed, it is refused. */
    // r = OK;
    if ((perm_bits | access_desired) != perm_bits) {
        return std::unexpected(make_error_code(ErrorCode::EACCES));
    }

    /* If none of the X bits are on, not even the super-user can execute it. */
    xmask = static_cast<uint16_t>((X_BIT << 6) | (X_BIT << 3) |
                                  X_BIT); /* all 3 X bits (X_BIT is int) */
    if ((access_desired & X_BIT) && (bits & xmask) == 0) {
        return std::unexpected(make_error_code(ErrorCode::EACCES));
    }

    /* Check to see if someone is trying to write on a file system that is
     * mounted read-only.
     */
    if (access_desired & W_BIT) {
        auto read_only_res = read_only(rip);
        if (!read_only_res)
            return std::unexpected(read_only_res.error());
    }

    return {}; // OK
}

/*===========================================================================*
 *				read_only				     *
 *===========================================================================*/
// Modernized K&R, changed return type, PRIVATE -> static
static std::expected<void, std::error_code> read_only(struct inode *ip) {
    /* Check to see if the file system on which the inode 'ip' resides is mounted
     * read only.  If so, return ErrorCode::EROFS, else return OK.
     */

    register struct super_block *sp;
    extern struct super_block *get_super();

    sp = get_super(ip->i_dev); // ip->i_dev is dev_nr (uint16_t)
    if (sp->s_rd_only) {
        return std::unexpected(make_error_code(ErrorCode::EROFS));
    }
    return {}; // OK
}
