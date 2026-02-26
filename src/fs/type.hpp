/**
 * @file type.hpp
 * @brief File system-local type definitions.
 */

#pragma once

/**
 * @struct dir_struct
 * @brief Directory entry stored on disk.
 */
struct dir_struct {
    inode_nr d_inum;        ///< Inode number.
    char d_name[NAME_SIZE]; ///< Character string name.
};

/**
 * @struct d_inode
 * @brief On-disk inode structure used by ::rw_inode().
 */
struct d_inode {
    mask_bits i_mode;             ///< File type, protection, etc.
    uid i_uid;                    ///< User ID of the file's owner.
    file_pos i_size;              ///< Current file size in bytes.
    file_pos64 i_size64;          ///< 64-bit file size.
    real_time i_modtime;          ///< Timestamp of last data modification.
    gid i_gid;                    ///< Group number.
    links i_nlinks;               ///< Link count.
    zone_nr i_zone[NR_ZONE_NUMS]; ///< Block numbers for direct/indirect zones.
};
