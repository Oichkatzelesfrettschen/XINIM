/**
 * @file ahci.cpp
 * @brief AHCI (Advanced Host Controller Interface) SATA Driver Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Based on AHCI Specification 1.3.1
 */

#include <xinim/drivers/ahci.hpp>
#include <xinim/kernel/irq.hpp>
#include <xinim/mm/dma_allocator.hpp>
#include <xinim/block/ahci_blockdev.hpp>
#include <xinim/block/blockdev.hpp>
#include <cstring>
#include <iostream>

namespace xinim::drivers {

// Static interrupt handler wrapper
static bool ahci_irq_handler(uint8_t vector, void* context) {
    (void)vector;
    AHCIDriver* driver = static_cast<AHCIDriver*>(context);
    if (driver) {
        driver->handle_interrupt();
        return true;
    }
    return false;
}

AHCIDriver::AHCIDriver()
    : abar_(nullptr)
    , abar_phys_(0)
    , abar_size_(0)
    , ports_implemented_(0)
    , num_ports_(0)
    , num_command_slots_(0)
    , supports_64bit_(false)
    , ports_(nullptr)
{
}

AHCIDriver::~AHCIDriver() {
    shutdown();
}

bool AHCIDriver::probe(uint16_t vendor_id, uint16_t device_id) {
    // Check for Intel AHCI controllers
    if (vendor_id == 0x8086) {
        // Intel AHCI controller IDs
        switch (device_id) {
            case 0x2922:  // ICH9
            case 0x2829:  // ICH8M
            case 0x2682:  // ESB2
            case 0x27C1:  // ICH7
            case 0x2681:  // ESB
            case 0x269E:  // ICH6M
                std::cout << "[AHCI] Detected Intel AHCI controller: 0x"
                          << std::hex << device_id << std::dec << std::endl;
                return true;
        }
    }

    // TODO: Add support for other vendors (AMD, VIA, NVIDIA, etc.)

    return false;
}

bool AHCIDriver::initialize() {
    std::cout << "[AHCI] Initializing AHCI controller..." << std::endl;

    // TODO: Get ABAR from PCI configuration
    // For now, assume it's been set up
    if (!abar_) {
        std::cerr << "[AHCI] Error: ABAR not configured" << std::endl;
        return false;
    }

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);

    // Read capabilities
    uint32_t cap = hba->cap;
    num_ports_ = (cap & CAPBits::NP_MASK) + 1;
    num_command_slots_ = ((cap & CAPBits::NCS_MASK) >> CAPBits::NCS_SHIFT) + 1;
    supports_64bit_ = (cap & CAPBits::S64A) != 0;

    std::cout << "[AHCI] Capabilities: " << num_ports_ << " ports, "
              << num_command_slots_ << " command slots, "
              << (supports_64bit_ ? "64-bit" : "32-bit") << std::endl;

    // Reset HBA
    reset_hba();

    // Enable AHCI mode
    enable_ahci();

    // Detect implemented ports
    ports_implemented_ = hba->pi;
    std::cout << "[AHCI] Ports implemented: 0x" << std::hex
              << ports_implemented_ << std::dec << std::endl;

    // Allocate port data structures
    ports_ = new PortData[MAX_PORTS];
    std::memset(ports_, 0, sizeof(PortData) * MAX_PORTS);

    // Probe and initialize each port
    for (uint32_t i = 0; i < MAX_PORTS; ++i) {
        if (is_port_implemented(i)) {
            if (probe_port(i)) {
                std::cout << "[AHCI] Port " << i << ": Device detected" << std::endl;
                init_port(i);
                rebase_port(i);
                ports_[i].active = true;

                // Register device with BlockDeviceManager
                register_block_device(i);
            }
        }
    }

    // TODO: Allocate and register IRQ
    // For now, skip IRQ registration

    std::cout << "[AHCI] Initialization complete" << std::endl;
    return true;
}

