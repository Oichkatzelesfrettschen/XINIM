// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// PCI Subsystem Implementation

#include <xinim/pci/pci.hpp>
#include <cstring>

namespace xinim::pci {

// PCI Configuration Mechanism 1 I/O Ports
constexpr uint16_t CONFIG_ADDRESS = 0xCF8;
constexpr uint16_t CONFIG_DATA = 0xCFC;

// Global device list (fixed-size, no STL -- freestanding safe)
static constexpr size_t MAX_PCI_DEVICES = 64;
static PCIDevice g_devices[MAX_PCI_DEVICES];
static size_t g_device_count = 0;
static bool g_initialized = false;

// Port I/O from centralized header
#include "../kernel/arch/x86_64/portio.hpp"
using xinim::arch::x86_64::outl;
using xinim::arch::x86_64::inl;
using xinim::arch::x86_64::outw;
using xinim::arch::x86_64::inw;
using xinim::arch::x86_64::outb;
using xinim::arch::x86_64::inb;

// Create PCI config address
static uint32_t pci_address(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    return 0x80000000 |
           (static_cast<uint32_t>(bus) << 16) |
           (static_cast<uint32_t>(device) << 11) |
           (static_cast<uint32_t>(function) << 8) |
           (offset & 0xFC);
}

bool PCI::initialize() {
    if (g_initialized) {
        return true;
    }

    g_device_count = 0;
    g_initialized = true;

    // Enumerate all devices
    enumerate_devices();

    return true;
}

void PCI::shutdown() {
    g_device_count = 0;
    g_initialized = false;
}

uint8_t PCI::read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);

    // Read byte at offset within the 32-bit value
    return inb(CONFIG_DATA + (offset & 3));
}

uint16_t PCI::read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);

    // Read word at offset within the 32-bit value
    return inw(CONFIG_DATA + (offset & 2));
}

uint32_t PCI::read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);
    return inl(CONFIG_DATA);
}

void PCI::write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);
    outb(CONFIG_DATA + (offset & 3), value);
}

void PCI::write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);
    outw(CONFIG_DATA + (offset & 2), value);
}

void PCI::write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) {
    uint32_t address = pci_address(bus, device, function, offset);
    outl(CONFIG_ADDRESS, address);
    outl(CONFIG_DATA, value);
}

static void probe_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = PCI::read_config_word(bus, device, function, config::VENDOR_ID);

    // Check if device exists
    if (vendor_id == 0xFFFF) {
        return;
    }

    PCIDevice dev;
    dev.bus = bus;
    dev.device = device;
    dev.function = function;

    // Read identification
    dev.vendor_id = vendor_id;
    dev.device_id = PCI::read_config_word(bus, device, function, config::DEVICE_ID);
    dev.class_code = PCI::read_config_byte(bus, device, function, config::CLASS_CODE);
    dev.subclass = PCI::read_config_byte(bus, device, function, config::SUBCLASS);
    dev.prog_if = PCI::read_config_byte(bus, device, function, config::PROG_IF);
    dev.revision_id = PCI::read_config_byte(bus, device, function, config::REVISION_ID);
    dev.header_type = PCI::read_config_byte(bus, device, function, config::HEADER_TYPE);

    // Read interrupt info
    dev.interrupt_line = PCI::read_config_byte(bus, device, function, config::INTERRUPT_LINE);
    dev.interrupt_pin = PCI::read_config_byte(bus, device, function, config::INTERRUPT_PIN);

    // Read BARs
    for (int i = 0; i < 6; ++i) {
        PCI::read_bar(dev, i, dev.bars[i]);
    }

    if (g_device_count < MAX_PCI_DEVICES) {
        g_devices[g_device_count++] = dev;
    }
}

static void probe_device(uint8_t bus, uint8_t device) {
    uint16_t vendor_id = PCI::read_config_word(bus, device, 0, config::VENDOR_ID);

    if (vendor_id == 0xFFFF) {
        return;  // Device doesn't exist
    }

    // Probe function 0
    probe_function(bus, device, 0);

    // Check if multifunction device
    uint8_t header_type = PCI::read_config_byte(bus, device, 0, config::HEADER_TYPE);
    if (header_type & header::MULTIFUNCTION) {
        // Probe functions 1-7
        for (uint8_t function = 1; function < 8; ++function) {
            vendor_id = PCI::read_config_word(bus, device, function, config::VENDOR_ID);
            if (vendor_id != 0xFFFF) {
                probe_function(bus, device, function);
            }
        }
    }
}

bool PCI::enumerate_devices() {
    g_device_count = 0;

    // Scan all buses, devices, and functions
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            probe_device(static_cast<uint8_t>(bus), device);
        }
    }

    return g_device_count > 0;
}

size_t PCI::get_device_count() {
    return g_device_count;
}

