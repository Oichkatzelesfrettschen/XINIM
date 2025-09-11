/**
 * @brief Extent-based allocation structures.
 */
#ifndef FS_EXTENT_H
#define FS_EXTENT_H

#include "sys/type.hpp"

/**
 * @brief Describes a contiguous range of zones on disk.
 */
struct extent {
    zone_nr e_start; /**< first zone in the extent */
    zone_nr e_count; /**< number of zones in the extent */
};

/// Null extent pointer used to denote absence of an extent table.
inline constexpr extent *NIL_EXTENT = nullptr;

#endif /* FS_EXTENT_H */
