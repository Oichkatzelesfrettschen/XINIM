// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// PCI (Peripheral Component Interconnect) Subsystem
// Provides PCI device enumeration, configuration space access, and BAR mapping

#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::pci {

// PCI Configuration Space Registers
namespace config {
    constexpr uint16_t VENDOR_ID = 0x00;
    constexpr uint16_t DEVICE_ID = 0x02;
    constexpr uint16_t COMMAND = 0x04;
    constexpr uint16_t STATUS = 0x06;
    constexpr uint16_t REVISION_ID = 0x08;
    constexpr uint16_t PROG_IF = 0x09;
    constexpr uint16_t SUBCLASS = 0x0A;
    constexpr uint16_t CLASS_CODE = 0x0B;
    constexpr uint16_t CACHE_LINE_SIZE = 0x0C;
    constexpr uint16_t LATENCY_TIMER = 0x0D;
    constexpr uint16_t HEADER_TYPE = 0x0E;
    constexpr uint16_t BIST = 0x0F;
    constexpr uint16_t BAR0 = 0x10;
    constexpr uint16_t BAR1 = 0x14;
    constexpr uint16_t BAR2 = 0x18;
    constexpr uint16_t BAR3 = 0x1C;
    constexpr uint16_t BAR4 = 0x20;
    constexpr uint16_t BAR5 = 0x24;
    constexpr uint16_t CARDBUS_CIS = 0x28;
    constexpr uint16_t SUBSYSTEM_VENDOR_ID = 0x2C;
    constexpr uint16_t SUBSYSTEM_ID = 0x2E;
    constexpr uint16_t EXPANSION_ROM = 0x30;
    constexpr uint16_t CAPABILITIES_PTR = 0x34;
    constexpr uint16_t INTERRUPT_LINE = 0x3C;
    constexpr uint16_t INTERRUPT_PIN = 0x3D;
    constexpr uint16_t MIN_GRANT = 0x3E;
    constexpr uint16_t MAX_LATENCY = 0x3F;
}

// PCI Command Register Bits
namespace command {
    constexpr uint16_t IO_SPACE = (1 << 0);
    constexpr uint16_t MEMORY_SPACE = (1 << 1);
    constexpr uint16_t BUS_MASTER = (1 << 2);
    constexpr uint16_t SPECIAL_CYCLES = (1 << 3);
    constexpr uint16_t MWI_ENABLE = (1 << 4);
    constexpr uint16_t VGA_PALETTE_SNOOP = (1 << 5);
    constexpr uint16_t PARITY_ERROR_RESPONSE = (1 << 6);
    constexpr uint16_t SERR_ENABLE = (1 << 8);
    constexpr uint16_t FAST_BACK_TO_BACK = (1 << 9);
    constexpr uint16_t INTERRUPT_DISABLE = (1 << 10);
}

// PCI Header Types
namespace header {
    constexpr uint8_t STANDARD = 0x00;
    constexpr uint8_t PCI_TO_PCI_BRIDGE = 0x01;
    constexpr uint8_t CARDBUS_BRIDGE = 0x02;
    constexpr uint8_t MULTIFUNCTION = 0x80;
}

// PCI Class Codes
namespace class_code {
    constexpr uint8_t UNCLASSIFIED = 0x00;
    constexpr uint8_t MASS_STORAGE = 0x01;
    constexpr uint8_t NETWORK = 0x02;
    constexpr uint8_t DISPLAY = 0x03;
    constexpr uint8_t MULTIMEDIA = 0x04;
    constexpr uint8_t MEMORY = 0x05;
    constexpr uint8_t BRIDGE = 0x06;
    constexpr uint8_t COMMUNICATION = 0x07;
    constexpr uint8_t SYSTEM_PERIPHERAL = 0x08;
    constexpr uint8_t INPUT_DEVICE = 0x09;
    constexpr uint8_t DOCKING_STATION = 0x0A;
    constexpr uint8_t PROCESSOR = 0x0B;
    constexpr uint8_t SERIAL_BUS = 0x0C;
}

// Mass Storage Subclasses
namespace mass_storage {
    constexpr uint8_t SCSI = 0x00;
    constexpr uint8_t IDE = 0x01;
    constexpr uint8_t FLOPPY = 0x02;
    constexpr uint8_t IPI = 0x03;
    constexpr uint8_t RAID = 0x04;
    constexpr uint8_t ATA = 0x05;
    constexpr uint8_t SATA = 0x06;
    constexpr uint8_t SAS = 0x07;
    constexpr uint8_t NVME = 0x08;
}

// Network Subclasses
namespace network {
    constexpr uint8_t ETHERNET = 0x00;
    constexpr uint8_t TOKEN_RING = 0x01;
    constexpr uint8_t FDDI = 0x02;
    constexpr uint8_t ATM = 0x03;
    constexpr uint8_t ISDN = 0x04;
    constexpr uint8_t WORLDFIP = 0x05;
    constexpr uint8_t PICMG = 0x06;
}

// BAR (Base Address Register) Information
struct BAR {
    uint64_t address;          // Physical address
    uint64_t size;             // Size in bytes
    bool is_mmio;              // True if memory-mapped, false if I/O ports
    bool is_64bit;             // True if 64-bit BAR
    bool is_prefetchable;      // True if prefetchable memory

    BAR()
        : address(0)
        , size(0)
        , is_mmio(false)
        , is_64bit(false)
        , is_prefetchable(false) {}

    bool is_valid() const { return size > 0; }
};

// PCI Device Descriptor
struct PCIDevice {
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;

    uint8_t interrupt_line;
    uint8_t interrupt_pin;

    BAR bars[6];  // Standard PCI devices have up to 6 BARs

    PCIDevice()
        : bus(0), device(0), function(0)
        , vendor_id(0xFFFF), device_id(0xFFFF)
        , class_code(0), subclass(0), prog_if(0)
        , revision_id(0), header_type(0)
        , interrupt_line(0), interrupt_pin(0)
        , bars{} {}

    bool is_valid() const { return vendor_id != 0xFFFF; }

    // Helper: Get BDF (Bus:Device.Function) as string
    constexpr uint32_t bdf() const {
        return (static_cast<uint32_t>(bus) << 8) |
               (static_cast<uint32_t>(device) << 3) |
               static_cast<uint32_t>(function);
    }
};

// PCI Subsystem Interface
class PCI {
public:
    // Initialize the PCI subsystem
    static bool initialize();

    // Shutdown the PCI subsystem
    static void shutdown();

    // Configuration space access
    static uint8_t read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
    static uint16_t read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
    static uint32_t read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

    static void write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value);
    static void write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value);
    static void write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value);

    // Device enumeration
    static bool enumerate_devices();
    static size_t get_device_count();
    static const PCIDevice* get_device(size_t index);

    // Device lookup
    static const PCIDevice* find_device(uint16_t vendor_id, uint16_t device_id);
    static const PCIDevice* find_device_by_class(uint8_t class_code, uint8_t subclass);

    // BAR operations
    static bool read_bar(const PCIDevice& device, uint8_t bar_index, BAR& bar_out);
    static void* map_bar(const BAR& bar);
    static void unmap_bar(void* mapped_address, size_t size);

    // Device control
    static void enable_bus_master(const PCIDevice& device);
    static void enable_memory_space(const PCIDevice& device);
    static void enable_io_space(const PCIDevice& device);
    static void disable_interrupts(const PCIDevice& device);

private:
    PCI() = delete;
    ~PCI() = delete;
    PCI(const PCI&) = delete;
    PCI& operator=(const PCI&) = delete;
};

} // namespace xinim::pci
