/**
 * @file tss.cpp
 * @brief Task State Segment (TSS) implementation for x86_64
 *
 * Manages kernel stack switching for Ring 3 → Ring 0 transitions.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "tss.hpp"
#include "gdt.hpp"
#include "../../early/serial_16550.hpp"
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel {

// ============================================================================
// TSS Structure
// ============================================================================

/**
 * @brief Task State Segment structure (104 bytes in 64-bit mode)
 *
 * In 64-bit mode, only a few fields are used:
 * - RSP0, RSP1, RSP2: Stack pointers for Ring 0, 1, 2
 * - IST1-IST7: Interrupt Stack Table pointers
 *
 * Most important: RSP0 (kernel stack for Ring 3 → Ring 0 transitions)
 */
struct [[gnu::packed]] TaskStateSegment {
    uint32_t reserved1;     ///< Reserved (must be 0)
    uint64_t rsp0;          ///< Ring 0 stack pointer (CRITICAL!)
    uint64_t rsp1;          ///< Ring 1 stack pointer (unused)
    uint64_t rsp2;          ///< Ring 2 stack pointer (unused)
    uint64_t reserved2;     ///< Reserved (must be 0)
    uint64_t ist[7];        ///< Interrupt Stack Table pointers
    uint64_t reserved3;     ///< Reserved (must be 0)
    uint16_t reserved4;     ///< Reserved (must be 0)
    uint16_t iomap_base;    ///< I/O Map Base Address
};

static_assert(sizeof(TaskStateSegment) == 104, "TSS must be 104 bytes");

// ============================================================================
// TSS Global State
// ============================================================================

/**
 * @brief Global Task State Segment
 *
 * There is only one TSS in the system. The kernel stack pointer (rsp0)
 * is updated on every context switch.
 */
static TaskStateSegment g_tss;

// ============================================================================
// TSS Descriptor Setup
// ============================================================================

/**
 * @brief Add TSS descriptor to GDT
 *
 * In 64-bit mode, TSS descriptors are 16 bytes (span 2 GDT entries).
 *
 * @param base Address of TSS
 * @param limit Size of TSS - 1
 */
static void add_tss_to_gdt(uint64_t base, uint32_t limit) {
    // Access external GDT (defined in gdt.cpp)
    extern uint8_t g_gdt[];  // Access GDT as byte array for TSS descriptor

    // TSS descriptor goes in entry 5 (index 5)
    uint8_t* tss_desc = &g_gdt[5 * 8];  // Each entry is 8 bytes

    // Low 64 bits (standard descriptor format)
    tss_desc[0] = limit & 0xFF;
    tss_desc[1] = (limit >> 8) & 0xFF;
    tss_desc[2] = base & 0xFF;
    tss_desc[3] = (base >> 8) & 0xFF;
    tss_desc[4] = (base >> 16) & 0xFF;
    tss_desc[5] = 0x89;  // Present, DPL=0, Type=TSS Available
    tss_desc[6] = 0x00;  // Granularity
    tss_desc[7] = (base >> 24) & 0xFF;

    // High 64 bits (upper 32 bits of base + reserved)
    tss_desc[8] = (base >> 32) & 0xFF;
    tss_desc[9] = (base >> 40) & 0xFF;
    tss_desc[10] = (base >> 48) & 0xFF;
    tss_desc[11] = (base >> 56) & 0xFF;
    tss_desc[12] = 0;  // Reserved
    tss_desc[13] = 0;  // Reserved
    tss_desc[14] = 0;  // Reserved
    tss_desc[15] = 0;  // Reserved
}

/**
 * @brief Load TSS using LTR instruction
 *
 * This is implemented in assembly in tss_load.S
 */
extern "C" void tss_load(uint16_t tss_selector);

// ============================================================================
// Public Functions
// ============================================================================

/**
 * @brief Initialize Task State Segment
 *
 * Sets up TSS and loads it into the CPU.
 */
void initialize_tss() {
    early_serial.write("[TSS] Initializing Task State Segment...\n");

    // Zero out TSS
    memset(&g_tss, 0, sizeof(g_tss));

    // Set I/O map base to size of TSS (no I/O permissions)
    g_tss.iomap_base = sizeof(TaskStateSegment);

    // RSP0 will be set by set_kernel_stack() before first Ring 3 process
    // IST entries can be set later if we want separate interrupt stacks

    // Add TSS descriptor to GDT
    uint64_t tss_base = reinterpret_cast<uint64_t>(&g_tss);
    uint32_t tss_limit = sizeof(TaskStateSegment) - 1;
    add_tss_to_gdt(tss_base, tss_limit);

    // Load TSS (selector 0x28 = GDT entry 5, RPL=0)
    tss_load(TSS_SEL);

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[TSS] Loaded at 0x%lx, selector 0x%x\n",
             tss_base, TSS_SEL);
    early_serial.write(buffer);
    early_serial.write("[TSS] Initialization complete\n");
}

/**
 * @brief Set kernel stack pointer in TSS
 *
 * This MUST be called before switching to a Ring 3 process.
 *
 * @param kernel_rsp Top of kernel stack for current process
 */
void set_kernel_stack(uint64_t kernel_rsp) {
    g_tss.rsp0 = kernel_rsp;
}

/**
 * @brief Get current kernel stack pointer from TSS
 *
 * @return Current TSS.rsp0 value
 */
uint64_t get_kernel_stack() {
    return g_tss.rsp0;
}

} // namespace xinim::kernel
