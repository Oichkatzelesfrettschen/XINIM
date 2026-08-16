#include "memory_syscalls.hpp"

#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../uaccess.hpp"
#include "elf64_user_image.hpp"
#include "user_address_space.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint64_t kAnonymousMappingBase = 0x0000100000000000ULL;
        constexpr uint64_t kAnonymousMappingLimit = kInitialUserStackBottom;
        constexpr int kProtectionRead = 0x1;
        constexpr int kProtectionWrite = 0x2;
        constexpr int kProtectionExecute = 0x4;
        constexpr int kMapPrivate = 0x02;
        constexpr int kMapFixed = 0x10;
        constexpr int kMapAnonymous = 0x20;
        constexpr unsigned long kRemapMayMove = 0x1UL;
        constexpr unsigned long kRemapFixed = 0x2UL;
        constexpr size_t kCopyBufferSize = 512U;

        [[nodiscard]] bool align_length(size_t length, uint64_t &aligned_length) noexcept {
            if (length == 0U || length > std::numeric_limits<uint64_t>::max() - (kPageSize - 1U)) {
                return false;
            }
            aligned_length = (static_cast<uint64_t>(length) + kPageSize - 1U) & ~(kPageSize - 1U);
            return aligned_length != 0U;
        }

        [[nodiscard]] bool is_page_aligned(uint64_t address) noexcept {
            return (address & (kPageSize - 1U)) == 0U;
        }

        [[nodiscard]] UserPageFlags page_flags_for(int protection) noexcept {
            UserPageFlags page_flags = UserPageFlags::ReadOnly;
            if ((protection & kProtectionWrite) != 0) {
                page_flags = page_flags | UserPageFlags::Writable;
            }
            if ((protection & kProtectionExecute) != 0) {
                page_flags = page_flags | UserPageFlags::Executable;
            }
            return page_flags;
        }

        [[nodiscard]] bool range_is_unmapped(const UserAddressSpace &address_space, uint64_t start,
                                             uint64_t length) noexcept {
            for (uint64_t page = start; page < start + length; page += kPageSize) {
                if (resolve_user_physical(address_space, page) != 0U) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool map_pages(UserAddressSpace &address_space, uint64_t start,
                                     uint64_t length, UserPageFlags page_flags) noexcept {
            uint64_t page = start;
            for (; page < start + length; page += kPageSize) {
                if (!map_zeroed_user_page(address_space, page, page_flags)) {
                    for (uint64_t rollback = start; rollback < page; rollback += kPageSize) {
                        static_cast<void>(unmap_user_page(address_space, rollback));
                    }
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool unmap_owned_pages(const UserMappingTable &mappings,
                                             UserAddressSpace &address_space, uint64_t start,
                                             uint64_t length) noexcept {
            const uint64_t requested_end = start + length;
            bool success = true;
            for (std::size_t index = 0U; index < kMaximumUserMappings; ++index) {
                const UserMapping *mapping = mappings.get(index);
                if (mapping == nullptr) {
                    continue;
                }
                const uint64_t mapping_end = mapping->start + mapping->length;
                const uint64_t intersection_start = std::max(start, mapping->start);
                const uint64_t intersection_end = std::min(requested_end, mapping_end);
                if (intersection_start >= intersection_end) {
                    continue;
                }
                for (uint64_t page = intersection_start; page < intersection_end;
                     page += kPageSize) {
                    if (resolve_user_physical(address_space, page) != 0U &&
                        !unmap_user_page(address_space, page)) {
                        success = false;
                    }
                }
            }
            return success;
        }

        [[nodiscard]] int64_t map_anonymous(ProcessControlBlock &process, uint64_t address_hint,
                                            uint64_t length, int protection) noexcept {
            if (process.user_mappings.active_count() >= kMaximumUserMappings) {
                return -ENOMEM;
            }

            const uint64_t aligned_hint =
                address_hint == 0U ? kAnonymousMappingBase : address_hint & ~(kPageSize - 1U);
            uint64_t candidate_hint = aligned_hint;
            UserAddressSpace address_space{process.address_space_root};
            for (;;) {
                uint64_t candidate = 0U;
                if (!process.user_mappings.find_free_range(kAnonymousMappingBase,
                                                           kAnonymousMappingLimit, length,
                                                           candidate_hint, candidate)) {
                    return -ENOMEM;
                }
                if (range_is_unmapped(address_space, candidate, length)) {
                    if (!map_pages(address_space, candidate, length, page_flags_for(protection))) {
                        return -ENOMEM;
                    }
                    if (!process.user_mappings.add(candidate, length,
                                                   static_cast<uint32_t>(protection))) {
                        static_cast<void>(unmap_owned_pages(process.user_mappings, address_space,
                                                            candidate, length));
                        for (uint64_t page = candidate; page < candidate + length;
                             page += kPageSize) {
                            if (resolve_user_physical(address_space, page) != 0U) {
                                static_cast<void>(unmap_user_page(address_space, page));
                            }
                        }
                        return -ENOMEM;
                    }
                    return static_cast<int64_t>(candidate);
                }
                if (candidate > kAnonymousMappingLimit - length - kPageSize) {
                    return -ENOMEM;
                }
                candidate_hint = candidate + kPageSize;
            }
        }

        [[nodiscard]] bool copy_user_range(const UserAddressSpace &address_space,
                                           uint64_t destination, uint64_t source,
                                           uint64_t length) noexcept {
            unsigned char buffer[kCopyBufferSize]{};
            uint64_t copied = 0U;
            while (copied < length) {
                const size_t chunk =
                    static_cast<size_t>(std::min<uint64_t>(kCopyBufferSize, length - copied));
                if (copy_from_user(buffer, source + copied, chunk) != 0 ||
                    !copy_to_user_address_space(address_space, destination + copied, buffer,
                                                chunk)) {
                    return false;
                }
                copied += chunk;
            }
            return true;
        }

    } // namespace

    int64_t process_mmap(uintptr_t address, size_t length, int protection, int flags,
                         int descriptor, uint64_t offset) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->address_space_root == 0U) {
            return -ESRCH;
        }
        if ((protection & ~(kProtectionRead | kProtectionWrite | kProtectionExecute)) != 0 ||
            (protection & (kProtectionRead | kProtectionWrite | kProtectionExecute)) == 0 ||
            (protection & (kProtectionWrite | kProtectionExecute)) ==
                (kProtectionWrite | kProtectionExecute)) {
            return -EINVAL;
        }
        if ((flags & kMapFixed) != 0) {
            return -ENOTSUP;
        }
        if (flags != (kMapPrivate | kMapAnonymous) || descriptor != -1 || offset != 0U) {
            return -EINVAL;
        }

        uint64_t aligned_length = 0U;
        if (!align_length(length, aligned_length)) {
            return -EINVAL;
        }
        return map_anonymous(*process, static_cast<uint64_t>(address), aligned_length, protection);
    }

    int64_t process_munmap(uintptr_t address, size_t length) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->address_space_root == 0U) {
            return -ESRCH;
        }
        uint64_t aligned_length = 0U;
        if (!is_page_aligned(address) || !align_length(length, aligned_length) ||
            address > kAnonymousMappingLimit - aligned_length) {
            return -EINVAL;
        }

        UserMappingTable updated_mappings = process->user_mappings;
        if (!updated_mappings.remove_range(address, aligned_length)) {
            return -ENOMEM;
        }
        UserAddressSpace address_space{process->address_space_root};
        if (!unmap_owned_pages(process->user_mappings, address_space, address, aligned_length)) {
            return -EIO;
        }
        process->user_mappings = updated_mappings;
        return 0;
    }

    int64_t process_mremap(uintptr_t old_address, size_t old_length, size_t new_length,
                           unsigned long flags, uintptr_t new_address) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr || process->address_space_root == 0U) {
            return -ESRCH;
        }
        uint64_t aligned_old_length = 0U;
        uint64_t aligned_new_length = 0U;
        // The fifth variadic argument has defined contents only with MREMAP_FIXED.
        // Fixed remapping is unsupported, so ordinary remaps must ignore it.
        static_cast<void>(new_address);
        if (!is_page_aligned(old_address) || !align_length(old_length, aligned_old_length) ||
            !align_length(new_length, aligned_new_length) ||
            (flags & ~(kRemapMayMove | kRemapFixed)) != 0U || (flags & kRemapFixed) != 0U) {
            return -EINVAL;
        }

        const int mapping_index = process->user_mappings.find_by_start(old_address);
        const UserMapping *mapping =
            mapping_index >= 0 ? process->user_mappings.get(static_cast<std::size_t>(mapping_index))
                               : nullptr;
        if (mapping == nullptr || mapping->length != aligned_old_length) {
            return -EFAULT;
        }
        if (aligned_new_length == aligned_old_length) {
            return static_cast<int64_t>(old_address);
        }

        UserAddressSpace address_space{process->address_space_root};
        if (aligned_new_length < aligned_old_length) {
            const uint64_t released_start = old_address + aligned_new_length;
            const uint64_t released_length = aligned_old_length - aligned_new_length;
            if (!unmap_owned_pages(process->user_mappings, address_space, released_start,
                                   released_length) ||
                !process->user_mappings.update_length(static_cast<std::size_t>(mapping_index),
                                                      aligned_new_length)) {
                return -EIO;
            }
            return static_cast<int64_t>(old_address);
        }

        const uint64_t additional_start = old_address + aligned_old_length;
        const uint64_t additional_length = aligned_new_length - aligned_old_length;
        if (old_address <= kAnonymousMappingLimit - aligned_new_length &&
            !process->user_mappings.overlaps(old_address, aligned_new_length, mapping_index) &&
            range_is_unmapped(address_space, additional_start, additional_length)) {
            if (!map_pages(address_space, additional_start, additional_length,
                           page_flags_for(static_cast<int>(mapping->protection)))) {
                return -ENOMEM;
            }
            if (!process->user_mappings.update_length(static_cast<std::size_t>(mapping_index),
                                                      aligned_new_length)) {
                for (uint64_t page = additional_start; page < additional_start + additional_length;
                     page += kPageSize) {
                    static_cast<void>(unmap_user_page(address_space, page));
                }
                return -ENOMEM;
            }
            return static_cast<int64_t>(old_address);
        }
        if ((flags & kRemapMayMove) == 0U) {
            return -ENOMEM;
        }

        const int protection = static_cast<int>(mapping->protection);
        const int64_t moved_address = map_anonymous(*process, 0U, aligned_new_length, protection);
        if (moved_address < 0) {
            return moved_address;
        }
        const uint64_t destination = static_cast<uint64_t>(moved_address);
        if (!copy_user_range(address_space, destination, old_address, aligned_old_length)) {
            static_cast<void>(process_munmap(destination, aligned_new_length));
            return -EFAULT;
        }
        const int64_t unmap_result = process_munmap(old_address, aligned_old_length);
        if (unmap_result != 0) {
            static_cast<void>(process_munmap(destination, aligned_new_length));
            return unmap_result;
        }
        return moved_address;
    }

} // namespace xinim::kernel::x86_64
