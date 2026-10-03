#include "user_backing.hpp"

#include "dma_pages.hpp"
#include "elf32_loader.hpp"

#include <stddef.h>

namespace xinim::i486::user_backing {
    static_assert(elf32::kUserVirtualBase == kImageOffsetBytes);
    static_assert(elf32::kUserAddressSpaceSize == kImageBytes);

    namespace {

        struct ImageSlot {
            uint8_t *address;
            bool in_use;
        };

        ImageSlot g_images[kMaximumImages]{};
        uint32_t g_live_images = 0U;
        uint32_t g_reserved_images = 0U;
        uint32_t g_capacity_images = 0U;
        // Two 256-entry virtqueues and 768 KiB of RX/TX buffers fit below this floor.
        constexpr uint32_t kDeviceBudgetBytes = 1024U * 1024U;

        // Ring 3 reaches the whole window, including the guard span below the
        // image, so a new tenant must observe none of the previous tenant's bytes.
        void clear_window(uint8_t *image) noexcept {
            uint8_t *const window = image - kImageOffsetBytes;
            for (uint32_t offset = 0U; offset < kSlotBytes; ++offset) {
                window[offset] = 0U;
            }
        }

    } // namespace

    bool initialize() noexcept {
        if (g_live_images != 0U) {
            return false;
        }
        for (auto &slot : g_images) {
            slot = {nullptr, false};
        }
        g_capacity_images = 0U;
        g_reserved_images = 0U;
        const uint32_t available = dma::available_bytes();
        uint32_t capacity =
            available > kDeviceBudgetBytes ? (available - kDeviceBudgetBytes) / kSlotBytes : 0U;
        if (capacity > kMaximumImages) {
            capacity = kMaximumImages;
        }
        if (capacity < kMinimumImages) {
            return false;
        }
        const dma::DmaBuffer arena = dma::reserve(capacity * kSlotBytes);
        if (arena.address == nullptr) {
            return false;
        }
        for (uint32_t index = 0U; index < kMaximumImages; ++index) {
            g_images[index] = {index < capacity
                                   ? arena.address + static_cast<size_t>(index) * kSlotBytes +
                                         kImageOffsetBytes
                                   : nullptr,
                               false};
        }
        g_capacity_images = capacity;
        return true;
    }

    uint32_t capacity_images() noexcept {
        return g_capacity_images;
    }
    uint32_t capacity_bytes() noexcept {
        return g_capacity_images * kSlotBytes;
    }

    uint8_t *acquire() noexcept {
        for (auto &slot : g_images) {
            if (slot.address != nullptr && !slot.in_use) {
                clear_window(slot.address);
                slot.in_use = true;
                ++g_live_images;
                if (g_live_images > g_reserved_images) {
                    g_reserved_images = g_live_images;
                }
                return slot.address;
            }
        }
        return nullptr;
    }

    bool release(uint8_t *image) noexcept {
        if (image == nullptr) {
            return false;
        }
        for (auto &slot : g_images) {
            if (slot.address == image && slot.in_use) {
                slot.in_use = false;
                --g_live_images;
                return true;
            }
        }
        return false;
    }

    uint32_t live_bytes() noexcept {
        return g_live_images * kImageBytes;
    }

    uint32_t reserved_bytes() noexcept {
        return g_reserved_images * kImageBytes;
    }

} // namespace xinim::i486::user_backing
