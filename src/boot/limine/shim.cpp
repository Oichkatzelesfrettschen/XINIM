/**
 * @file shim.cpp
 * @brief Limine bootloader request shim.
 */

#include <limine.h>
#include <xinim/boot/bootinfo.hpp>

extern "C" {
#if defined(__APPLE__)
#  define LIMINE_REQ_SECTION
#else
#  define LIMINE_REQ_SECTION __attribute__((used, section(".limine.requests")))
#endif

// Base revision marker (API rev 0)
static volatile uint64_t limine_base_revision[] LIMINE_REQ_SECTION = LIMINE_BASE_REVISION(0);

// Request declarations placed into .limine.requests (on ELF)
static volatile struct limine_memmap_request limine_memmap_request LIMINE_REQ_SECTION = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_module_request limine_module_request LIMINE_REQ_SECTION = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .internal_module_count = 0,
    .internal_modules = nullptr
};

static volatile struct limine_bootloader_info_request limine_bootloader_info_request LIMINE_REQ_SECTION = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_hhdm_request limine_hhdm_request LIMINE_REQ_SECTION = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_rsdp_request limine_rsdp_request LIMINE_REQ_SECTION = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

static volatile struct limine_executable_cmdline_request limine_executable_cmdline_request LIMINE_REQ_SECTION = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};
} 

namespace xinim::boot {

/**
 * @brief Convert Limine memory map types to BootInfo types.
 *
 * @param t Limine memory map type.
 * @return BootInfo-compatible memory type.
 */
[[maybe_unused]] static uint32_t translate_type(uint64_t t) {
    // Map Limine numeric types to our generic ones; for now pass through.
    return static_cast<uint32_t>(t);
}

/**
 * @brief Build BootInfo from Limine responses.
 *
 * @return BootInfo populated from Limine requests.
 */
BootInfo from_limine() {
    BootInfo bi{};

    if (limine_executable_cmdline_request.response) {
        bi.cmdline = limine_executable_cmdline_request.response->cmdline;
    }

    if (limine_memmap_request.response) {
        auto resp = limine_memmap_request.response;
        bi.memory_map_entries = static_cast<size_t>(resp->entry_count);
        // Copy into a static buffer for early use (implementation left to kernel arena).
    }

    if (limine_module_request.response) {
        bi.modules_count = static_cast<size_t>(limine_module_request.response->module_count);
        // Copy handling TBD once early allocator is available.
    }

    if (limine_hhdm_request.response) {
        bi.hhdm_offset = limine_hhdm_request.response->offset;
    }

    if (limine_rsdp_request.response) {
        #if LIMINE_API_REVISION >= 1
        bi.acpi_rsdp = reinterpret_cast<const void*>(limine_rsdp_request.response->address);
        #else
        bi.acpi_rsdp = limine_rsdp_request.response->address;
        #endif
    }

    return bi;
}

} // namespace xinim::boot
