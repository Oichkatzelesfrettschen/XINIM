#include "../include/lib.hpp" // C++23 header
#include <expected>

/// Current end of the data segment managed by brk/sbrk.
extern char *brksize;

namespace xinim {

/**
 * @brief Safe C++ interface for manipulating the program break.
 *
 * The functions wrap the underlying system calls and use std::expected
 * to report errors instead of relying on sentinel values.
 */
class ProgramBreakManager final {
  public:
    ProgramBreakManager() = delete;
    ProgramBreakManager(const ProgramBreakManager &) = delete;
    ProgramBreakManager &operator=(const ProgramBreakManager &) = delete;

    /**
     * @brief Set the program break to the specified address.
     * @param addr Desired end of the data segment.
     * @return std::expected<void, int> Empty on success, otherwise error code.
     */
    [[nodiscard]] static std::expected<void, int> set(char *addr) noexcept;

    /**
     * @brief Increment the program break by a given amount.
     * @param incr Number of bytes to grow the data segment.
     * @return std::expected<char *, int> Previous break on success; error code otherwise.
     */
    [[nodiscard]] static std::expected<char *, int> increment(int incr) noexcept;
};

inline std::expected<void, int> ProgramBreakManager::set(char *addr) noexcept {
    if (auto k = callm1(MM, BRK, 0, 0, 0, addr, NIL_PTR, NIL_PTR); k == OK) {
        brksize = M.m2_p1();
        return {};
    }
    return std::unexpected(k);
}

inline std::expected<char *, int> ProgramBreakManager::increment(int incr) noexcept {
    char *oldsize = brksize;
    char *newsize = brksize + incr;
    if (auto result = set(newsize); result) {
        return oldsize;
    }
    return std::unexpected(result.error());
}

} // namespace xinim

/**
 * @brief C-compatible wrapper for setting the program break.
 * @param addr Desired end of the data segment.
 * @return NIL_PTR on success, `(char*)-1` on failure.
 */
extern "C" char *brk(char *addr) {
    auto result = xinim::ProgramBreakManager::set(addr);
    return result ? NIL_PTR : reinterpret_cast<char *>(-1);
}

/**
 * @brief C-compatible wrapper for incrementing the program break.
 * @param incr Number of bytes to grow the data segment.
 * @return Previous break on success, `(char*)-1` on failure.
 */
extern "C" char *sbrk(int incr) {
    auto result = xinim::ProgramBreakManager::increment(incr);
    return result ? result.value() : reinterpret_cast<char *>(-1);
}