void AHCIDriver::shutdown() {
    if (!abar_) return;

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);

    // Stop all ports
    for (uint32_t i = 0; i < MAX_PORTS; ++i) {
        if (ports_ && ports_[i].active) {
            stop_command_engine(i);

            // Free DMA buffers
            if (ports_[i].command_list && ports_[i].command_list_phys) {
                xinim::mm::DMABuffer buffer;
                buffer.virtual_addr = ports_[i].command_list;
                buffer.physical_addr = ports_[i].command_list_phys;
                buffer.size = COMMAND_LIST_SIZE;
                xinim::mm::DMAAllocator::free(buffer);
            }

            if (ports_[i].received_fis && ports_[i].received_fis_phys) {
                xinim::mm::DMABuffer buffer;
                buffer.virtual_addr = ports_[i].received_fis;
                buffer.physical_addr = ports_[i].received_fis_phys;
                buffer.size = RECEIVED_FIS_SIZE;
                xinim::mm::DMAAllocator::free(buffer);
            }
        }
    }

    // Disable interrupts
    hba->ghc &= ~GHCBits::IE;

    // Free port data
    if (ports_) {
        delete[] ports_;
        ports_ = nullptr;
    }

    std::cout << "[AHCI] Driver shutdown complete" << std::endl;
}

uint32_t AHCIDriver::read_hba_reg(uint32_t reg) const {
    if (!abar_) return 0;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(abar_ + reg);
    return *addr;
}

void AHCIDriver::write_hba_reg(uint32_t reg, uint32_t value) {
    if (!abar_) return;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(abar_ + reg);
    *addr = value;
}

uint32_t AHCIDriver::read_port_reg(uint8_t port, uint32_t reg) const {
    if (!abar_ || port >= MAX_PORTS) return 0;
    uint32_t offset = PORT_BASE + (port * PORT_SIZE) + reg;
    return read_hba_reg(offset);
}

void AHCIDriver::write_port_reg(uint8_t port, uint32_t reg, uint32_t value) {
    if (!abar_ || port >= MAX_PORTS) return;
    uint32_t offset = PORT_BASE + (port * PORT_SIZE) + reg;
    write_hba_reg(offset, value);
}

void AHCIDriver::reset_hba() {
    std::cout << "[AHCI] Resetting HBA..." << std::endl;

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);

    // Set HBA reset bit
    hba->ghc |= GHCBits::HR;

    // Wait for reset to complete (timeout: 1 second)
    for (int i = 0; i < 1000; ++i) {
        if (!(hba->ghc & GHCBits::HR)) {
            std::cout << "[AHCI] HBA reset complete" << std::endl;
            return;
        }
        // TODO: Proper delay (1ms)
        for (volatile int j = 0; j < 10000; ++j) {}
    }

    std::cerr << "[AHCI] Warning: HBA reset timeout" << std::endl;
}

void AHCIDriver::enable_ahci() {
    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);

    // Enable AHCI mode
    hba->ghc |= GHCBits::AE;

    std::cout << "[AHCI] AHCI mode enabled" << std::endl;
}

bool AHCIDriver::wait_for_not_busy(uint8_t port, uint32_t timeout_ms) {
    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    for (uint32_t i = 0; i < timeout_ms; ++i) {
        uint32_t tfd = port_regs->tfd;

        // Check BSY and DRQ bits
        if (!(tfd & (0x80 | 0x08))) {
            return true;  // Not busy
        }

        // TODO: Proper 1ms delay
        for (volatile int j = 0; j < 10000; ++j) {}
    }

    return false;  // Timeout
}

void AHCIDriver::start_command_engine(uint8_t port) {
    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Wait for command engine to stop
    while (port_regs->cmd & PortCMDBits::CR) {
        // TODO: Proper delay
    }

    // Start FIS receive
    port_regs->cmd |= PortCMDBits::FRE;

    // Start command engine
    port_regs->cmd |= PortCMDBits::ST;
}

void AHCIDriver::stop_command_engine(uint8_t port) {
    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Clear ST bit
    port_regs->cmd &= ~PortCMDBits::ST;

    // Wait for CR to clear
    while (port_regs->cmd & PortCMDBits::CR) {
        // TODO: Proper delay
    }

    // Clear FRE bit
    port_regs->cmd &= ~PortCMDBits::FRE;

    // Wait for FR to clear
    while (port_regs->cmd & PortCMDBits::FR) {
        // TODO: Proper delay
    }
}

uint32_t AHCIDriver::get_port_count() const {
    return num_ports_;
}

bool AHCIDriver::is_port_implemented(uint8_t port) const {
    if (port >= MAX_PORTS) return false;
    return (ports_implemented_ & (1 << port)) != 0;
}

bool AHCIDriver::probe_port(uint8_t port) {
    if (!is_port_implemented(port)) {
        return false;
    }

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Check SATA status
    uint32_t ssts = port_regs->ssts;
    uint8_t det = ssts & 0x0F;           // Device detection
    uint8_t ipm = (ssts >> 8) & 0x0F;    // Interface power management

    // DET should be 3 (device present and PHY communication established)
    // IPM should be 1 (active)
    if (det != 3 || ipm != 1) {
        return false;
    }

    return true;
}

