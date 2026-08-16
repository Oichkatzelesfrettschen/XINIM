#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xinim::kernel {

    inline constexpr std::size_t kMaximumUserMappings = 128U;

    struct UserMapping {
        uint64_t start{0U};
        uint64_t length{0U};
        uint32_t protection{0U};
        bool active{false};
    };

    class UserMappingTable {
    public:
        void reset() noexcept;

        [[nodiscard]] bool add(uint64_t start, uint64_t length, uint32_t protection) noexcept;

        [[nodiscard]] int find_by_start(uint64_t start) const noexcept;

        [[nodiscard]] bool overlaps(uint64_t start, uint64_t length,
                                    int excluded_index = -1) const noexcept;

        [[nodiscard]] bool find_free_range(uint64_t lower_bound, uint64_t upper_bound,
                                           uint64_t length, uint64_t hint,
                                           uint64_t &result) const noexcept;

        [[nodiscard]] bool remove_range(uint64_t start, uint64_t length) noexcept;

        [[nodiscard]] bool update_length(std::size_t index, uint64_t length) noexcept;

        [[nodiscard]] const UserMapping *get(std::size_t index) const noexcept;

        [[nodiscard]] std::size_t active_count() const noexcept;

    private:
        [[nodiscard]] int find_inactive_slot() const noexcept;

        std::array<UserMapping, kMaximumUserMappings> mappings_{};
    };

} // namespace xinim::kernel
