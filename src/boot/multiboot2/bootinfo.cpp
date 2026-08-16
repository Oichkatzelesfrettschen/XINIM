#include "xinim/boot/multiboot2_shim.hpp"

namespace xinim::boot {
    namespace {

        constexpr uint32_t kMultiboot2BootMagic = 0x36d76289U;
        constexpr uint32_t kTagTypeEnd = 0U;
        constexpr uint32_t kTagTypeCmdline = 1U;
        constexpr uint32_t kTagTypeModule = 3U;
        constexpr uint32_t kTagTypeMmap = 6U;
        constexpr uint32_t kTagTypeFramebuffer = 8U;
        constexpr uint32_t kTagTypeAcpiOld = 14U;
        constexpr uint32_t kTagTypeAcpiNew = 15U;
        constexpr size_t kMaxMemoryRanges = kBootMemoryRangeCapacity;

        struct MultibootInfoHeader {
            uint32_t total_size;
            uint32_t reserved;
        };

        struct MultibootTag {
            uint32_t type;
            uint32_t size;
        };

        struct MultibootTagMmap {
            uint32_t type;
            uint32_t size;
            uint32_t entry_size;
            uint32_t entry_version;
        };

        struct MultibootMmapEntry {
            uint64_t addr;
            uint64_t len;
            uint32_t type;
            uint32_t zero;
        } __attribute__((packed));

        struct MultibootTagModule {
            uint32_t type;
            uint32_t size;
            uint32_t mod_start;
            uint32_t mod_end;
            char string[1];
        } __attribute__((packed));

        struct MultibootTagFramebuffer {
            uint32_t type;
            uint32_t size;
            uint64_t address;
            uint32_t pitch;
            uint32_t width;
            uint32_t height;
            uint8_t bpp;
            uint8_t framebuffer_type;
            uint16_t reserved;
            uint8_t red_field_position;
            uint8_t red_mask_size;
            uint8_t green_field_position;
            uint8_t green_mask_size;
            uint8_t blue_field_position;
            uint8_t blue_mask_size;
        } __attribute__((packed));

        alignas(16) MemRange g_memory_ranges[kMaxMemoryRanges]{};
        alignas(16) BootModule g_boot_modules[kBootModuleCapacity]{};

        [[nodiscard]] constexpr uintptr_t align_up_8(uintptr_t value) noexcept {
            return (value + 7U) & ~static_cast<uintptr_t>(7U);
        }

        [[nodiscard]] bool detect_cpuid() noexcept {
            uint32_t original = 0U;
            uint32_t toggled = 0U;

            asm volatile("pushfl\n\t"
                         "popl %0"
                         : "=r"(original));

            const uint32_t desired = original ^ (1U << 21U);

            asm volatile("pushl %0\n\t"
                         "popfl\n\t"
                         :
                         : "r"(desired)
                         : "cc");

            asm volatile("pushfl\n\t"
                         "popl %0"
                         : "=r"(toggled));

            asm volatile("pushl %0\n\t"
                         "popfl\n\t"
                         :
                         : "r"(original)
                         : "cc");

            return ((toggled ^ original) & (1U << 21U)) != 0U;
        }

        [[nodiscard]] uint32_t read_cr0() noexcept {
            uint32_t value = 0U;
            asm volatile("mov %%cr0, %0" : "=r"(value));
            return value;
        }

    } // namespace

    BootInfo from_multiboot2(uint32_t magic, uintptr_t info_addr) noexcept {
        BootInfo info{};
        if (magic != kMultiboot2BootMagic || info_addr == 0U) {
            return info;
        }

        info.protocol = BootProtocol::Multiboot2;
        info.cpu.has_cpuid = detect_cpuid();
        info.cpu.has_fpu = (read_cr0() & (1U << 2U)) == 0U;
        info.memory_map = g_memory_ranges;
        info.modules = g_boot_modules;

        const auto *header = reinterpret_cast<const MultibootInfoHeader *>(info_addr);
        uintptr_t cursor = info_addr + sizeof(MultibootInfoHeader);
        const uintptr_t end = info_addr + header->total_size;

        while (cursor + sizeof(MultibootTag) <= end) {
            const auto *tag = reinterpret_cast<const MultibootTag *>(cursor);
            if (tag->type == kTagTypeEnd) {
                break;
            }
            if (tag->size < sizeof(MultibootTag)) {
                break;
            }

            switch (tag->type) {
            case kTagTypeCmdline:
                info.cmdline = reinterpret_cast<const char *>(cursor + sizeof(MultibootTag));
                break;
            case kTagTypeModule: {
                if (info.modules_count >= kBootModuleCapacity) {
                    break;
                }
                const auto *module_tag = reinterpret_cast<const MultibootTagModule *>(tag);
                BootModule &module = g_boot_modules[info.modules_count];
                module.address =
                    reinterpret_cast<const void *>(static_cast<uintptr_t>(module_tag->mod_start));
                module.size = static_cast<uint64_t>(module_tag->mod_end - module_tag->mod_start);
                module.string = module_tag->string;
                ++info.modules_count;
                break;
            }
            case kTagTypeMmap: {
                const auto *mmap_tag = reinterpret_cast<const MultibootTagMmap *>(tag);
                if (mmap_tag->entry_size < sizeof(MultibootMmapEntry)) {
                    break;
                }
                const uintptr_t entries_begin = cursor + sizeof(MultibootTagMmap);
                const uintptr_t entries_end = cursor + tag->size;
                size_t count = 0U;

                for (uintptr_t entry_addr = entries_begin;
                     entry_addr + sizeof(MultibootMmapEntry) <= entries_end &&
                     count < kMaxMemoryRanges;
                     entry_addr += mmap_tag->entry_size) {
                    const auto *entry = reinterpret_cast<const MultibootMmapEntry *>(entry_addr);
                    g_memory_ranges[count].base = entry->addr;
                    g_memory_ranges[count].length = entry->len;
                    g_memory_ranges[count].type = entry->type;
                    ++count;
                }

                info.memory_map_entries = count;
                break;
            }
            case kTagTypeFramebuffer: {
                const auto *fb_tag = reinterpret_cast<const MultibootTagFramebuffer *>(tag);
                info.framebuffer.address =
                    reinterpret_cast<void *>(static_cast<uintptr_t>(fb_tag->address));
                info.framebuffer.width = fb_tag->width;
                info.framebuffer.height = fb_tag->height;
                info.framebuffer.pitch = fb_tag->pitch;
                info.framebuffer.bpp = fb_tag->bpp;
                info.framebuffer.memory_model = fb_tag->framebuffer_type;
                info.framebuffer.red_mask_shift = fb_tag->red_field_position;
                info.framebuffer.red_mask_size = fb_tag->red_mask_size;
                info.framebuffer.green_mask_shift = fb_tag->green_field_position;
                info.framebuffer.green_mask_size = fb_tag->green_mask_size;
                info.framebuffer.blue_mask_shift = fb_tag->blue_field_position;
                info.framebuffer.blue_mask_size = fb_tag->blue_mask_size;
                break;
            }
            case kTagTypeAcpiOld:
            case kTagTypeAcpiNew:
                info.acpi_rsdp = reinterpret_cast<const void *>(cursor + sizeof(MultibootTag));
                break;
            default:
                break;
            }

            cursor = align_up_8(cursor + tag->size);
        }

        return info;
    }

} // namespace xinim::boot
