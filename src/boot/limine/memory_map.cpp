#include <limine.h>
#include <xinim/boot/limine_shim.hpp>

namespace xinim::boot {

    uint32_t translate_limine_memory_type(uint64_t type) noexcept {
        switch (type) {
        case LIMINE_MEMMAP_USABLE:
            return MEMORY_RANGE_USABLE;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return MEMORY_RANGE_ACPI_RECLAIMABLE;
        case LIMINE_MEMMAP_ACPI_NVS:
            return MEMORY_RANGE_ACPI_NVS;
        case LIMINE_MEMMAP_BAD_MEMORY:
            return MEMORY_RANGE_BAD;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return MEMORY_RANGE_BOOTLOADER_RECLAIMABLE;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            return MEMORY_RANGE_KERNEL_AND_MODULES;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return MEMORY_RANGE_FRAMEBUFFER;
        case LIMINE_MEMMAP_ACPI_TABLES:
            return MEMORY_RANGE_ACPI_TABLES;
        case LIMINE_MEMMAP_RESERVED:
        default:
            return MEMORY_RANGE_RESERVED;
        }
    }

    bool normalize_limine_memory_map(const limine_memmap_response *response, MemRange *output,
                                     size_t output_capacity, size_t &output_count) noexcept {
        output_count = 0U;
        if (response == nullptr || output == nullptr || output_capacity == 0U) {
            return false;
        }

        const size_t response_count = static_cast<size_t>(response->entry_count);
        if (response_count > output_capacity) {
            return false;
        }
        if (response_count == 0U) {
            return true;
        }
        if (response->entries == nullptr) {
            return false;
        }

        for (size_t index = 0U; index < response_count; ++index) {
            const limine_memmap_entry *entry = response->entries[index];
            if (entry == nullptr) {
                output_count = 0U;
                return false;
            }
            output[index] =
                MemRange{entry->base, entry->length, translate_limine_memory_type(entry->type)};
            ++output_count;
        }
        return true;
    }

} // namespace xinim::boot
