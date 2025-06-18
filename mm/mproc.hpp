#pragma once
// Modernized for C++23

/**
 * @brief Per-process memory management information.
 *
 * Each entry describes a single process and stores segment descriptors,
 * credentials and various status flags.  The kernel and file system maintain
 * parallel tables indexed by process number.
 */

#include "../h/type.hpp" // For uid, gid, unshort, mem_map
#include <cstddef>       // For std::size_t if not via type.hpp
#include <cstdint>       // For explicit use of uint16_t, uint8_t if not via type.hpp

EXTERN struct mproc {
    struct mem_map mp_seg[NR_SEGS]; /**< Segment descriptors for text, data and stack. */
    char mp_exitstatus;             /**< Status code when the process exits. */
    char mp_sigstatus;              /**< Signal number that caused termination. */
    int mp_pid;                     /**< Process identifier. */
    int mp_parent;                  /**< Index of parent process. */
    int mp_procgrp;                 /**< Process group used for signals. */
    std::uint64_t mp_token;         /**< Capability token for privileged actions. */

    /* Real and effective uids and gids. */
    uid mp_realuid; /**< Process real user id. */
    uid mp_effuid;  /**< Process effective user id. */
    gid mp_realgid; /**< Process real group id. */
    gid mp_effgid;  /**< Process effective group id. */

    /* Bit maps for signals. */
    unshort mp_ignore; /**< Non-zero to ignore the signal. */
    unshort mp_catch;  /**< Non-zero to catch the signal. */
    int (*mp_func)();  /**< User function handling all signals. */

    unsigned mp_flags; /**< Process flags. */
} mproc[NR_PROCS];

/**
 * @brief Values for ::mproc::mp_flags.
 */
inline constexpr unsigned int IN_USE = 001;   /**< Slot is currently in use. */
inline constexpr unsigned int WAITING = 002;  /**< Set by the WAIT system call. */
inline constexpr unsigned int HANGING = 004;  /**< Set by the EXIT system call. */
inline constexpr unsigned int PAUSED = 010;   /**< Set by the PAUSE system call. */
inline constexpr unsigned int ALARM_ON = 020; /**< Set when the SIGALRM timer is active. */
inline constexpr unsigned int SEPARATE = 040; /**< Process has separate I&D space. */
