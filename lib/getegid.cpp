#include "../include/lib.hpp" // C++23 header
#include <expected>

namespace xinim {

/**
 * @brief Retrieve the effective group identifier of the current process.
 *
 * Constructs and sends a `GETGID` request directly to the memory manager and
 * extracts the effective group identifier from the reply. This avoids the
 * intermediate `callm1` helper, yielding a self-contained implementation.
 *
 * @return std::expected<gid, int> The effective group ID on success or a
 *         negative error code on failure.
 */
[[nodiscard]] std::expected<gid, int> get_effective_group_id() noexcept {
    message msg{};       // Local message to preserve thread safety
    msg.m_type = GETGID; // Request real and effective group identifiers

    if (int status = sendrec(MM, &msg); status != OK) {
        return std::unexpected(status); // Transport error
    }
    if (msg.m_type < 0) {
        return std::unexpected(msg.m_type); // Kernel reported failure
    }

    return static_cast<gid>(msg.m2_i1()); // Effective group ID returned in m2_i1
}

} // namespace xinim

/**
 * @brief C-compatible wrapper exposing the effective group ID.
 *
 * @return gid Effective group ID on success; otherwise the negative error code
 *         cast to `gid`.
 */
extern "C" gid getegid() {
    auto result = xinim::get_effective_group_id();
    return result ? result.value() : static_cast<gid>(result.error());
}
