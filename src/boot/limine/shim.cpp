/**
 * @file shim.cpp
 * @brief Limine bootloader request shim.
 */

#include <limine.h>
#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>

extern "C" {
#if defined(__APPLE__)
#define LIMINE_REQ_SECTION
#else
#define LIMINE_REQ_SECTION __attribute__((used, section(".limine.requests")))
#endif

// Base revision marker (API rev 0)
static volatile uint64_t limine_base_revision[] LIMINE_REQ_SECTION = LIMINE_BASE_REVISION(0);

// Request declarations placed into .limine.requests (on ELF)
static volatile struct limine_memmap_request limine_memmap_request LIMINE_REQ_SECTION = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0, .response = nullptr};

static volatile struct limine_module_request limine_module_request LIMINE_REQ_SECTION = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .internal_module_count = 0,
    .internal_modules = nullptr};

static volatile struct limine_bootloader_info_request limine_bootloader_info_request
    LIMINE_REQ_SECTION = {
        .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0, .response = nullptr};

static volatile struct limine_hhdm_request limine_hhdm_request LIMINE_REQ_SECTION = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0, .response = nullptr};

static volatile struct limine_framebuffer_request limine_framebuffer_request LIMINE_REQ_SECTION = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0, .response = nullptr};

static volatile struct limine_rsdp_request limine_rsdp_request LIMINE_REQ_SECTION = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 0, .response = nullptr};

static volatile struct limine_executable_cmdline_request limine_executable_cmdline_request
    LIMINE_REQ_SECTION = {
        .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0, .response = nullptr};
}

namespace xinim::boot {
    namespace {

        BootModule g_limine_modules[kBootModuleCapacity]{};
        MemRange g_limine_memory_ranges[kLimineMemoryRangeCapacity]{};

    } // namespace

    /**
     * @brief Build BootInfo from Limine responses.
     *
     * @return BootInfo populated from Limine requests.
     */
    BootInfo from_limine() noexcept {
        BootInfo bi{};
        bi.protocol = BootProtocol::Limine;

        if (limine_executable_cmdline_request.response) {
            bi.cmdline = limine_executable_cmdline_request.response->cmdline;
        }

        if (limine_memmap_request.response != nullptr) {
            const auto *response = limine_memmap_request.response;
            size_t stored_count = 0U;
            if (normalize_limine_memory_map(response, g_limine_memory_ranges,
                                            kLimineMemoryRangeCapacity, stored_count) &&
                stored_count != 0U) {
                bi.memory_map = g_limine_memory_ranges;
                bi.memory_map_entries = stored_count;
            }
        }

        if (limine_module_request.response) {
            const auto count = static_cast<size_t>(limine_module_request.response->module_count);
            const size_t limit = count < kBootModuleCapacity ? count : kBootModuleCapacity;
            bi.modules = g_limine_modules;
            bi.modules_count = limit;
            for (size_t index = 0; index < limit; ++index) {
                const auto *module = limine_module_request.response->modules[index];
                if (module == nullptr) {
                    continue;
                }
                g_limine_modules[index].address = module->address;
                g_limine_modules[index].size = module->size;
                g_limine_modules[index].string = module->path;
            }
        }

        if (limine_hhdm_request.response) {
            bi.hhdm_offset = limine_hhdm_request.response->offset;
        }

        if (limine_rsdp_request.response) {
#if LIMINE_API_REVISION >= 1
            bi.acpi_rsdp = reinterpret_cast<const void *>(limine_rsdp_request.response->address);
#else
            bi.acpi_rsdp = limine_rsdp_request.response->address;
#endif
        }

        if (limine_framebuffer_request.response &&
            limine_framebuffer_request.response->framebuffer_count > 0 &&
            limine_framebuffer_request.response->framebuffers != nullptr) {
            auto *fb = limine_framebuffer_request.response->framebuffers[0];
            if (fb != nullptr) {
                bi.framebuffer.address = fb->address;
                bi.framebuffer.width = fb->width;
                bi.framebuffer.height = fb->height;
                bi.framebuffer.pitch = fb->pitch;
                bi.framebuffer.bpp = fb->bpp;
                bi.framebuffer.memory_model = fb->memory_model;
                bi.framebuffer.red_mask_size = fb->red_mask_size;
                bi.framebuffer.red_mask_shift = fb->red_mask_shift;
                bi.framebuffer.green_mask_size = fb->green_mask_size;
                bi.framebuffer.green_mask_shift = fb->green_mask_shift;
                bi.framebuffer.blue_mask_size = fb->blue_mask_size;
                bi.framebuffer.blue_mask_shift = fb->blue_mask_shift;
            }
        }

        bi.cpu.has_cpuid = true;
        bi.cpu.has_fpu = true;

        return bi;
    }

} // namespace xinim::boot
