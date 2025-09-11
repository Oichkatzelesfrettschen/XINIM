#include "console.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "multiboot.hpp"
#include <stddef.h> // For NULL, size_t
#include <stdint.h> // For uintptr_t

// Placeholder for linker symbols. These must be defined in linker.ld.
// These are PHYSICAL addresses for the kernel's load location.
extern "C" uint8_t _kernel_physical_start[];
extern "C" uint8_t _kernel_physical_end[];
// These are VIRTUAL addresses the kernel expects to run at.
// For identity mapping at 1MB, virtual start = physical start.
extern "C" uint8_t _kernel_virtual_start[];
extern "C" uint8_t _kernel_virtual_end[];


// Store the Multiboot info globally for now (physical address)
static uintptr_t multiboot_info_physical_addr = 0;
static struct multiboot_tag_mmap* memory_map_tag = NULL;

// multiboot_tag_string was missing from my initial multiboot.h
struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[];
};


void parse_multiboot_info(uintptr_t mb_info_addr) {
    multiboot_info_physical_addr = mb_info_addr;
    struct multiboot_tag* tag;
    // The first 4 bytes at mb_info_addr is the total size of the boot information.
    // The next 4 bytes are reserved (must be zero).
    // Tags begin after these 8 bytes.
    // unsigned int mb_info_size = *(unsigned int*)mb_info_addr;

    console_write_string("Multiboot info @ 0x", DEFAULT_COLOR); console_write_hex(mb_info_addr, DEFAULT_COLOR);
    // console_write_string(" (Declared Size: ", DEFAULT_COLOR); console_write_dec(mb_info_size, DEFAULT_COLOR);
    // console_write_string(" bytes)\n", DEFAULT_COLOR);

    for (tag = (struct multiboot_tag*)(mb_info_addr + 8); // Skip total_size and reserved fields
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + multiboot_tag_align(tag->size))) {

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            memory_map_tag = (struct multiboot_tag_mmap*)tag;
            console_write_string("Found Multiboot Memory Map Tag @ 0x", DEFAULT_COLOR); console_write_hex((uintptr_t)memory_map_tag, DEFAULT_COLOR);
            console_write_string("\n  Entry size: ", DEFAULT_COLOR); console_write_dec(memory_map_tag->entry_size, DEFAULT_COLOR);
            console_write_string(", Num Entries (approx): ", DEFAULT_COLOR); console_write_dec((memory_map_tag->size - (sizeof(uint32_t)*4)) / memory_map_tag->entry_size, DEFAULT_COLOR);
            console_write_char('\n', DEFAULT_COLOR);
        } else if (tag->type == MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME) {
            struct multiboot_tag_string* bootloader_tag = (struct multiboot_tag_string*)tag;
            console_write_string("Bootloader: ", DEFAULT_COLOR);
            console_write_string(bootloader_tag->string, DEFAULT_COLOR);
            console_write_char('\n', DEFAULT_COLOR);
        }
        // Add more tag parsing if needed (e.g. framebuffer)
    }
}


