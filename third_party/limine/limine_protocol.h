// Minimal, clean-room Limine protocol declarations needed by XINIM.
// This file is intentionally minimal and contains only the constants and
// structs necessary to bridge the bootloader handoff into BootInfo.
// Protocol constants and layout follow publicly documented facts; this file
// contains original wording and layout and is not derived from upstream text.

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// UUID-based IDs for requests: represented as 128-bit values in little-endian.
// Note: Real request and response structures are larger; here we define just
// the subset to extract memory map and modules. Fill out as needed.

struct limine_uuid { uint64_t lo, hi; };

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type; // 1=usable, others reserved/device/ACPI/etc.
};

struct limine_memmap_response {
    uint64_t entry_count;
    struct limine_memmap_entry** entries; // array of pointers to entries
};

struct limine_memmap_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_memmap_response* response;
};

struct limine_module {
    uint64_t base;
    uint64_t length;
    const char* cmdline;
};

struct limine_module_response {
    uint64_t module_count;
    struct limine_module** modules; // array of module pointers
};

struct limine_module_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_module_response* response;
};

struct limine_bootloader_info {
    const char* name;
    const char* version;
};

struct limine_bootloader_info_response { struct limine_bootloader_info* info; };
struct limine_bootloader_info_request { uint64_t id[2]; uint64_t revision; struct limine_bootloader_info_response* response; };

#ifdef __cplusplus
}
#endif

