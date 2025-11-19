/**
 * @file gdt.cpp
 * @brief Global Descriptor Table (GDT) implementation for x86_64
 *
 * Sets up segment descriptors for privilege level separation.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "gdt.hpp"
#include "../../early/serial_16550.hpp"
#include <cstring>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// GDT Structures
// ============================================================================

/**
 * @brief GDT entry structure (8 bytes)
 *
 * In 64-bit mode, code/data segments are largely ignored (flat memory model),
 * but privilege levels (DPL) are still enforced.
 */
struct [[gnu::packed]] GdtEntry {
    uint16_t limit_low;     ///< Limit bits 0-15 (ignored in 64-bit)
    uint16_t base_low;      ///< Base bits 0-15 (ignored in 64-bit)
    uint8_t base_mid;       ///< Base bits 16-23 (ignored in 64-bit)
    uint8_t access;         ///< Access byte (DPL, type, present)
    uint8_t granularity;    ///< Limit bits 16-19 + flags
    uint8_t base_high;      ///< Base bits 24-31 (ignored in 64-bit)
};

static_assert(sizeof(GdtEntry) == 8, "GDT entry must be 8 bytes");

/**
 * @brief GDT pointer structure for LGDT instruction (10 bytes)
 */
struct [[gnu::packed]] GdtPointer {
    uint16_t limit;         ///< Size of GDT - 1
    uint64_t base;          ///< Address of GDT
};

static_assert(sizeof(GdtPointer) == 10, "GDT pointer must be 10 bytes");

// ============================================================================
// GDT Global State
// ============================================================================

/**
 * @brief Global Descriptor Table (6 entries)
 *
 * Entry 0: Null descriptor (required by x86_64)
 * Entry 1: Kernel code segment (Ring 0, executable)
 * Entry 2: Kernel data segment (Ring 0, writable)
 * Entry 3: User code segment (Ring 3, executable)
 * Entry 4: User data segment (Ring 3, writable)
 * Entry 5-6: TSS descriptor (added by initialize_tss(), 16 bytes in 64-bit mode)
 */
static GdtEntry g_gdt[7];  // 7 to accommodate TSS (which spans 2 entries)

/**
 * @brief GDT pointer for LGDT instruction
 */
static GdtPointer g_gdt_ptr;

// ============================================================================
// Access Byte Flags
// ============================================================================

// Descriptor types
constexpr uint8_t GDT_TYPE_CODE = 0x0A;  // Execute/Read
constexpr uint8_t GDT_TYPE_DATA = 0x02;  // Read/Write

// Access byte bits
constexpr uint8_t GDT_PRESENT = 0x80;    // Present bit (must be 1)
constexpr uint8_t GDT_DPL_0 = 0x00;      // Descriptor Privilege Level 0 (Ring 0)
constexpr uint8_t GDT_DPL_3 = 0x60;      // Descriptor Privilege Level 3 (Ring 3)
constexpr uint8_t GDT_CODE_DATA = 0x10;  // Code/Data segment (not system)

// Granularity byte flags
constexpr uint8_t GDT_LONG_MODE = 0x20;  // Long mode (64-bit)
constexpr uint8_t GDT_GRANULAR = 0x80;   // Granularity (ignored in 64-bit)

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Set a GDT entry
 *
 * @param index GDT entry index (0-6)
 * @param base Base address (ignored in 64-bit for code/data)
 * @param limit Limit (ignored in 64-bit for code/data)
 * @param access Access byte (DPL, type, present)
 * @param granularity Granularity byte (flags)
 */
static void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    GdtEntry& entry = g_gdt[index];

    entry.base_low = base & 0xFFFF;
    entry.base_mid = (base >> 16) & 0xFF;
    entry.base_high = (base >> 24) & 0xFF;

    entry.limit_low = limit & 0xFFFF;
    entry.granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);

    entry.access = access;
}

/**
 * @brief Load GDT using LGDT instruction
 *
 * This is implemented in assembly in gdt_load.S
 */
extern "C" void gdt_load(uint64_t gdt_ptr_address);

// ============================================================================
// Public Functions
// ============================================================================

/**
 * @brief Initialize Global Descriptor Table
 *
 * Sets up a minimal GDT with kernel and user segments.
 */
void initialize_gdt() {
    early_serial.write("[GDT] Initializing Global Descriptor Table...\n");

    // Zero out GDT
    memset(g_gdt, 0, sizeof(g_gdt));

    // Entry 0: Null descriptor (required)
    // Already zeroed, no action needed

    // Entry 1: Kernel code segment (Ring 0, 64-bit, executable)
    set_gdt_entry(1, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_CODE_DATA | GDT_DPL_0 | GDT_TYPE_CODE,
                  GDT_LONG_MODE | GDT_GRANULAR);

    // Entry 2: Kernel data segment (Ring 0, writable)
    set_gdt_entry(2, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_CODE_DATA | GDT_DPL_0 | GDT_TYPE_DATA,
                  GDT_GRANULAR);

    // Entry 3: User code segment (Ring 3, 64-bit, executable)
    set_gdt_entry(3, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_CODE_DATA | GDT_DPL_3 | GDT_TYPE_CODE,
                  GDT_LONG_MODE | GDT_GRANULAR);

    // Entry 4: User data segment (Ring 3, writable)
    set_gdt_entry(4, 0, 0xFFFFF,
                  GDT_PRESENT | GDT_CODE_DATA | GDT_DPL_3 | GDT_TYPE_DATA,
                  GDT_GRANULAR);

    // Entry 5-6: TSS will be added by initialize_tss()

    // Set up GDT pointer
    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base = reinterpret_cast<uint64_t>(g_gdt);

    // Load GDT
    gdt_load(reinterpret_cast<uint64_t>(&g_gdt_ptr));

    early_serial.write("[GDT] Loaded with 5 entries:\n");
    early_serial.write("  [0] Null descriptor\n");
    early_serial.write("  [1] Kernel code (0x08, Ring 0)\n");
    early_serial.write("  [2] Kernel data (0x10, Ring 0)\n");
    early_serial.write("  [3] User code (0x1B, Ring 3)\n");
    early_serial.write("  [4] User data (0x23, Ring 3)\n");
    early_serial.write("[GDT] Initialization complete\n");
}

} // namespace xinim::kernel