extern "C" void kmain(unsigned long multiboot_magic, unsigned long multiboot_addr) {
    // Initialize console early (uses direct VGA physical address 0xB8000 initially)
    console_init(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    console_write_string("Console Initialized.\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

    // These will be unresolved until linker script is updated.
    // For now, to allow compilation, we can provide temporary weak definitions or expect errors.
    // Let's assume they will be linked.
    console_write_string("Kernel Physical Start (Linker Symbol): 0x", DEFAULT_COLOR); console_write_hex((uintptr_t)_kernel_physical_start, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
    console_write_string("Kernel Physical End (Linker Symbol):   0x", DEFAULT_COLOR); console_write_hex((uintptr_t)_kernel_physical_end, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
    console_write_string("Kernel Virtual Start (Linker Symbol):  0x", DEFAULT_COLOR); console_write_hex((uintptr_t)_kernel_virtual_start, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
    console_write_string("Kernel Virtual End (Linker Symbol):    0x", DEFAULT_COLOR); console_write_hex((uintptr_t)_kernel_virtual_end, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);


    const unsigned long MULTIBOOT2_BOOTLOADER_MAGIC = 0x36d76289;
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        console_write_string("ERROR: Invalid Multiboot2 magic number: 0x", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        console_write_hex(multiboot_magic, vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        console_write_string("\nSystem Halted due to invalid Multiboot magic.\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        // Infinite loop to halt the system
        while (true) {
#ifdef __x86_64__
            asm volatile("cli; hlt");
#elif defined(__aarch64__)
            asm volatile("wfi"); // Wait for interrupt
#endif
        }
    }
    console_write_string("Multiboot2 magic verified.\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

    bool boot_success = true; // Flag to track boot process
    // Variables used in the main boot sequence block
    uintptr_t pd_phys_addr = 0;
    uintptr_t test_page_phys = 0;

    do { // Using do-while(false) to create a breakable block for sequential operations that can fail
        // Parse Multiboot information
        parse_multiboot_info(multiboot_addr);

        if (!memory_map_tag) {
            console_write_string("ERROR: Multiboot Memory Map not found!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            boot_success = false; break;
        }

        // Initialize Physical Memory Manager (PMM)
        pmm_init(memory_map_tag, (uintptr_t)_kernel_physical_start, (uintptr_t)_kernel_physical_end, multiboot_info_physical_addr);
        console_write_string("PMM initialized.\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        console_write_string("Total RAM: ", DEFAULT_COLOR); console_write_dec(pmm_get_total_pages() * PAGE_SIZE / (1024*1024), DEFAULT_COLOR); console_write_string(" MB\n", DEFAULT_COLOR);
        console_write_string("Used RAM:  ", DEFAULT_COLOR); console_write_dec(pmm_get_used_pages() * PAGE_SIZE / (1024*1024), DEFAULT_COLOR); console_write_string(" MB\n", DEFAULT_COLOR);
        console_write_string("Free RAM:  ", DEFAULT_COLOR); console_write_dec(pmm_get_free_pages() * PAGE_SIZE / (1024*1024), DEFAULT_COLOR); console_write_string(" MB\n", DEFAULT_COLOR);

        // Initialize Virtual Memory Manager (VMM)
        vmm_init((uintptr_t)_kernel_physical_start, (uintptr_t)_kernel_physical_end,
                 (uintptr_t)_kernel_virtual_start, (uintptr_t)_kernel_virtual_end);
        console_write_string("VMM initialized.\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

        // Load the new page directory
        pd_phys_addr = vmm_get_page_directory_physical_addr();
        if (pd_phys_addr == 0) { // Check if VMM init failed to set page directory
            console_write_string("ERROR: Page Directory physical address is NULL after VMM init!\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            boot_success = false; break;
        }
        console_write_string("Loading Page Directory from Phys Addr: 0x", DEFAULT_COLOR); console_write_hex(pd_phys_addr, DEFAULT_COLOR); console_write_char('\n', DEFAULT_COLOR);
        vmm_load_page_directory(pd_phys_addr);
        console_write_string("Page Directory Loaded (CR3 set).\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

        // Enable Paging
        console_write_string("Enabling Paging...\n", vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        vmm_enable_paging();

        console_write_string("Paging Enabled! Hope this still works!\n", vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        console_write_string("If you see this, VMM and console mapping are (probably) working.\n", DEFAULT_COLOR);

        console_write_string("Testing PMM allocation after paging: ", DEFAULT_COLOR);
        test_page_phys = pmm_alloc_page();
        if (test_page_phys) {
            console_write_string("Allocated page at Phys 0x", DEFAULT_COLOR); console_write_hex(test_page_phys, DEFAULT_COLOR);
            console_write_string(". Freeing it.\n", DEFAULT_COLOR);
            pmm_free_page(test_page_phys);
        } else {
            console_write_string("Failed to allocate test page.\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            // This might not be critical enough to halt, depending on OS design
        }

    } while(false); // End of breakable block

    if (!boot_success) {
        console_write_string("Halting due to critical error during boot sequence.\n", vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
    }

// halt_system: // Label for explicit halt, now only used if boot_success is false or at end of kmain
    console_write_string("System Halted.\n", vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    while (true) {
#ifdef __x86_64__
        asm volatile("cli; hlt");
#elif defined(__aarch64__)
        asm volatile("wfi"); // Wait for interrupt
#endif
    }
}