const PCIDevice* PCI::get_device(size_t index) {
    if (index < g_device_count) {
        return &g_devices[index];
    }
    return nullptr;
}

const PCIDevice* PCI::find_device(uint16_t vendor_id, uint16_t device_id) {
    for (size_t i = 0; i < g_device_count; ++i) {
        if (g_devices[i].vendor_id == vendor_id && g_devices[i].device_id == device_id) {
            return &g_devices[i];
        }
    }
    return nullptr;
}

const PCIDevice* PCI::find_device_by_class(uint8_t class_code, uint8_t subclass) {
    for (size_t i = 0; i < g_device_count; ++i) {
        if (g_devices[i].class_code == class_code && g_devices[i].subclass == subclass) {
            return &g_devices[i];
        }
    }
    return nullptr;
}

bool PCI::read_bar(const PCIDevice& device, uint8_t bar_index, BAR& bar_out) {
    if (bar_index >= 6) {
        return false;
    }

    uint16_t bar_offset = config::BAR0 + (bar_index * 4);

    // Read BAR value
    uint32_t bar_value = read_config_dword(device.bus, device.device, device.function, bar_offset);

    if (bar_value == 0) {
        bar_out = BAR{};  // Invalid/unused BAR
        return false;
    }

    // Determine if Memory or I/O
    bar_out.is_mmio = !(bar_value & 0x1);

    if (bar_out.is_mmio) {
        // Memory BAR
        bar_out.is_prefetchable = (bar_value & 0x8) != 0;
        bar_out.is_64bit = ((bar_value & 0x6) >> 1) == 2;

        // Read size by writing all 1s and reading back
        write_config_dword(device.bus, device.device, device.function, bar_offset, 0xFFFFFFFF);
        uint32_t size_mask = read_config_dword(device.bus, device.device, device.function, bar_offset);
        write_config_dword(device.bus, device.device, device.function, bar_offset, bar_value);  // Restore

        // Calculate size
        size_mask &= 0xFFFFFFF0;  // Clear flags
        bar_out.size = ~size_mask + 1;

        // Get address
        if (bar_out.is_64bit) {
            uint64_t addr_low = bar_value & 0xFFFFFFF0;
            uint32_t addr_high = read_config_dword(device.bus, device.device, device.function, bar_offset + 4);
            bar_out.address = (static_cast<uint64_t>(addr_high) << 32) | addr_low;
        } else {
            bar_out.address = bar_value & 0xFFFFFFF0;
        }
    } else {
        // I/O BAR
        bar_out.is_64bit = false;
        bar_out.is_prefetchable = false;

        // Read size
        write_config_dword(device.bus, device.device, device.function, bar_offset, 0xFFFFFFFF);
        uint32_t size_mask = read_config_dword(device.bus, device.device, device.function, bar_offset);
        write_config_dword(device.bus, device.device, device.function, bar_offset, bar_value);  // Restore

        size_mask &= 0xFFFFFFFC;  // Clear flags
        bar_out.size = ~size_mask + 1;
        bar_out.address = bar_value & 0xFFFFFFFC;
    }

    return bar_out.is_valid();
}

void* PCI::map_bar(const BAR& bar) {
    if (!bar.is_valid() || !bar.is_mmio) {
        return nullptr;
    }

    // TODO: Use proper MMU mapping when available
    // For now, return direct physical address + kernel offset
    constexpr uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;
    return reinterpret_cast<void*>(bar.address + KERNEL_VIRTUAL_BASE);
}

void PCI::unmap_bar(void* mapped_address, size_t size) {
    // TODO: Implement proper unmapping when MMU is fully integrated
    (void)mapped_address;
    (void)size;
}

void PCI::enable_bus_master(const PCIDevice& device) {
    uint16_t cmd = read_config_word(device.bus, device.device, device.function, config::COMMAND);
    cmd |= command::BUS_MASTER;
    write_config_word(device.bus, device.device, device.function, config::COMMAND, cmd);
}

void PCI::enable_memory_space(const PCIDevice& device) {
    uint16_t cmd = read_config_word(device.bus, device.device, device.function, config::COMMAND);
    cmd |= command::MEMORY_SPACE;
    write_config_word(device.bus, device.device, device.function, config::COMMAND, cmd);
}

void PCI::enable_io_space(const PCIDevice& device) {
    uint16_t cmd = read_config_word(device.bus, device.device, device.function, config::COMMAND);
    cmd |= command::IO_SPACE;
    write_config_word(device.bus, device.device, device.function, config::COMMAND, cmd);
}

void PCI::disable_interrupts(const PCIDevice& device) {
    uint16_t cmd = read_config_word(device.bus, device.device, device.function, config::COMMAND);
    cmd |= command::INTERRUPT_DISABLE;
    write_config_word(device.bus, device.device, device.function, config::COMMAND, cmd);
}

} // namespace xinim::pci
