#include "user_backing.hpp"

#include "dma_pages.hpp"

namespace xinim::i486::user_backing {
    namespace {

        struct ImageSlot {
            uint8_t *address;
            bool in_use;
        };

        ImageSlot g_images[kMaximumImages]{};
        uint32_t g_live_images = 0U;
        uint32_t g_reserved_images = 0U;

        void clear_image(uint8_t *image) noexcept {
            for (uint32_t offset = 0U; offset < kImageBytes; ++offset) {
                image[offset] = 0U;
            }
        }

    } // namespace

    uint8_t *acquire() noexcept {
        for (auto &slot : g_images) {
            if (slot.address != nullptr && !slot.in_use) {
                clear_image(slot.address);
                slot.in_use = true;
                ++g_live_images;
                return slot.address;
            }
        }
        for (auto &slot : g_images) {
            if (slot.address == nullptr) {
                const dma::DmaBuffer backing = dma::allocate(kImageBytes);
                if (backing.address == nullptr || backing.size != kImageBytes) {
                    return nullptr;
                }
                slot.address = backing.address;
                slot.in_use = true;
                ++g_live_images;
                ++g_reserved_images;
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
