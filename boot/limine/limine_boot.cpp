/**
 * @file limine_boot.cpp
 * @brief Limine Boot Protocol implementation for XINIM
 *
 * Complete implementation of the Limine boot protocol v5, providing
 * a modern, architecture-agnostic boot interface with support for
 * memory mapping, framebuffer, modules, and ACPI/device tree.
 */

#include <cstdint>
#include <cstring>
#include <span>
#include <array>
#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>

namespace xinim::boot::limine {

/**
 * @brief Limine protocol magic numbers
 */
static constexpr uint64_t LIMINE_MAGIC[2] = {
    0xc7b1dd30df4c8b88ULL,
    0x0a82e883a194f07bULL
};

/**
 * @brief Limine request IDs
 */
enum class RequestId : uint64_t {
    BOOTLOADER_INFO = 0xf55038d8e2a1202fULL,
    STACK_SIZE      = 0x224ef0460a8e8926ULL,
    HHDM            = 0x48dcf1cb8ad2b852ULL,
    FRAMEBUFFER     = 0x9d5827dcd881dd75ULL,
    PAGING          = 0x95a67b819a1b857eULL,
    SMP             = 0x95a67b819a1b857fULL,
    MEMORY_MAP      = 0x67cf3d9d378a806fULL,
    ENTRY_POINT     = 0x13d86c035a1cd3e1ULL,
    KERNEL_FILE     = 0xad97e90e83f1ed67ULL,
    MODULE          = 0x3e7e279702be32afULL,
    RSDP            = 0xc5e77b6b397e7b43ULL,
    SMBIOS          = 0x9e9046f11e095391ULL,
    EFI_SYSTEM_TABLE = 0x5ceba5163eaaf6d6ULL,
    BOOT_TIME       = 0x502746e184c088aaULL,
    KERNEL_ADDRESS  = 0x71ba76863cc55f63ULL,
    DTB             = 0xb40ddb48fb54bac7ULL,
};

/**
 * @brief Base structure for all Limine requests
 */
struct [[gnu::packed]] LimineRequest {
    uint64_t id;
    uint64_t revision;
    void* response;
};

/**
 * @brief Memory map entry types
 */
enum class MemoryType : uint64_t {
    USABLE                = 0,
    RESERVED              = 1,
    ACPI_RECLAIMABLE     = 2,
    ACPI_NVS             = 3,
    BAD_MEMORY           = 4,
    BOOTLOADER_RECLAIMABLE = 5,
    KERNEL_AND_MODULES   = 6,
    FRAMEBUFFER          = 7
};

/**
 * @brief Memory map entry
 */
struct [[gnu::packed]] LimineMemoryMapEntry {
    uint64_t base;
    uint64_t length;
    MemoryType type;
};

/**
 * @brief Memory map response
 */
struct [[gnu::packed]] LimineMemoryMapResponse {
    uint64_t revision;
    uint64_t entry_count;
    LimineMemoryMapEntry** entries;
};

/**
 * @brief HHDM (Higher Half Direct Map) response
 */
struct [[gnu::packed]] LimineHhdmResponse {
    uint64_t revision;
    uint64_t offset;
};

/**
 * @brief Framebuffer structure
 */
struct [[gnu::packed]] LimineFramebuffer {
    void* address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void* edid;
};

/**
 * @brief Framebuffer response
 */
struct [[gnu::packed]] LimineFramebufferResponse {
    uint64_t revision;
    uint64_t framebuffer_count;
    LimineFramebuffer** framebuffers;
};

/**
 * @brief Module structure
 */
struct [[gnu::packed]] LimineFile {
    uint64_t revision;
    void* address;
    uint64_t size;
    char* path;
    char* cmdline;
    uint64_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    uint8_t gpt_disk_uuid[16];
    uint8_t gpt_part_uuid[16];
    uint8_t part_uuid[16];
};

/**
 * @brief Module response
 */
struct [[gnu::packed]] LimineModuleResponse {
    uint64_t revision;
    uint64_t module_count;
    LimineFile** modules;
};

/**
 * @brief RSDP response
 */
struct [[gnu::packed]] LimineRsdpResponse {
    uint64_t revision;
    void* address;
};

/**
 * @brief SMP CPU info
 */
struct [[gnu::packed]] LimineSmpInfo {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t reserved;
    void (*goto_address)(struct LimineSmpInfo*);
    uint64_t extra_argument;
};

/**
 * @brief SMP response
 */
struct [[gnu::packed]] LimineSmpResponse {
    uint64_t revision;
    uint32_t flags;
    uint32_t bsp_lapic_id;
    uint64_t cpu_count;
    LimineSmpInfo** cpus;
};

/**
 * @brief Bootloader info response
 */
struct [[gnu::packed]] LimineBootloaderInfoResponse {
    uint64_t revision;
    char* name;
    char* version;
};

/**
 * @brief Kernel address response
 */
struct [[gnu::packed]] LimineKernelAddressResponse {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

/**
 * @brief Boot time response
 */
struct [[gnu::packed]] LimineBootTimeResponse {
    uint64_t revision;
    int64_t boot_time;  // UNIX timestamp
};

/**
 * @brief Complete set of Limine requests
 */
struct LimineRequests {
    // Memory and addressing
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest memory_map_request = {
        .id = static_cast<uint64_t>(RequestId::MEMORY_MAP),
        .revision = 0,
        .response = nullptr
    };
    
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest hhdm_request = {
        .id = static_cast<uint64_t>(RequestId::HHDM),
        .revision = 0,
        .response = nullptr
    };
    
