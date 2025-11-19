/**
 * @file syscall_init.cpp
 * @brief Syscall/sysret initialization implementation
 *
 * Configures x86_64 MSRs to enable fast system calls.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "syscall_init.hpp"
#include "gdt.hpp"  // For segment selectors
#include "../../early/serial_16550.hpp"
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

// Syscall handler from assembly
extern "C" void syscall_handler();

namespace xinim::kernel {

// ============================================================================
// MSR Definitions
// ============================================================================

/**
 * @brief Model Specific Register numbers
 */
constexpr uint32_t MSR_EFER   = 0xC0000080;  ///< Extended Feature Enable Register
constexpr uint32_t MSR_STAR   = 0xC0000081;  ///< Syscall Target Address Register
constexpr uint32_t MSR_LSTAR  = 0xC0000082;  ///< Long Mode Syscall Target Address
constexpr uint32_t MSR_CSTAR  = 0xC0000083;  ///< Compatibility Mode Syscall Target
constexpr uint32_t MSR_FMASK  = 0xC0000084;  ///< Syscall Flag Mask

/**
 * @brief EFER register bits
 */
constexpr uint64_t EFER_SCE   = (1ULL << 0);  ///< System Call Extensions enable
constexpr uint64_t EFER_LME   = (1ULL << 8);  ///< Long Mode Enable
constexpr uint64_t EFER_LMA   = (1ULL << 10); ///< Long Mode Active

/**
 * @brief RFLAGS bits to mask (disable during syscall)
 */
constexpr uint64_t RFLAGS_IF  = (1ULL << 9);  ///< Interrupt Flag

// ============================================================================
// MSR Access Functions
// ============================================================================

/**
 * @brief Read Model Specific Register
 *
 * @param msr MSR number
 * @return uint64_t MSR value
 */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * @brief Write Model Specific Register
 *
 * @param msr MSR number
 * @param value Value to write
 */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

// ============================================================================
// Syscall Initialization
// ============================================================================

/**
 * @brief Initialize syscall/sysret mechanism
 *
 * Sets up MSRs to enable fast system calls on x86_64.
 */
void initialize_syscall() {
    early_serial.write("[SYSCALL] Initializing fast syscall mechanism...\n");

    // Step 1: Enable SCE (System Call Extensions) in IA32_EFER
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;  // Enable syscall/sysret
    wrmsr(MSR_EFER, efer);

    early_serial.write("[SYSCALL] Enabled SCE in IA32_EFER\n");

    // Step 2: Set up IA32_STAR (Syscall Target Address Register)
    // Bits 63:48 = User CS base (user code selector - 16)
    // Bits 47:32 = Kernel CS base
    // Bits 31:0  = Reserved (must be 0)
    //
    // User CS:   0x1B (GDT entry 3, RPL=3)
    // User CS base: 0x1B - 16 = 0x0B (GDT entry 1, which becomes 3 with RPL=3)
    // Kernel CS: 0x08 (GDT entry 1, RPL=0)
    //
    // Actually, the CPU adds 16 to the base, so:
    // User CS base = (desired user CS - 16) = 0x1B - 16 = 0x0B
    // But we want it to load 0x1B, so we set base to 0x0B

    uint64_t star = 0;
    star |= ((uint64_t)(USER_CS - 16) << 48);  // User CS base
    star |= ((uint64_t)KERNEL_CS << 32);        // Kernel CS base

    wrmsr(MSR_STAR, star);

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] Set IA32_STAR: kernel_cs=0x%x, user_cs_base=0x%x\n",
             KERNEL_CS, (uint32_t)(USER_CS - 16));
    early_serial.write(buffer);

    // Step 3: Set up IA32_LSTAR (Long Mode Syscall Target Address)
    // This is the address of our syscall handler
    uint64_t handler_addr = reinterpret_cast<uint64_t>(&syscall_handler);
    wrmsr(MSR_LSTAR, handler_addr);

    snprintf(buffer, sizeof(buffer),
             "[SYSCALL] Set IA32_LSTAR: handler=0x%lx\n", handler_addr);
    early_serial.write(buffer);

    // Step 4: Set up IA32_FMASK (Syscall Flag Mask)
    // Bits set in FMASK will be CLEARED in RFLAGS on syscall
    // We want to disable interrupts during syscall handling
    wrmsr(MSR_FMASK, RFLAGS_IF);

    early_serial.write("[SYSCALL] Set IA32_FMASK: will clear IF (disable interrupts)\n");

    // Step 5: Verify setup
    uint64_t efer_verify = rdmsr(MSR_EFER);
    if (efer_verify & EFER_SCE) {
        early_serial.write("[SYSCALL] Verification: SCE enabled ✓\n");
    } else {
        early_serial.write("[SYSCALL] ERROR: SCE not enabled!\n");
    }

    early_serial.write("[SYSCALL] Initialization complete\n");
}

} // namespace xinim::kernel
