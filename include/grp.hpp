#pragma once

/**
 * @file grp.hpp
 * @brief Lightweight group database entry used by compatibility layers.
 */

/// Legacy representation of a group database entry.
struct group {
    char *name;   ///< Name of the group.
    char *passwd; ///< Encrypted group password.
    int gid;      ///< Numerical group identifier.
};
