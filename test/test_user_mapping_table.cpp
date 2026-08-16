#include "user_mapping_table.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {

    constexpr uint64_t kPageSize = 4096U;
    constexpr uint64_t kLowerBound = 0x100000000ULL;
    constexpr uint64_t kUpperBound = kLowerBound + 1024U * kPageSize;

    void test_first_fit_skips_overlaps_and_reuses_holes() {
        xinim::kernel::UserMappingTable mappings;
        mappings.reset();
        assert(mappings.add(kLowerBound, 2U * kPageSize, 3U));
        assert(mappings.add(kLowerBound + 4U * kPageSize, kPageSize, 1U));

        uint64_t result = 0U;
        assert(mappings.find_free_range(kLowerBound, kUpperBound, 2U * kPageSize, kLowerBound,
                                        result));
        assert(result == kLowerBound + 2U * kPageSize);

        assert(mappings.remove_range(kLowerBound, 2U * kPageSize));
        assert(mappings.find_free_range(kLowerBound, kUpperBound, kPageSize, kLowerBound, result));
        assert(result == kLowerBound);
    }

    void test_overlap_and_overflow_rejection() {
        xinim::kernel::UserMappingTable mappings;
        mappings.reset();
        assert(mappings.add(kLowerBound, kPageSize, 3U));
        assert(!mappings.add(kLowerBound, kPageSize, 3U));
        assert(!mappings.add(UINT64_MAX - kPageSize + 1U, kPageSize, 3U));
        assert(mappings.overlaps(kLowerBound + kPageSize / 2U, kPageSize));
        assert(!mappings.overlaps(kLowerBound + kPageSize, kPageSize));
    }

    void test_partial_unmap_splits_and_trims() {
        xinim::kernel::UserMappingTable mappings;
        mappings.reset();
        assert(mappings.add(kLowerBound, 4U * kPageSize, 3U));
        assert(mappings.remove_range(kLowerBound + kPageSize, 2U * kPageSize));
        assert(mappings.active_count() == 2U);
        assert(mappings.overlaps(kLowerBound, kPageSize));
        assert(!mappings.overlaps(kLowerBound + kPageSize, 2U * kPageSize));
        assert(mappings.overlaps(kLowerBound + 3U * kPageSize, kPageSize));

        assert(mappings.remove_range(kLowerBound, kPageSize));
        assert(mappings.active_count() == 1U);
        assert(mappings.remove_range(kLowerBound + 3U * kPageSize, kPageSize));
        assert(mappings.active_count() == 0U);
    }

    void test_capacity_is_explicit_and_stable() {
        xinim::kernel::UserMappingTable mappings;
        mappings.reset();
        for (std::size_t index = 0U; index < xinim::kernel::kMaximumUserMappings; ++index) {
            assert(mappings.add(kLowerBound + 2U * index * kPageSize, kPageSize, 3U));
        }
        assert(mappings.active_count() == xinim::kernel::kMaximumUserMappings);
        assert(!mappings.add(kLowerBound + 2U * xinim::kernel::kMaximumUserMappings * kPageSize,
                             kPageSize, 3U));
    }

    void test_length_update_rejects_collisions() {
        xinim::kernel::UserMappingTable mappings;
        mappings.reset();
        assert(mappings.add(kLowerBound, kPageSize, 3U));
        assert(mappings.add(kLowerBound + 2U * kPageSize, kPageSize, 1U));
        const int first_index = mappings.find_by_start(kLowerBound);
        assert(first_index >= 0);
        assert(mappings.update_length(static_cast<std::size_t>(first_index), 2U * kPageSize));
        assert(!mappings.update_length(static_cast<std::size_t>(first_index), 3U * kPageSize));
    }

} // namespace

int main() {
    test_first_fit_skips_overlaps_and_reuses_holes();
    test_overlap_and_overflow_rejection();
    test_partial_unmap_splits_and_trims();
    test_capacity_is_explicit_and_stable();
    test_length_update_rejects_collisions();
    return 0;
}
