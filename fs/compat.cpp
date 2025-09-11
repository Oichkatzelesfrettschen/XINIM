/**
 * @brief Translation helpers for compatibility with the original Minix FS.
 *
 * These helpers store 64-bit file sizes and extent tables while maintaining
 * the legacy 32-bit structures.
 */

#include "../h/const.hpp"
#include "../h/type.hpp"
inline constexpr int NR_ZONE_NUMS = 9;
inline constexpr int NR_INODES = 32;
inline constexpr zone_nr NO_ZONE = static_cast<zone_nr>(0);
#include "../include/lib.hpp" // C++23 header
#include "compat.hpp"

/**
 * @brief Return the 64-bit size of an inode.
 *
 * Retrieves size information directly from memory without performing I/O.
 *
 * @param ip inode to query.
 * @return 64-bit file size.
 * @note No locking is performed; caller must ensure the inode remains
 *       consistent during the read operation.
 */
file_pos64 compat_get_size(const struct inode *ip) {
    return ip->i_size64 ? ip->i_size64 : (file_pos64)ip->i_size;
}

/**
 * @brief Update both the 64-bit and 32-bit size fields.
 *
 * Modifies in-memory inode metadata and does not trigger disk I/O. The caller
 * is responsible for persisting changes.
 *
 * @param ip inode to update.
 * @param sz new file size.
 * @note No internal synchronization; callers should provide appropriate
 *       locking when updating shared inodes.
 */
void compat_set_size(struct inode *ip, file_pos64 sz) {
    ip->i_size64 = sz;
    ip->i_size = (file_pos)sz;
}

/**
 * @brief Initialize the 64-bit fields of an inode.
 *
 * Clears extent information and leaves the inode ready for use without
 * touching persistent storage.
 *
 * @param ip inode to set up.
 * @note Assumes the inode is newly allocated and not concurrently modified.
 */
void init_extended_inode(struct inode *ip) {
    ip->i_size64 = ip->i_size;
    ip->i_extents = nullptr;
    ip->i_extent_count = 0;
}

/**
 * @brief Allocate and zero @p count extents for an inode.
 * @param ip inode receiving the extent table.
 * @param count number of extents to allocate.
 * @return OK on success or ErrorCode on failure.
 *
 * Allocates memory and initializes it for extent tracking; no disk I/O is
 * performed. Errors propagate via the return value.
 *
 * @note Not thread-safe. Callers must synchronize access to the inode when
 *       installing or replacing extent tables.
 */
int alloc_extent_table(struct inode *ip, unsigned short count) {

    // RAII wrapper for the extent table memory.
    SafeBuffer<extent> table_buf(count); // managed table memory
    unsigned short i;                    /* Loop counter for initialization.       */

    /* Reject zero-sized tables to avoid undefined behaviour. */
    if (count == 0) {
        ip->i_extents = nullptr;
        ip->i_extent_count = 0;
        return static_cast<int>(ErrorCode::EINVAL);
    }

    /* Memory was successfully allocated by SafeBuffer. */

    /* Initialize all table entries so the caller starts with empty extents. */
    for (i = 0; i < count; i++) {
        table_buf[i].e_start = NO_ZONE;
        table_buf[i].e_count = 0;
    }

    /* Attach the table to the inode. */
    ip->i_extents = table_buf.release();
    ip->i_extent_count = count;

    return OK;
}
