// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// IRQ (Interrupt Request) Management Subsystem
// Provides interrupt allocation, handler registration, and dispatch

#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::kernel {

// Maximum number of IRQ lines (x86_64 has 256 vectors)
constexpr size_t MAX_IRQS = 256;

// IRQ vector ranges
constexpr uint8_t IRQ_VECTOR_BASE = 32;        // First 32 vectors reserved for exceptions
constexpr uint8_t IRQ_VECTOR_TIMER = 32;       // PIT timer
constexpr uint8_t IRQ_VECTOR_KEYBOARD = 33;    // PS/2 keyboard
constexpr uint8_t IRQ_VECTOR_CASCADE = 34;     // PIC cascade
constexpr uint8_t IRQ_VECTOR_COM2 = 35;        // COM2 serial
constexpr uint8_t IRQ_VECTOR_COM1 = 36;        // COM1 serial
constexpr uint8_t IRQ_VECTOR_LPT2 = 37;        // LPT2 parallel
constexpr uint8_t IRQ_VECTOR_FLOPPY = 38;      // Floppy disk
constexpr uint8_t IRQ_VECTOR_LPT1 = 39;        // LPT1 parallel
constexpr uint8_t IRQ_VECTOR_RTC = 40;         // Real-time clock
constexpr uint8_t IRQ_VECTOR_ACPI = 41;        // ACPI
constexpr uint8_t IRQ_VECTOR_AVAILABLE1 = 42;  // Available
constexpr uint8_t IRQ_VECTOR_AVAILABLE2 = 43;  // Available
constexpr uint8_t IRQ_VECTOR_MOUSE = 44;       // PS/2 mouse
constexpr uint8_t IRQ_VECTOR_FPU = 45;         // FPU exception
constexpr uint8_t IRQ_VECTOR_PRIMARY_ATA = 46; // Primary ATA
constexpr uint8_t IRQ_VECTOR_SECONDARY_ATA = 47; // Secondary ATA

// Dynamic IRQ allocation range (48-255)
constexpr uint8_t IRQ_VECTOR_DYNAMIC_START = 48;
constexpr uint8_t IRQ_VECTOR_DYNAMIC_END = 255;

// IRQ flags
enum class IRQFlags : uint32_t {
    NONE = 0,
    SHARED = (1 << 0),        // IRQ can be shared between devices
    LEVEL_TRIGGERED = (1 << 1), // Level-triggered (vs edge-triggered)
    ACTIVE_LOW = (1 << 2),     // Active low (vs active high)
    MSI = (1 << 3),            // Message Signaled Interrupt
    MSIX = (1 << 4),           // MSI-X (extended)
};

// IRQ handler callback function
// Returns true if interrupt was handled, false otherwise
using IRQHandler = bool (*)(uint8_t vector, void* context);

// IRQ descriptor (internal representation)
struct IRQDescriptor {
    IRQHandler handler;        // Handler function
    void* context;             // Context pointer for handler
    IRQFlags flags;            // IRQ configuration flags
    uint32_t count;            // Interrupt count
    bool allocated;            // True if IRQ is allocated
    bool enabled;              // True if IRQ is enabled
    const char* device_name;   // Name of device using this IRQ
};

// MSI (Message Signaled Interrupt) configuration
struct MSIConfig {
    uint64_t address;          // MSI address
    uint32_t data;             // MSI data value
    uint8_t vector;            // Assigned vector
    bool is_64bit;             // 64-bit addressing support
};

// MSI-X table entry
struct MSIXEntry {
    uint64_t msg_addr;         // Message address
    uint32_t msg_data;         // Message data
    uint32_t vector_control;   // Vector control (bit 0: mask)
};

// IRQ Management Interface
class IRQ {
public:
    // Initialize the IRQ subsystem
    static bool initialize();

    // Shutdown the IRQ subsystem
    static void shutdown();

    // ===== IRQ Allocation =====

    // Allocate a specific IRQ vector
    // Returns true if successful, false if already allocated
    static bool allocate_irq(uint8_t vector, const char* device_name = nullptr);

    // Allocate any available IRQ in the dynamic range
    // Returns the allocated vector, or 0 if none available
    static uint8_t allocate_irq_dynamic(const char* device_name = nullptr);

    // Free an allocated IRQ
    static void free_irq(uint8_t vector);

    // Check if an IRQ is allocated
    static bool is_allocated(uint8_t vector);

    // ===== Handler Registration =====

    // Register an interrupt handler for an IRQ
    // Multiple handlers can be registered for shared IRQs
    static bool register_handler(uint8_t vector, IRQHandler handler,
                                 void* context = nullptr,
                                 IRQFlags flags = IRQFlags::NONE);

    // Unregister an interrupt handler
    static bool unregister_handler(uint8_t vector, IRQHandler handler);

    // ===== IRQ Control =====

    // Enable an IRQ line
    static void enable_irq(uint8_t vector);

    // Disable an IRQ line
    static void disable_irq(uint8_t vector);

    // Check if an IRQ is enabled
    static bool is_enabled(uint8_t vector);

    // Mask/unmask IRQ (legacy PIC)
    static void mask_irq(uint8_t irq_line);
    static void unmask_irq(uint8_t irq_line);

    // Send End-Of-Interrupt signal
    static void send_eoi(uint8_t vector);

    // ===== MSI/MSI-X Support =====

    // Allocate MSI interrupt
    // Returns vector number, or 0 on failure
    static uint8_t allocate_msi(MSIConfig& config_out, const char* device_name = nullptr);

    // Free MSI interrupt
    static void free_msi(uint8_t vector);

    // Configure MSI-X entry
    static bool configure_msix(uint8_t vector, MSIXEntry& entry);

    // ===== Interrupt Dispatch =====

    // Dispatch an interrupt (called from assembly stub)
    static void dispatch_interrupt(uint8_t vector);

    // ===== Query Functions =====

    // Get interrupt count for a vector
    static uint32_t get_interrupt_count(uint8_t vector);

    // Get device name for a vector
    static const char* get_device_name(uint8_t vector);

    // Get IRQ descriptor (for debugging)
    static const IRQDescriptor* get_descriptor(uint8_t vector);

    // ===== Debugging =====

    // Dump all IRQ allocations
    static void dump_irqs();

private:
    IRQ() = delete;
    ~IRQ() = delete;
    IRQ(const IRQ&) = delete;
    IRQ& operator=(const IRQ&) = delete;
};

// Bitwise operators for IRQFlags
inline IRQFlags operator|(IRQFlags a, IRQFlags b) {
    return static_cast<IRQFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline IRQFlags operator&(IRQFlags a, IRQFlags b) {
    return static_cast<IRQFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline IRQFlags operator^(IRQFlags a, IRQFlags b) {
    return static_cast<IRQFlags>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

inline IRQFlags operator~(IRQFlags a) {
    return static_cast<IRQFlags>(~static_cast<uint32_t>(a));
}

inline IRQFlags& operator|=(IRQFlags& a, IRQFlags b) {
    return a = a | b;
}

inline IRQFlags& operator&=(IRQFlags& a, IRQFlags b) {
    return a = a & b;
}

inline bool has_flag(IRQFlags flags, IRQFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

} // namespace xinim::kernel