void AHCIDriver::init_port(uint8_t port) {
    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Stop command engine before initialization
    stop_command_engine(port);

    // Clear error register
    port_regs->serr = 0xFFFFFFFF;

    // Clear interrupt status
    port_regs->is = 0xFFFFFFFF;

    // Get device signature
    uint32_t sig = port_regs->sig;
    ports_[port].signature = static_cast<DeviceSignature>(sig);

    std::cout << "[AHCI] Port " << (int)port << " signature: 0x"
              << std::hex << sig << std::dec << std::endl;
}

void AHCIDriver::rebase_port(uint8_t port) {
    std::cout << "[AHCI] Rebasing port " << (int)port << "..." << std::endl;

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Stop command engine
    stop_command_engine(port);

    // Allocate command list (1KB, 1KB aligned)
    xinim::mm::DMABuffer cmdlist_buffer = xinim::mm::DMAAllocator::allocate(
        COMMAND_LIST_SIZE,
        xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
    );

    if (!cmdlist_buffer.is_valid()) {
        std::cerr << "[AHCI] Error: Failed to allocate command list for port " << (int)port << std::endl;
        return;
    }

    ports_[port].command_list = cmdlist_buffer.virtual_addr;
    ports_[port].command_list_phys = cmdlist_buffer.physical_addr;

    // Allocate FIS receive area (256 bytes, 256 byte aligned)
    xinim::mm::DMABuffer fis_buffer = xinim::mm::DMAAllocator::allocate(
        RECEIVED_FIS_SIZE,
        xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
    );

    if (!fis_buffer.is_valid()) {
        std::cerr << "[AHCI] Error: Failed to allocate FIS buffer for port " << (int)port << std::endl;
        return;
    }

    ports_[port].received_fis = fis_buffer.virtual_addr;
    ports_[port].received_fis_phys = fis_buffer.physical_addr;

    // Set command list base address
    port_regs->clb = static_cast<uint32_t>(ports_[port].command_list_phys & 0xFFFFFFFF);
    port_regs->clbu = static_cast<uint32_t>(ports_[port].command_list_phys >> 32);

    // Set FIS base address
    port_regs->fb = static_cast<uint32_t>(ports_[port].received_fis_phys & 0xFFFFFFFF);
    port_regs->fbu = static_cast<uint32_t>(ports_[port].received_fis_phys >> 32);

    // Allocate command tables for each command slot
    HBACommandHeader* cmdheader = static_cast<HBACommandHeader*>(ports_[port].command_list);

    for (uint32_t i = 0; i < num_command_slots_; ++i) {
        // Each command table: 256 bytes + 8KB for PRDT (max 128 entries * 16 bytes + padding)
        size_t cmdtable_size = sizeof(HBACommandTable) + (127 * sizeof(HBAPRDTEntry));

        xinim::mm::DMABuffer cmdtable_buffer = xinim::mm::DMAAllocator::allocate(
            cmdtable_size,
            xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
        );

        if (!cmdtable_buffer.is_valid()) {
            std::cerr << "[AHCI] Error: Failed to allocate command table " << i << std::endl;
            continue;
        }

        cmdheader[i].prdtl = 0;
        cmdheader[i].prdbc = 0;
        cmdheader[i].ctba = static_cast<uint32_t>(cmdtable_buffer.physical_addr & 0xFFFFFFFF);
        cmdheader[i].ctbau = static_cast<uint32_t>(cmdtable_buffer.physical_addr >> 32);
    }

    // Enable interrupts for this port
    port_regs->ie = PortISBits::DHRS | PortISBits::PSS | PortISBits::DSS |
                    PortISBits::SDBS | PortISBits::TFES;

    // Start command engine
    start_command_engine(port);

    std::cout << "[AHCI] Port " << (int)port << " rebase complete" << std::endl;
}

int AHCIDriver::find_command_slot(uint8_t port) {
    if (!ports_ || !ports_[port].active) {
        return -1;
    }

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    // Check which command slots are free
    uint32_t slots = (port_regs->sact | port_regs->ci);

    for (uint32_t i = 0; i < num_command_slots_; ++i) {
        if (!(slots & (1 << i))) {
            return static_cast<int>(i);
        }
    }

    return -1;  // No free slots
}

