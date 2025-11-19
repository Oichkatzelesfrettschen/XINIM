// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// IRQ Management Subsystem Implementation

#include <xinim/kernel/irq.hpp>
#include <cstring>

namespace xinim::kernel {

// PIC (Programmable Interrupt Controller) I/O ports
constexpr uint16_t PIC1_CMD = 0x20;
constexpr uint16_t PIC1_DATA = 0x21;
constexpr uint16_t PIC2_CMD = 0xA0;
constexpr uint16_t PIC2_DATA = 0xA1;
constexpr uint8_t PIC_EOI = 0x20;

// Global IRQ descriptor table
static IRQDescriptor g_irq_descriptors[MAX_IRQS];
static bool g_irq_initialized = false;

// PIC masking state (for legacy IRQs 0-15)
static uint16_t g_pic_mask = 0xFFFF;  // All masked initially

// Port I/O functions (x86_64)
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Initialize PIC (for legacy interrupts)
static void init_pic() {
    // ICW1: Initialize command
    outb(PIC1_CMD, 0x11);  // Init + ICW4 needed
    outb(PIC2_CMD, 0x11);

    // ICW2: Vector offsets
    outb(PIC1_DATA, IRQ_VECTOR_BASE);      // Master PIC: vectors 32-39
    outb(PIC2_DATA, IRQ_VECTOR_BASE + 8);  // Slave PIC: vectors 40-47

    // ICW3: Cascade setup
    outb(PIC1_DATA, 0x04);  // Slave on IRQ2
    outb(PIC2_DATA, 0x02);  // Cascade identity

    // ICW4: Mode
    outb(PIC1_DATA, 0x01);  // 8086 mode
    outb(PIC2_DATA, 0x01);

    // Mask all interrupts initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// Disable PIC (when using APIC)
static void disable_pic() {
    // Mask all interrupts on both PICs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

bool IRQ::initialize() {
    if (g_irq_initialized) {
        return true;
    }

    // Clear all descriptors
    memset(g_irq_descriptors, 0, sizeof(g_irq_descriptors));

    // Initialize PIC for legacy interrupts
    init_pic();

    // TODO: Detect and initialize APIC/IOAPIC if available
    // For now, we use legacy PIC mode

    g_irq_initialized = true;
    return true;
}

void IRQ::shutdown() {
    if (!g_irq_initialized) {
        return;
    }

    // Mask all IRQs
    for (size_t i = 0; i < MAX_IRQS; ++i) {
        if (g_irq_descriptors[i].enabled) {
            disable_irq(i);
        }
    }

    // Disable PIC
    disable_pic();

    g_irq_initialized = false;
}

bool IRQ::allocate_irq(uint8_t vector, const char* device_name) {
    if (vector >= MAX_IRQS) {
        return false;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];
    if (desc.allocated) {
        return false;  // Already allocated
    }

    desc.allocated = true;
    desc.device_name = device_name;
    desc.handler = nullptr;
    desc.context = nullptr;
    desc.flags = IRQFlags::NONE;
    desc.count = 0;
    desc.enabled = false;

    return true;
}

uint8_t IRQ::allocate_irq_dynamic(const char* device_name) {
    // Search for free IRQ in dynamic range
    for (uint8_t vector = IRQ_VECTOR_DYNAMIC_START; vector <= IRQ_VECTOR_DYNAMIC_END; ++vector) {
        if (!g_irq_descriptors[vector].allocated) {
            if (allocate_irq(vector, device_name)) {
                return vector;
            }
        }
    }
    return 0;  // No free IRQ found
}

void IRQ::free_irq(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];
    if (!desc.allocated) {
        return;
    }

    // Disable IRQ if enabled
    if (desc.enabled) {
        disable_irq(vector);
    }

    // Clear descriptor
    desc.allocated = false;
    desc.handler = nullptr;
    desc.context = nullptr;
    desc.device_name = nullptr;
    desc.flags = IRQFlags::NONE;
    desc.count = 0;
    desc.enabled = false;
}

bool IRQ::is_allocated(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return false;
    }
    return g_irq_descriptors[vector].allocated;
}

bool IRQ::register_handler(uint8_t vector, IRQHandler handler,
                           void* context, IRQFlags flags) {
    if (vector >= MAX_IRQS || !handler) {
        return false;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];

    // Allocate if not already allocated
    if (!desc.allocated) {
        desc.allocated = true;
    }

    // Check for shared IRQ support
    if (desc.handler != nullptr) {
        // Handler already registered
        if (!has_flag(desc.flags, IRQFlags::SHARED) || !has_flag(flags, IRQFlags::SHARED)) {
            return false;  // Cannot share this IRQ
        }
        // TODO: Support multiple handlers via linked list
        return false;  // For now, only one handler per IRQ
    }

    desc.handler = handler;
    desc.context = context;
    desc.flags = flags;

    return true;
}

bool IRQ::unregister_handler(uint8_t vector, IRQHandler handler) {
    if (vector >= MAX_IRQS || !handler) {
        return false;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];
    if (desc.handler != handler) {
        return false;
    }

    desc.handler = nullptr;
    desc.context = nullptr;

    return true;
}

void IRQ::enable_irq(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];
    desc.enabled = true;

