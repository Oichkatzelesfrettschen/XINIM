#include "user_address_space.hpp"

#include "../../../mm/alloc.hpp"

#include <cstring>
#include <limits>
#include <xinim/boot/bootinfo.hpp>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint64_t kEntriesPerTable = 512U;
        constexpr uint64_t kAddressMask = 0x000ffffffffff000ULL;
        constexpr uint64_t kPresent = 1U << 0U;
        constexpr uint64_t kWritable = 1U << 1U;
        constexpr uint64_t kUser = 1U << 2U;
        constexpr uint64_t kHugePage = 1U << 7U;
        constexpr uint64_t kNoExecute = 1ULL << 63U;
        constexpr uint64_t kKernelPml4Start = 256U;
        constexpr uint64_t kClicksPerPage = kPageSize / CLICK_SIZE;

        static_assert((kPageSize % CLICK_SIZE) == 0U);

        [[nodiscard]] bool is_page_aligned(uint64_t address) noexcept {
            return (address & (kPageSize - 1U)) == 0U;
        }

        [[nodiscard]] bool is_user_page(uint64_t virtual_address) noexcept {
            return is_page_aligned(virtual_address) && virtual_address < kUserCanonicalLimit;
        }

        [[nodiscard]] uint64_t active_root_physical() noexcept {
            uint64_t root = 0U;
            asm volatile("mov %%cr3, %0" : "=r"(root));
            return root & kAddressMask;
        }

        [[nodiscard]] uint64_t *physical_table(uint64_t physical_address) noexcept {
            if (!is_page_aligned(physical_address)) {
                return nullptr;
            }
            const uint64_t hhdm_offset = xinim::boot::get_info().hhdm_offset;
            if (physical_address > std::numeric_limits<uint64_t>::max() - hhdm_offset) {
                return nullptr;
            }
            return reinterpret_cast<uint64_t *>(physical_address + hhdm_offset);
        }

        [[nodiscard]] uint64_t allocate_zeroed_page() noexcept {
            const uint64_t base_click = alloc_mem(kClicksPerPage);
            if (base_click == NO_MEM ||
                base_click > (std::numeric_limits<uint64_t>::max() >> CLICK_SHIFT)) {
                return 0U;
            }
            const uint64_t physical_address = base_click << CLICK_SHIFT;
            if (!is_page_aligned(physical_address)) {
                (void) free_mem(base_click, kClicksPerPage);
                return 0U;
            }
            uint64_t *page = physical_table(physical_address);
            if (page == nullptr) {
                (void) free_mem(base_click, kClicksPerPage);
                return 0U;
            }
            std::memset(page, 0, kPageSize);
            return physical_address;
        }

        [[nodiscard]] uint64_t *descend_or_allocate(uint64_t *table, uint64_t index) noexcept {
            uint64_t entry = table[index];
            if ((entry & kPresent) != 0U) {
                if ((entry & kHugePage) != 0U) {
                    return nullptr;
                }
                entry |= kWritable | kUser;
                table[index] = entry;
                return physical_table(entry & kAddressMask);
            }

            const uint64_t child_physical = allocate_zeroed_page();
            if (child_physical == 0U) {
                return nullptr;
            }
            table[index] = child_physical | kPresent | kWritable | kUser;
            return physical_table(child_physical);
        }

        [[nodiscard]] uint64_t *find_leaf(const UserAddressSpace &address_space,
                                          uint64_t virtual_address) noexcept {
            if (address_space.root_physical == 0U || virtual_address >= kUserCanonicalLimit) {
                return nullptr;
            }

            uint64_t *level4 = physical_table(address_space.root_physical);
            if (level4 == nullptr) {
                return nullptr;
            }
            const uint64_t indexes[4] = {
                (virtual_address >> 39U) & 0x1ffU,
                (virtual_address >> 30U) & 0x1ffU,
                (virtual_address >> 21U) & 0x1ffU,
                (virtual_address >> 12U) & 0x1ffU,
            };

            uint64_t *table = level4;
            for (size_t level = 0U; level < 3U; ++level) {
                const uint64_t entry = table[indexes[level]];
                if ((entry & kPresent) == 0U || (entry & kHugePage) != 0U) {
                    return nullptr;
                }
                table = physical_table(entry & kAddressMask);
                if (table == nullptr) {
                    return nullptr;
                }
            }
            return &table[indexes[3]];
        }

    } // namespace

    bool create_user_address_space(UserAddressSpace &address_space) noexcept {
        address_space.root_physical = 0U;
        const uint64_t source_root_physical = active_root_physical();
        uint64_t *source_root = physical_table(source_root_physical);
        if (source_root == nullptr) {
            return false;
        }

        const uint64_t destination_root_physical = allocate_zeroed_page();
        uint64_t *destination_root = physical_table(destination_root_physical);
        if (destination_root == nullptr) {
            return false;
        }

        // Kernel and HHDM mappings remain supervisor-only and are shared. Lower
        // canonical entries start empty so user mappings must be admitted explicitly.
        for (uint64_t index = kKernelPml4Start; index < kEntriesPerTable; ++index) {
            destination_root[index] = source_root[index];
        }
        address_space.root_physical = destination_root_physical;
        return true;
    }

    bool clone_user_address_space(const UserAddressSpace &source,
                                  UserAddressSpace &destination) noexcept {
        destination = {};
        if (source.root_physical == 0U || !create_user_address_space(destination)) {
            return false;
        }
        uint64_t *source_level4 = physical_table(source.root_physical);
        if (source_level4 == nullptr) {
            destroy_user_address_space(destination);
            return false;
        }

        for (uint64_t level4_index = 0U; level4_index < kKernelPml4Start; ++level4_index) {
            const uint64_t level4_entry = source_level4[level4_index];
            if ((level4_entry & kPresent) == 0U || (level4_entry & kHugePage) != 0U) {
                continue;
            }
            uint64_t *source_level3 = physical_table(level4_entry & kAddressMask);
            if (source_level3 == nullptr) {
                destroy_user_address_space(destination);
                return false;
            }
            for (uint64_t level3_index = 0U; level3_index < kEntriesPerTable; ++level3_index) {
                const uint64_t level3_entry = source_level3[level3_index];
                if ((level3_entry & kPresent) == 0U) {
                    continue;
                }
                if ((level3_entry & kHugePage) != 0U) {
                    destroy_user_address_space(destination);
                    return false;
                }
                uint64_t *source_level2 = physical_table(level3_entry & kAddressMask);
                if (source_level2 == nullptr) {
                    destroy_user_address_space(destination);
                    return false;
                }
                for (uint64_t level2_index = 0U; level2_index < kEntriesPerTable; ++level2_index) {
                    const uint64_t level2_entry = source_level2[level2_index];
                    if ((level2_entry & kPresent) == 0U) {
                        continue;
                    }
                    if ((level2_entry & kHugePage) != 0U) {
                        destroy_user_address_space(destination);
                        return false;
                    }
                    uint64_t *source_level1 = physical_table(level2_entry & kAddressMask);
                    if (source_level1 == nullptr) {
                        destroy_user_address_space(destination);
                        return false;
                    }
                    for (uint64_t level1_index = 0U; level1_index < kEntriesPerTable;
                         ++level1_index) {
                        const uint64_t source_leaf = source_level1[level1_index];
                        if ((source_leaf & kPresent) == 0U) {
                            continue;
                        }
                        const uint64_t virtual_address =
                            (level4_index << 39U) | (level3_index << 30U) | (level2_index << 21U) |
                            (level1_index << 12U);
                        UserPageFlags flags = UserPageFlags::ReadOnly;
                        if ((source_leaf & kWritable) != 0U) {
                            flags = flags | UserPageFlags::Writable;
                        }
                        if ((source_leaf & kNoExecute) == 0U) {
                            flags = flags | UserPageFlags::Executable;
                        }
                        uint64_t *source_page = physical_table(source_leaf & kAddressMask);
                        if (source_page == nullptr ||
                            !map_zeroed_user_page(destination, virtual_address, flags) ||
                            !copy_to_user_address_space(destination, virtual_address, source_page,
                                                        kPageSize)) {
                            destroy_user_address_space(destination);
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }

    void destroy_user_address_space(UserAddressSpace &address_space) noexcept {
        if (address_space.root_physical == 0U) {
            return;
        }
        uint64_t *level4 = physical_table(address_space.root_physical);
        if (level4 != nullptr) {
            for (uint64_t level4_index = 0U; level4_index < kKernelPml4Start; ++level4_index) {
                const uint64_t level4_entry = level4[level4_index];
                if ((level4_entry & kPresent) == 0U || (level4_entry & kHugePage) != 0U) {
                    continue;
                }
                uint64_t *level3 = physical_table(level4_entry & kAddressMask);
                if (level3 == nullptr) {
                    continue;
                }
                for (uint64_t level3_index = 0U; level3_index < kEntriesPerTable; ++level3_index) {
                    const uint64_t level3_entry = level3[level3_index];
                    if ((level3_entry & kPresent) == 0U || (level3_entry & kHugePage) != 0U) {
                        continue;
                    }
                    uint64_t *level2 = physical_table(level3_entry & kAddressMask);
                    if (level2 == nullptr) {
                        continue;
                    }
                    for (uint64_t level2_index = 0U; level2_index < kEntriesPerTable;
                         ++level2_index) {
                        const uint64_t level2_entry = level2[level2_index];
                        if ((level2_entry & kPresent) == 0U || (level2_entry & kHugePage) != 0U) {
                            continue;
                        }
                        uint64_t *level1 = physical_table(level2_entry & kAddressMask);
                        if (level1 == nullptr) {
                            continue;
                        }
                        for (uint64_t level1_index = 0U; level1_index < kEntriesPerTable;
                             ++level1_index) {
                            const uint64_t leaf = level1[level1_index];
                            if ((leaf & kPresent) != 0U) {
                                (void) free_mem((leaf & kAddressMask) >> CLICK_SHIFT,
                                                kClicksPerPage);
                            }
                        }
                        (void) free_mem((level2_entry & kAddressMask) >> CLICK_SHIFT,
                                        kClicksPerPage);
                    }
                    (void) free_mem((level3_entry & kAddressMask) >> CLICK_SHIFT, kClicksPerPage);
                }
                (void) free_mem((level4_entry & kAddressMask) >> CLICK_SHIFT, kClicksPerPage);
            }
        }
        (void) free_mem(address_space.root_physical >> CLICK_SHIFT, kClicksPerPage);
        address_space.root_physical = 0U;
    }

    bool map_zeroed_user_page(UserAddressSpace &address_space, uint64_t virtual_address,
                              UserPageFlags flags) noexcept {
        if (address_space.root_physical == 0U || !is_user_page(virtual_address)) {
            return false;
        }

        uint64_t *level4 = physical_table(address_space.root_physical);
        if (level4 == nullptr) {
            return false;
        }
        const uint64_t indexes[4] = {
            (virtual_address >> 39U) & 0x1ffU,
            (virtual_address >> 30U) & 0x1ffU,
            (virtual_address >> 21U) & 0x1ffU,
            (virtual_address >> 12U) & 0x1ffU,
        };

        uint64_t *level3 = descend_or_allocate(level4, indexes[0]);
        uint64_t *level2 = level3 != nullptr ? descend_or_allocate(level3, indexes[1]) : nullptr;
        uint64_t *level1 = level2 != nullptr ? descend_or_allocate(level2, indexes[2]) : nullptr;
        if (level1 == nullptr || (level1[indexes[3]] & kPresent) != 0U) {
            return false;
        }

        const uint64_t page_physical = allocate_zeroed_page();
        if (page_physical == 0U) {
            return false;
        }
        uint64_t entry_flags = kPresent | kUser;
        if (has_flag(flags, UserPageFlags::Writable)) {
            entry_flags |= kWritable;
        }
        if (!has_flag(flags, UserPageFlags::Executable)) {
            entry_flags |= kNoExecute;
        }
        level1[indexes[3]] = page_physical | entry_flags;
        return true;
    }

    bool unmap_user_page(UserAddressSpace &address_space, uint64_t virtual_address) noexcept {
        if (!is_user_page(virtual_address)) {
            return false;
        }
        uint64_t *leaf = find_leaf(address_space, virtual_address);
        if (leaf == nullptr || (*leaf & (kPresent | kUser)) != (kPresent | kUser)) {
            return false;
        }
        const uint64_t physical_address = *leaf & kAddressMask;
        const uint64_t previous_entry = *leaf;
        *leaf = 0U;
        asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
        if (free_mem(physical_address >> CLICK_SHIFT, kClicksPerPage)) {
            return true;
        }
        *leaf = previous_entry;
        asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
        return false;
    }

    uint64_t resolve_user_physical(const UserAddressSpace &address_space,
                                   uint64_t virtual_address) noexcept {
        uint64_t *leaf = find_leaf(address_space, virtual_address);
        if (leaf == nullptr || (*leaf & kPresent) == 0U || (*leaf & kUser) == 0U) {
            return 0U;
        }
        return (*leaf & kAddressMask) | (virtual_address & (kPageSize - 1U));
    }

    bool is_user_range_mapped(const UserAddressSpace &address_space, uint64_t virtual_address,
                              size_t size, bool require_writable) noexcept {
        if (size == 0U) {
            return virtual_address < kUserCanonicalLimit;
        }
        if (virtual_address >= kUserCanonicalLimit ||
            size > kUserCanonicalLimit - virtual_address) {
            return false;
        }
        const uint64_t last_address = virtual_address + size - 1U;
        uint64_t page = virtual_address & ~(kPageSize - 1U);
        const uint64_t last_page = last_address & ~(kPageSize - 1U);
        for (;;) {
            uint64_t *leaf = find_leaf(address_space, page);
            if (leaf == nullptr || (*leaf & (kPresent | kUser)) != (kPresent | kUser) ||
                (require_writable && (*leaf & kWritable) == 0U)) {
                return false;
            }
            if (page == last_page) {
                return true;
            }
            page += kPageSize;
        }
    }

    bool is_user_range_executable(const UserAddressSpace &address_space, uint64_t virtual_address,
                                  size_t size) noexcept {
        if (size == 0U || virtual_address >= kUserCanonicalLimit ||
            size > kUserCanonicalLimit - virtual_address) {
            return false;
        }
        const uint64_t last_address = virtual_address + size - 1U;
        uint64_t page = virtual_address & ~(kPageSize - 1U);
        const uint64_t last_page = last_address & ~(kPageSize - 1U);
        for (;;) {
            uint64_t *leaf = find_leaf(address_space, page);
            if (leaf == nullptr || (*leaf & (kPresent | kUser)) != (kPresent | kUser) ||
                (*leaf & kNoExecute) != 0U) {
                return false;
            }
            if (page == last_page) {
                return true;
            }
            page += kPageSize;
        }
    }

    bool copy_to_user_address_space(const UserAddressSpace &address_space, uint64_t destination,
                                    const void *source, size_t size) noexcept {
        if ((source == nullptr && size != 0U) ||
            destination > std::numeric_limits<uint64_t>::max() - size) {
            return false;
        }
        const auto *input = static_cast<const uint8_t *>(source);
        size_t copied = 0U;
        while (copied < size) {
            const uint64_t user_address = destination + copied;
            const uint64_t physical_address = resolve_user_physical(address_space, user_address);
            if (physical_address == 0U) {
                return false;
            }
            const size_t page_remaining =
                static_cast<size_t>(kPageSize - (user_address & (kPageSize - 1U)));
            const size_t chunk =
                (size - copied) < page_remaining ? (size - copied) : page_remaining;
            void *output = physical_table(physical_address & kAddressMask);
            if (output == nullptr) {
                return false;
            }
            auto *output_bytes =
                static_cast<uint8_t *>(output) + (physical_address & (kPageSize - 1U));
            std::memcpy(output_bytes, input + copied, chunk);
            copied += chunk;
        }
        return true;
    }

    bool zero_user_address_space(const UserAddressSpace &address_space, uint64_t destination,
                                 size_t size) noexcept {
        if (destination > std::numeric_limits<uint64_t>::max() - size) {
            return false;
        }
        size_t cleared = 0U;
        while (cleared < size) {
            const uint64_t user_address = destination + cleared;
            const uint64_t physical_address = resolve_user_physical(address_space, user_address);
            if (physical_address == 0U) {
                return false;
            }
            const size_t page_remaining =
                static_cast<size_t>(kPageSize - (user_address & (kPageSize - 1U)));
            const size_t chunk =
                (size - cleared) < page_remaining ? (size - cleared) : page_remaining;
            void *output = physical_table(physical_address & kAddressMask);
            if (output == nullptr) {
                return false;
            }
            auto *output_bytes =
                static_cast<uint8_t *>(output) + (physical_address & (kPageSize - 1U));
            std::memset(output_bytes, 0, chunk);
            cleared += chunk;
        }
        return true;
    }

} // namespace xinim::kernel::x86_64