bool AHCIDriver::execute_command(uint8_t port, uint8_t* fis, size_t fis_size,
                                 uint8_t* buffer, size_t buffer_size, bool write) {
    if (!ports_ || !ports_[port].active) {
        return false;
    }

    // Find free command slot
    int slot = find_command_slot(port);
    if (slot < 0) {
        std::cerr << "[AHCI] No free command slots on port " << (int)port << std::endl;
        return false;
    }

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);
    volatile HBAPort* port_regs = &hba->ports[port];

    HBACommandHeader* cmdheader = static_cast<HBACommandHeader*>(ports_[port].command_list);
    cmdheader += slot;

    // Set command header
    cmdheader->cfl = fis_size / sizeof(uint32_t);  // FIS length in DWORDs
    cmdheader->w = write ? 1 : 0;                   // Write direction
    cmdheader->prdtl = 1;                           // One PRDT entry

    // Get command table
    uint64_t cmdtable_phys = static_cast<uint64_t>(cmdheader->ctba) |
                             (static_cast<uint64_t>(cmdheader->ctbau) << 32);

    // TODO: Translate physical to virtual address properly
    HBACommandTable* cmdtable = reinterpret_cast<HBACommandTable*>(
        xinim::mm::DMAAllocator::phys_to_virt(cmdtable_phys)
    );

    if (!cmdtable) {
        std::cerr << "[AHCI] Error: Cannot map command table" << std::endl;
        return false;
    }

    // Clear command table
    std::memset(cmdtable, 0, sizeof(HBACommandTable));

    // Copy FIS to command table
    std::memcpy(cmdtable->cfis, fis, fis_size);

    // Setup PRDT entry
    if (buffer && buffer_size > 0) {
        uint64_t buffer_phys = xinim::mm::DMAAllocator::virt_to_phys(buffer);

        cmdtable->prdt_entry[0].dba = static_cast<uint32_t>(buffer_phys & 0xFFFFFFFF);
        cmdtable->prdt_entry[0].dbau = static_cast<uint32_t>(buffer_phys >> 32);
        cmdtable->prdt_entry[0].dbc = (buffer_size - 1) & 0x3FFFFF;  // 0-based count
        cmdtable->prdt_entry[0].i = 1;  // Interrupt on completion
    }

    // Wait for port to be ready
    if (!wait_for_not_busy(port, 1000)) {
        std::cerr << "[AHCI] Port " << (int)port << " busy timeout" << std::endl;
        return false;
    }

    // Issue command
    port_regs->ci = (1 << slot);

    // Wait for completion
    for (int i = 0; i < 1000; ++i) {
        if (!(port_regs->ci & (1 << slot))) {
            // Command completed
            break;
        }

        // Check for errors
        if (port_regs->is & PortISBits::TFES) {
            std::cerr << "[AHCI] Task file error on port " << (int)port << std::endl;
            port_regs->is = PortISBits::TFES;  // Clear error
            return false;
        }

        // TODO: Proper 1ms delay
        for (volatile int j = 0; j < 10000; ++j) {}
    }

    // Check if command is still running (timeout)
    if (port_regs->ci & (1 << slot)) {
        std::cerr << "[AHCI] Command timeout on port " << (int)port << std::endl;
        return false;
    }

    return true;
}

bool AHCIDriver::read_sectors(uint8_t port, uint64_t lba, uint16_t count, uint8_t* buffer) {
    if (!buffer || count == 0) {
        return false;
    }

    if (!ports_ || !ports_[port].active) {
        return false;
    }

    std::cout << "[AHCI] Reading " << count << " sectors from LBA " << lba
              << " on port " << (int)port << std::endl;

    // Build READ DMA EXT FIS
    FIS_REG_H2D fis;
    std::memset(&fis, 0, sizeof(fis));

    fis.fis_type = static_cast<uint8_t>(FISType::REG_H2D);
    fis.c = 1;  // Command register
    fis.command = ATACommand::READ_DMA_EXT;

    // LBA (48-bit addressing)
    fis.lba0 = lba & 0xFF;
    fis.lba1 = (lba >> 8) & 0xFF;
    fis.lba2 = (lba >> 16) & 0xFF;
    fis.lba3 = (lba >> 24) & 0xFF;
    fis.lba4 = (lba >> 32) & 0xFF;
    fis.lba5 = (lba >> 40) & 0xFF;

    fis.device = (1 << 6);  // LBA mode

    // Sector count
    fis.countl = count & 0xFF;
    fis.counth = (count >> 8) & 0xFF;

    // Execute command
    size_t buffer_size = count * 512;  // Standard sector size
    return execute_command(port, reinterpret_cast<uint8_t*>(&fis), sizeof(fis),
                          buffer, buffer_size, false);
}