    // If legacy PIC IRQ (0-15), unmask it
    if (vector >= IRQ_VECTOR_BASE && vector < IRQ_VECTOR_BASE + 16) {
        uint8_t irq_line = vector - IRQ_VECTOR_BASE;
        unmask_irq(irq_line);
    }

    // TODO: For APIC/IOAPIC, configure IOAPIC redirection entry
}

void IRQ::disable_irq(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];
    desc.enabled = false;

    // If legacy PIC IRQ (0-15), mask it
    if (vector >= IRQ_VECTOR_BASE && vector < IRQ_VECTOR_BASE + 16) {
        uint8_t irq_line = vector - IRQ_VECTOR_BASE;
        mask_irq(irq_line);
    }

    // TODO: For APIC/IOAPIC, mask IOAPIC redirection entry
}

bool IRQ::is_enabled(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return false;
    }
    return g_irq_descriptors[vector].enabled;
}

void IRQ::mask_irq(uint8_t irq_line) {
    if (irq_line >= 16) {
        return;
    }

    g_pic_mask |= (1 << irq_line);

    if (irq_line < 8) {
        outb(PIC1_DATA, g_pic_mask & 0xFF);
    } else {
        outb(PIC2_DATA, (g_pic_mask >> 8) & 0xFF);
    }
}

void IRQ::unmask_irq(uint8_t irq_line) {
    if (irq_line >= 16) {
        return;
    }

    g_pic_mask &= ~(1 << irq_line);

    if (irq_line < 8) {
        outb(PIC1_DATA, g_pic_mask & 0xFF);
    } else {
        outb(PIC2_DATA, (g_pic_mask >> 8) & 0xFF);
    }
}

void IRQ::send_eoi(uint8_t vector) {
    if (vector < IRQ_VECTOR_BASE) {
        return;  // Not a hardware interrupt
    }

    // For legacy PIC
    if (vector >= IRQ_VECTOR_BASE && vector < IRQ_VECTOR_BASE + 16) {
        uint8_t irq_line = vector - IRQ_VECTOR_BASE;
        if (irq_line >= 8) {
            outb(PIC2_CMD, PIC_EOI);  // Send EOI to slave PIC
        }
        outb(PIC1_CMD, PIC_EOI);      // Send EOI to master PIC
    }

    // TODO: For APIC, write to Local APIC EOI register
}

uint8_t IRQ::allocate_msi(MSIConfig& config_out, const char* device_name) {
    // Allocate a dynamic IRQ vector
    uint8_t vector = allocate_irq_dynamic(device_name);
    if (vector == 0) {
        return 0;
    }

    // Configure MSI
    // MSI address format (x86_64):
    // Bits 31-20: 0xFEE (fixed)
    // Bits 19-12: Destination ID (APIC ID)
    // Bit 3: Redirection hint (0 = physical, 1 = logical)
    // Bit 2: Destination mode (0 = physical, 1 = logical)
    config_out.address = 0xFEE00000;  // Base MSI address
    config_out.data = vector;         // Vector number
    config_out.vector = vector;
    config_out.is_64bit = true;

    // Mark as MSI
    g_irq_descriptors[vector].flags = IRQFlags::MSI;

    return vector;
}

void IRQ::free_msi(uint8_t vector) {
    free_irq(vector);
}

bool IRQ::configure_msix(uint8_t vector, MSIXEntry& entry) {
    if (vector >= MAX_IRQS) {
        return false;
    }

    // Configure MSI-X entry
    entry.msg_addr = 0xFEE00000;      // Base MSI address
    entry.msg_data = vector;          // Vector number
    entry.vector_control = 0;         // Unmasked

    // Mark as MSI-X
    g_irq_descriptors[vector].flags = IRQFlags::MSIX;

    return true;
}

void IRQ::dispatch_interrupt(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return;
    }

    IRQDescriptor& desc = g_irq_descriptors[vector];

    // Increment interrupt count
    desc.count++;

    // Call handler if registered
    if (desc.handler) {
        bool handled = desc.handler(vector, desc.context);
        (void)handled;  // TODO: Track unhandled interrupts
    }

    // Send EOI
    send_eoi(vector);
}

uint32_t IRQ::get_interrupt_count(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return 0;
    }
    return g_irq_descriptors[vector].count;
}

const char* IRQ::get_device_name(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return nullptr;
    }
    return g_irq_descriptors[vector].device_name;
}

const IRQDescriptor* IRQ::get_descriptor(uint8_t vector) {
    if (vector >= MAX_IRQS) {
        return nullptr;
    }
    return &g_irq_descriptors[vector];
}

void IRQ::dump_irqs() {
    // TODO: Implement IRQ dump for debugging
    // This would print all allocated IRQs with their handlers and counts
}

} // namespace xinim::kernel
