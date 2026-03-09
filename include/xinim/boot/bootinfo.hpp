#pragma once
#include <stdint.h>
#include <stddef.h>

namespace xinim::boot {

struct MemRange { uint64_t base{}, length{}; uint32_t type{}; };

struct BootModule {
    const void* address{nullptr};
    uint64_t size{0};
    const char* string{nullptr};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return address != nullptr && size != 0;
    }
};

enum class BootProtocol : uint8_t {
    Unknown = 0,
    Limine = 1,
    Multiboot2 = 2,
};

struct FramebufferInfo {
    void* address{nullptr};
    uint64_t width{0};
    uint64_t height{0};
    uint64_t pitch{0};
    uint16_t bpp{0};
    uint8_t memory_model{0};
    uint8_t red_mask_size{0};
    uint8_t red_mask_shift{0};
    uint8_t green_mask_size{0};
    uint8_t green_mask_shift{0};
    uint8_t blue_mask_size{0};
    uint8_t blue_mask_shift{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return address != nullptr && width != 0 && height != 0 && pitch != 0;
    }
};

struct CpuBootInfo {
    bool has_cpuid{false};
    bool has_fpu{false};
};

struct BootInfo {
    BootProtocol protocol{BootProtocol::Unknown};
    const char* cmdline{nullptr};
    const void* acpi_rsdp{nullptr};
    uint64_t hhdm_offset{0};
    const MemRange* memory_map{nullptr};
    size_t memory_map_entries{0};
    const BootModule* modules{nullptr};
    size_t modules_count{0};
    FramebufferInfo framebuffer{};
    CpuBootInfo cpu{};

    [[nodiscard]] constexpr bool has_framebuffer() const noexcept {
        return framebuffer.valid();
    }
};

} // namespace xinim::boot