bool AHCIDriver::write_sectors(uint8_t port, uint64_t lba, uint16_t count, const uint8_t* buffer) {
    if (!buffer || count == 0) {
        return false;
    }

    if (!ports_ || !ports_[port].active) {
        return false;
    }

    std::cout << "[AHCI] Writing " << count << " sectors to LBA " << lba
              << " on port " << (int)port << std::endl;

    // Build WRITE DMA EXT FIS
    FIS_REG_H2D fis;
    std::memset(&fis, 0, sizeof(fis));

    fis.fis_type = static_cast<uint8_t>(FISType::REG_H2D);
    fis.c = 1;  // Command register
    fis.command = ATACommand::WRITE_DMA_EXT;

    // LBA (48-bit addressing)
    fis.lba0 = lba & 0xFF;
    fis.lba1 = (lba >> 8) & 0xFF;
    fis.lba2 = (lba >> 16) & 0xFF;
    fis.lba3 = (lba >> 24) & 0xFF;
    fis.lba4 = (lba >> 32) & 0xFF;
    fis.lba5 = (lba >> 40) & 0xFF;

    fis.device = (1 << 6);  // LBA mode

    // Sector count
    fis.countl = count & 0xFF;
    fis.counth = (count >> 8) & 0xFF;

    // Execute command
    size_t buffer_size = count * 512;  // Standard sector size
    return execute_command(port, reinterpret_cast<uint8_t*>(&fis), sizeof(fis),
                          const_cast<uint8_t*>(buffer), buffer_size, true);
}

AHCIDriver::DeviceSignature AHCIDriver::get_device_type(uint8_t port) const {
    if (!ports_ || port >= MAX_PORTS) {
        return DeviceSignature::ATA;
    }
    return ports_[port].signature;
}

bool AHCIDriver::get_drive_info(uint8_t port, uint64_t& sectors, uint32_t& sector_size) {
    // TODO: Issue IDENTIFY command to get drive information
    // For now, return defaults
    sectors = 0;
    sector_size = 512;
    return false;
}

void AHCIDriver::handle_interrupt() {
    if (!abar_) return;

    volatile HBAMemory* hba = reinterpret_cast<volatile HBAMemory*>(abar_);

    // Check which ports have pending interrupts
    uint32_t is = hba->is;

    for (uint32_t i = 0; i < MAX_PORTS; ++i) {
        if (is & (1 << i)) {
            // Port has pending interrupt
            volatile HBAPort* port_regs = &hba->ports[i];
            uint32_t port_is = port_regs->is;

            // Check for errors
            if (port_is & (PortISBits::TFES | PortISBits::HBFS | PortISBits::HBDS | PortISBits::IFS)) {
                std::cerr << "[AHCI] Error on port " << i << ": IS=0x"
                          << std::hex << port_is << std::dec << std::endl;
            }

            // Check for command completion
            if (port_is & PortISBits::DHRS) {
                // D2H Register FIS received - command completed
                std::cout << "[AHCI] Command completed on port " << i << std::endl;
            }

            // Clear port interrupt status
            port_regs->is = port_is;
        }
    }

    // Clear HBA interrupt status
    hba->is = is;
}

void AHCIDriver::register_block_device(uint8_t port) {
    if (port >= MAX_PORTS) {
        std::cerr << "[AHCI] Invalid port number: " << static_cast<int>(port) << std::endl;
        return;
    }

    // Create shared pointer to this driver (needed for AHCIBlockDevice)
    // Note: This assumes the driver is managed externally
    // TODO: Improve lifecycle management
    auto driver_ptr = std::shared_ptr<AHCIDriver>(this, [](AHCIDriver*){});

    // Create block device for this port
    auto block_dev = std::make_shared<xinim::block::AHCIBlockDevice>(driver_ptr, port);

    // Register with BlockDeviceManager
    auto& mgr = xinim::block::BlockDeviceManager::instance();
    std::string dev_name = mgr.register_device(block_dev);

    if (!dev_name.empty()) {
        std::cout << "[AHCI] Registered block device: " << dev_name << std::endl;

        // Scan for partitions
        int partitions = mgr.scan_partitions(block_dev);
        std::cout << "[AHCI] Found " << partitions << " partitions on " << dev_name << std::endl;
    } else {
        std::cerr << "[AHCI] Failed to register block device for port " << static_cast<int>(port) << std::endl;
    }
}

} // namespace xinim::drivers