    // Display
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest framebuffer_request = {
        .id = static_cast<uint64_t>(RequestId::FRAMEBUFFER),
        .revision = 0,
        .response = nullptr
    };
    
    // Modules and files
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest module_request = {
        .id = static_cast<uint64_t>(RequestId::MODULE),
        .revision = 0,
        .response = nullptr
    };
    
    // Firmware tables
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest rsdp_request = {
        .id = static_cast<uint64_t>(RequestId::RSDP),
        .revision = 0,
        .response = nullptr
    };
    
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest smbios_request = {
        .id = static_cast<uint64_t>(RequestId::SMBIOS),
        .revision = 0,
        .response = nullptr
    };
    
    // SMP
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest smp_request = {
        .id = static_cast<uint64_t>(RequestId::SMP),
        .revision = 0,
        .response = nullptr
    };
    
    // Boot info
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest bootloader_info_request = {
        .id = static_cast<uint64_t>(RequestId::BOOTLOADER_INFO),
        .revision = 0,
        .response = nullptr
    };
    
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest kernel_address_request = {
        .id = static_cast<uint64_t>(RequestId::KERNEL_ADDRESS),
        .revision = 0,
        .response = nullptr
    };
    
    [[gnu::section(".limine_reqs"), gnu::used]]
    static inline volatile LimineRequest boot_time_request = {
        .id = static_cast<uint64_t>(RequestId::BOOT_TIME),
        .revision = 0,
        .response = nullptr
    };
};

/**
 * @brief Convert Limine memory type to XINIM memory type
 */
static MemoryRegionType convert_memory_type(MemoryType type) noexcept {
    switch (type) {
        case MemoryType::USABLE:
            return MemoryRegionType::AVAILABLE;
        case MemoryType::RESERVED:
            return MemoryRegionType::RESERVED;
        case MemoryType::ACPI_RECLAIMABLE:
            return MemoryRegionType::ACPI_RECLAIMABLE;
        case MemoryType::ACPI_NVS:
            return MemoryRegionType::ACPI_NVS;
        case MemoryType::BAD_MEMORY:
            return MemoryRegionType::BAD_MEMORY;
        case MemoryType::BOOTLOADER_RECLAIMABLE:
            return MemoryRegionType::BOOTLOADER_RECLAIMABLE;
        case MemoryType::KERNEL_AND_MODULES:
            return MemoryRegionType::KERNEL_AND_MODULES;
        case MemoryType::FRAMEBUFFER:
            return MemoryRegionType::FRAMEBUFFER;
        default:
            return MemoryRegionType::RESERVED;
    }
}

/**
 * @brief Parse Limine boot information
 */
BootInfo parse_limine_boot_info() noexcept {
    BootInfo info{};
    
    // Get HHDM offset
    if (auto* hhdm_resp = reinterpret_cast<LimineHhdmResponse*>(
            LimineRequests::hhdm_request.response)) {
        info.hhdm_offset = hhdm_resp->offset;
    }
    
    // Get memory map
    if (auto* mmap_resp = reinterpret_cast<LimineMemoryMapResponse*>(
            LimineRequests::memory_map_request.response)) {
        
        info.memory_map_entries = std::min(mmap_resp->entry_count, 
                                          static_cast<uint64_t>(BootInfo::MAX_MEMORY_REGIONS));
        
        uint64_t total_memory = 0;
        for (uint64_t i = 0; i < info.memory_map_entries; ++i) {
            auto* entry = mmap_resp->entries[i];
            info.memory_map[i] = {
                .base = entry->base,
                .length = entry->length,
                .type = convert_memory_type(entry->type)
            };
            
            if (entry->type == MemoryType::USABLE) {
                total_memory += entry->length;
            }
        }
        info.total_memory = total_memory;
    }
    
    // Get framebuffer info
    if (auto* fb_resp = reinterpret_cast<LimineFramebufferResponse*>(
            LimineRequests::framebuffer_request.response)) {
        
        if (fb_resp->framebuffer_count > 0 && fb_resp->framebuffers[0]) {
            auto* fb = fb_resp->framebuffers[0];
            info.framebuffer_addr = reinterpret_cast<uintptr_t>(fb->address);
            info.framebuffer_width = static_cast<uint32_t>(fb->width);
            info.framebuffer_height = static_cast<uint32_t>(fb->height);
            info.framebuffer_pitch = static_cast<uint32_t>(fb->pitch);
            info.framebuffer_bpp = fb->bpp;
        }
    }
    
    // Get ACPI RSDP
    if (auto* rsdp_resp = reinterpret_cast<LimineRsdpResponse*>(
            LimineRequests::rsdp_request.response)) {
        info.acpi_rsdp = rsdp_resp->address;
    }
    
    // Get modules
    if (auto* mod_resp = reinterpret_cast<LimineModuleResponse*>(
            LimineRequests::module_request.response)) {
        
        info.module_count = std::min(mod_resp->module_count,
                                    static_cast<uint64_t>(BootInfo::MAX_MODULES));
        
        for (uint64_t i = 0; i < info.module_count; ++i) {
            auto* mod = mod_resp->modules[i];
            info.modules[i] = {
                .address = reinterpret_cast<uintptr_t>(mod->address),
                .size = mod->size,
                .name = {0}  // Copy name if needed
            };
            
            // Copy module name (truncate if too long)
            if (mod->path) {
                size_t len = std::strlen(mod->path);
                len = std::min(len, sizeof(info.modules[i].name) - 1);
                std::memcpy(info.modules[i].name, mod->path, len);
            }
        }
    }
    
    // Get SMP info
    if (auto* smp_resp = reinterpret_cast<LimineSmpResponse*>(
            LimineRequests::smp_request.response)) {
        info.cpu_count = static_cast<uint32_t>(smp_resp->cpu_count);
        info.bsp_lapic_id = smp_resp->bsp_lapic_id;
    } else {
        info.cpu_count = 1;  // At least BSP
    }
    
    // Get kernel address info
    if (auto* kaddr_resp = reinterpret_cast<LimineKernelAddressResponse*>(
            LimineRequests::kernel_address_request.response)) {
        info.kernel_physical_base = kaddr_resp->physical_base;
        info.kernel_virtual_base = kaddr_resp->virtual_base;
    }
    
    // Get boot time
    if (auto* time_resp = reinterpret_cast<LimineBootTimeResponse*>(
            LimineRequests::boot_time_request.response)) {
        info.boot_time = static_cast<uint64_t>(time_resp->boot_time);
    }
    
    // Get bootloader info
    if (auto* bl_resp = reinterpret_cast<LimineBootloaderInfoResponse*>(
            LimineRequests::bootloader_info_request.response)) {
        if (bl_resp->name) {
            size_t len = std::strlen(bl_resp->name);
            len = std::min(len, sizeof(info.bootloader_name) - 1);
            std::memcpy(info.bootloader_name, bl_resp->name, len);
        }
    }
    
    // Set architecture
#ifdef XINIM_ARCH_X86_64
    info.arch = Architecture::X86_64;
#elif defined(XINIM_ARCH_ARM64)
    info.arch = Architecture::ARM64;
#else
    info.arch = Architecture::UNKNOWN;
#endif
    
    return info;
}

/**
 * @brief Initialize application processors (APs)
 */
void init_smp(void (*ap_entry)(LimineSmpInfo*)) noexcept {
    if (auto* smp_resp = reinterpret_cast<LimineSmpResponse*>(
            LimineRequests::smp_request.response)) {
        
        for (uint64_t i = 0; i < smp_resp->cpu_count; ++i) {
            auto* cpu = smp_resp->cpus[i];
            
            // Skip BSP
            if (cpu->lapic_id == smp_resp->bsp_lapic_id) {
                continue;
            }
            
            // Start AP
            cpu->goto_address = ap_entry;
            
            // Memory barrier to ensure the AP sees the updated goto_address
            asm volatile("" ::: "memory");
        }
    }
}

/**
 * @brief Get a specific module by name
 */
ModuleInfo* find_module(const char* name) noexcept {
    static ModuleInfo module_cache[BootInfo::MAX_MODULES];
    
    if (auto* mod_resp = reinterpret_cast<LimineModuleResponse*>(
            LimineRequests::module_request.response)) {
        
        for (uint64_t i = 0; i < mod_resp->module_count; ++i) {
            auto* mod = mod_resp->modules[i];
            
            if (mod->path && std::strstr(mod->path, name)) {
                module_cache[i] = {
                    .address = reinterpret_cast<uintptr_t>(mod->address),
                    .size = mod->size,
                    .name = {0}
                };
                
                // Copy name
                size_t len = std::strlen(mod->path);
                len = std::min(len, sizeof(module_cache[i].name) - 1);
                std::memcpy(module_cache[i].name, mod->path, len);
                
                return &module_cache[i];
            }
        }
    }
    
    return nullptr;
}

} // namespace xinim::boot::limine

namespace xinim::boot {

/**
 * @brief Main entry point to parse Limine boot information
 */
BootInfo from_limine() noexcept {
    return limine::parse_limine_boot_info();
}

} // namespace xinim::boot