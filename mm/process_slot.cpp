/**
 * @file process_slot.cpp
 * @brief Implementation of ScopedProcessSlot.
 */

#include "process_slot.hpp"
#include <ranges>

namespace xinim {

ScopedProcessSlot::ScopedProcessSlot(std::span<mproc> table) noexcept
    : table_{table}, slot_{nullptr}, index_{-1} {
    auto it =
        std::ranges::find_if(table_, [](const mproc &p) { return (p.mp_flags & IN_USE) == 0; });
    if (it != table_.end()) {
        slot_ = &*it;
        index_ = static_cast<int>(std::distance(table_.begin(), it));
        slot_->mp_flags |= IN_USE;
        ++procs_in_use;
    }
}

ScopedProcessSlot::~ScopedProcessSlot() {
    if (slot_) {
        slot_->mp_flags &= ~IN_USE;
        --procs_in_use;
    }
}

} // namespace xinim
