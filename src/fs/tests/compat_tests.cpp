#include "../"sys/const.hpp"
inline constexpr int NR_ZONE_NUMS = 9;
inline constexpr int NR_INODES = 32;
#include "compat.hpp"
#include "error.hpp"
#include "lib.hpp"

#include <cassert>
#include <cstddef>

int main() {
    struct inode ip{};
    ip.i_size = 42;
    ip.i_size64 = 0;
    assert(compat_get_size(&ip) == 42);

    ip.i_size64 = 1000;
    assert(compat_get_size(&ip) == 1000);

    init_extended_inode(&ip);
    assert(ip.i_size64 == ip.i_size);
    assert(ip.i_extents == nullptr);
    assert(ip.i_extent_count == 0);

    assert(alloc_extent_table(&ip, 0) == static_cast<int>(ErrorCode::EINVAL));
    assert(ip.i_extents == nullptr);
    assert(ip.i_extent_count == 0);

    assert(alloc_extent_table(&ip, 2) == OK);
    assert(ip.i_extents != nullptr);
    assert(ip.i_extent_count == 2);

    safe_free(ip.i_extents);
    ip.i_extents = nullptr;

    return 0;
}
