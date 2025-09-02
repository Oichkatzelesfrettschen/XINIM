#pragma once

/**
 * @file process_slot.hpp
 * @brief RAII helper for managing entries in the global process table.
 */

#include "glo.hpp"
#include "mproc.hpp"
#include <ranges>
#include <span>

namespace xinim {

/**
 * @brief RAII wrapper that acquires a free slot in the process table.
 *
 * On construction the first unused slot is marked as ::IN_USE and the global
 * process counter increments. Unless @ref release is called, destruction
 * automatically frees the slot and decrements the counter, ensuring exception
 * safety.
 */
class ScopedProcessSlot {
  public:
    /**
     * @brief Attempt to acquire a free process table slot.
     * @param table View of the global process table.
     */
    explicit ScopedProcessSlot(std::span<mproc> table) noexcept;

    /// Releases the slot if still owned.
    ~ScopedProcessSlot();

    /// @return Pointer to the acquired slot or `nullptr` if none available.
    [[nodiscard]] mproc *get() const noexcept { return slot_; }

    /// @return Index of the slot within the table, or `-1` if invalid.
    [[nodiscard]] int index() const noexcept { return index_; }

    /// Dereference operator providing access to the underlying slot.
    mproc &operator*() const noexcept { return *slot_; }

    /// Member access operator for the underlying slot.
    mproc *operator->() const noexcept { return slot_; }

    /**
     * @brief Release ownership so the destructor does not free the slot.
     */
    void release() noexcept { slot_ = nullptr; }

    /// @return True if a slot was successfully acquired.
    [[nodiscard]] bool valid() const noexcept { return slot_ != nullptr; }

  private:
    std::span<mproc> table_; ///< View of the process table.
    mproc *slot_;            ///< Pointer to the reserved slot.
    int index_;              ///< Index of the slot in the table.
};

} // namespace xinim
