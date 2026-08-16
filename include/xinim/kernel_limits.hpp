#pragma once

#include <cstddef>

namespace xinim::limits {

    /** Maximum number of process slots owned by the kernel scheduler. */
    inline constexpr std::size_t MAX_PROCESS_COUNT = 64U;

} // namespace xinim::limits
