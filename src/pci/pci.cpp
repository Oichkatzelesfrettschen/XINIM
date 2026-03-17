// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// PCI Subsystem Implementation

#include <xinim/pci/pci.hpp>
#include <xinim/arch/x86/pci_cfg.hpp>
#include <cstring>

namespace xinim::pci {

// Global device list (fixed-size, no STL -- freestanding safe)
static constexpr size_t MAX_PCI_DEVICES = 64;
static PCIDevice g_devices[MAX_PCI_DEVICES];
static size_t g_device_count = 0;
static bool g_initialized = false;

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
    return xinim::arch::x86::pci::read_config_byte(bus, device, function, offset);
}

uint16_t PCI::read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    return xinim::arch::x86::pci::read_config_word(bus, device, function, offset);
}

uint32_t PCI::read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    return xinim::arch::x86::pci::read_config_dword(bus, device, function, offset);
}

void PCI::write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value) {
    xinim::arch::x86::pci::write_config_byte(bus, device, function, offset, value);
}

void PCI::write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value) {
    xinim::arch::x86::pci::write_config_word(bus, device, function, offset, value);
}

void PCI::write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) {
    xinim::arch::x86::pci::write_config_dword(bus, device, function, offset, value);
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
    for (uint8_t i = 0; i < 6; ++i) {
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

    return xinim::arch::x86::pci::map_mmio_physical(bar.address);
}

void PCI::unmap_bar(void* mapped_address, size_t size) {
    xinim::arch::x86::pci::unmap_mmio(mapped_address, size);
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
