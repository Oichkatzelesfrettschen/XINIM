#include "user_mapping_table.hpp"

#include <limits>

namespace xinim::kernel {
    namespace {

        [[nodiscard]] bool range_end(uint64_t start, uint64_t length, uint64_t &end) noexcept {
            if (length == 0U || start > std::numeric_limits<uint64_t>::max() - length) {
                return false;
            }
            end = start + length;
            return true;
        }

        [[nodiscard]] bool ranges_overlap(uint64_t first_start, uint64_t first_end,
                                          uint64_t second_start, uint64_t second_end) noexcept {
            return first_start < second_end && second_start < first_end;
        }

    } // namespace

    void UserMappingTable::reset() noexcept {
        for (UserMapping &mapping : mappings_) {
            mapping = {};
        }
    }

    bool UserMappingTable::add(uint64_t start, uint64_t length, uint32_t protection) noexcept {
        const int slot = find_inactive_slot();
        if (slot < 0 || overlaps(start, length)) {
            return false;
        }
        mappings_[static_cast<std::size_t>(slot)] = {start, length, protection, true};
        return true;
    }

    int UserMappingTable::find_by_start(uint64_t start) const noexcept {
        for (std::size_t index = 0U; index < mappings_.size(); ++index) {
            if (mappings_[index].active && mappings_[index].start == start) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    bool UserMappingTable::overlaps(uint64_t start, uint64_t length,
                                    int excluded_index) const noexcept {
        uint64_t end = 0U;
        if (!range_end(start, length, end)) {
            return true;
        }
        for (std::size_t index = 0U; index < mappings_.size(); ++index) {
            const UserMapping &mapping = mappings_[index];
            if (!mapping.active || static_cast<int>(index) == excluded_index) {
                continue;
            }
            if (ranges_overlap(start, end, mapping.start, mapping.start + mapping.length)) {
                return true;
            }
        }
        return false;
    }

    bool UserMappingTable::find_free_range(uint64_t lower_bound, uint64_t upper_bound,
                                           uint64_t length, uint64_t hint,
                                           uint64_t &result) const noexcept {
        if (length == 0U || lower_bound >= upper_bound || length > upper_bound - lower_bound) {
            return false;
        }

        uint64_t candidate = hint >= lower_bound ? hint : lower_bound;
        if (candidate > upper_bound - length) {
            candidate = lower_bound;
        }

        for (;;) {
            uint64_t next_candidate = candidate;
            bool collision = false;
            for (const UserMapping &mapping : mappings_) {
                if (!mapping.active) {
                    continue;
                }
                const uint64_t candidate_end = candidate + length;
                const uint64_t mapping_end = mapping.start + mapping.length;
                if (ranges_overlap(candidate, candidate_end, mapping.start, mapping_end)) {
                    collision = true;
                    if (mapping_end > next_candidate) {
                        next_candidate = mapping_end;
                    }
                }
            }
            if (!collision) {
                result = candidate;
                return true;
            }
            if (next_candidate <= candidate || next_candidate > upper_bound - length) {
                if (candidate == lower_bound) {
                    return false;
                }
                candidate = lower_bound;
                continue;
            }
            candidate = next_candidate;
        }
    }

    bool UserMappingTable::remove_range(uint64_t start, uint64_t length) noexcept {
        uint64_t removal_end = 0U;
        if (!range_end(start, length, removal_end)) {
            return false;
        }

        std::size_t split_count = 0U;
        for (const UserMapping &mapping : mappings_) {
            if (!mapping.active) {
                continue;
            }
            const uint64_t mapping_end = mapping.start + mapping.length;
            if (mapping.start < start && removal_end < mapping_end) {
                ++split_count;
            }
        }
        if (active_count() + split_count > mappings_.size()) {
            return false;
        }

        for (std::size_t index = 0U; index < mappings_.size(); ++index) {
            UserMapping &mapping = mappings_[index];
            if (!mapping.active) {
                continue;
            }
            const uint64_t mapping_start = mapping.start;
            const uint64_t mapping_end = mapping.start + mapping.length;
            if (!ranges_overlap(start, removal_end, mapping_start, mapping_end)) {
                continue;
            }
            const uint64_t retained_start = start > mapping_start ? start : mapping_start;
            const uint64_t retained_end = removal_end < mapping_end ? removal_end : mapping_end;
            if (retained_start == mapping_start && retained_end == mapping_end) {
                mapping = {};
                continue;
            }
            if (retained_start == mapping_start) {
                mapping.start = retained_end;
                mapping.length = mapping_end - retained_end;
                continue;
            }
            if (retained_end == mapping_end) {
                mapping.length = retained_start - mapping_start;
                continue;
            }

            const uint32_t protection = mapping.protection;
            mapping.length = retained_start - mapping_start;
            const int new_slot = find_inactive_slot();
            if (new_slot < 0) {
                return false;
            }
            mappings_[static_cast<std::size_t>(new_slot)] = {
                retained_end, mapping_end - retained_end, protection, true};
        }
        return true;
    }

    bool UserMappingTable::update_length(std::size_t index, uint64_t length) noexcept {
        if (index >= mappings_.size() || !mappings_[index].active || length == 0U) {
            return false;
        }
        if (overlaps(mappings_[index].start, length, static_cast<int>(index))) {
            return false;
        }
        mappings_[index].length = length;
        return true;
    }

    const UserMapping *UserMappingTable::get(std::size_t index) const noexcept {
        if (index >= mappings_.size() || !mappings_[index].active) {
            return nullptr;
        }
        return &mappings_[index];
    }

    std::size_t UserMappingTable::active_count() const noexcept {
        std::size_t count = 0U;
        for (const UserMapping &mapping : mappings_) {
            if (mapping.active) {
                ++count;
            }
        }
        return count;
    }

    int UserMappingTable::find_inactive_slot() const noexcept {
        for (std::size_t index = 0U; index < mappings_.size(); ++index) {
            if (!mappings_[index].active) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

} // namespace xinim::kernel
