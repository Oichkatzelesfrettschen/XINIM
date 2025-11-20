/**
 * @file e1000.cpp
 * @brief Intel E1000 Gigabit Ethernet Driver Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/drivers/e1000.hpp>
#include <xinim/kernel/irq.hpp>
#include <xinim/mm/dma_allocator.hpp>
#include <cstring>
#include <iostream>

namespace xinim::drivers {

// Static interrupt handler wrapper
static bool e1000_irq_handler(uint8_t vector, void* context) {
    (void)vector;
    E1000Driver* driver = static_cast<E1000Driver*>(context);
    if (driver) {
        driver->handle_interrupt();
        return true;
    }
    return false;
}

E1000Driver::E1000Driver()
    : mmio_base_(nullptr)
    , mmio_phys_(0)
    , mmio_size_(0)
    , has_eeprom_(false)
    , link_up_(false)
    , irq_vector_(0)
    , rx_descriptors_(nullptr)
    , tx_descriptors_(nullptr)
    , rx_descriptors_phys_(0)
    , tx_descriptors_phys_(0)
    , rx_buffers_(nullptr)
    , tx_buffers_(nullptr)
    , rx_buffers_phys_(nullptr)
    , tx_buffers_phys_(nullptr)
    , rx_tail_(0)
    , tx_tail_(0)
    , rx_desc_dma_buffer_(nullptr)
    , tx_desc_dma_buffer_(nullptr)
{
    std::memset(mac_address_, 0, sizeof(mac_address_));
}

E1000Driver::~E1000Driver() {
    shutdown();
}

bool E1000Driver::probe(uint16_t vendor_id, uint16_t device_id) {
    // Check if this is an Intel device
    if (vendor_id != 0x8086) {
        return false;
    }

    // Check if device ID matches any supported E1000 variant
    switch (static_cast<DeviceID>(device_id)) {
        case DeviceID::E1000_82540EM:
        case DeviceID::E1000_82545EM:
        case DeviceID::E1000_82546EB:
        case DeviceID::E1000_82545GM:
        case DeviceID::E1000_82566DM:
        case DeviceID::E1000_82571EB:
        case DeviceID::E1000_82572EI:
        case DeviceID::E1000_82573E:
        case DeviceID::E1000_82574L:
        case DeviceID::E1000_82583V:
            std::cout << "[E1000] Detected supported device ID: 0x" 
                      << std::hex << device_id << std::dec << std::endl;
            return true;
        default:
            return false;
    }
}

bool E1000Driver::initialize() {
    std::cout << "[E1000] Initializing driver..." << std::endl;

    // TODO: Get MMIO base address from PCI configuration
    // For now, assume it's been set up

    if (!mmio_base_) {
        std::cerr << "[E1000] Error: MMIO base not configured" << std::endl;
        return false;
    }

    // Reset the hardware
    reset_hardware();

    // Detect EEPROM presence
    has_eeprom_ = detect_eeprom();
    if (has_eeprom_) {
        std::cout << "[E1000] EEPROM detected" << std::endl;
        read_mac_from_eeprom();
    } else {
        std::cout << "[E1000] No EEPROM, using hardcoded MAC" << std::endl;
        // Default MAC address for testing
        mac_address_[0] = 0x52;
        mac_address_[1] = 0x54;
        mac_address_[2] = 0x00;
        mac_address_[3] = 0x12;
        mac_address_[4] = 0x34;
        mac_address_[5] = 0x56;
    }

    std::cout << "[E1000] MAC Address: "
              << std::hex
              << (int)mac_address_[0] << ":"
              << (int)mac_address_[1] << ":"
              << (int)mac_address_[2] << ":"
              << (int)mac_address_[3] << ":"
              << (int)mac_address_[4] << ":"
              << (int)mac_address_[5]
              << std::dec << std::endl;

    // Allocate IRQ and register handler
    irq_vector_ = xinim::kernel::IRQ::allocate_irq_dynamic("e1000");
    if (irq_vector_ == 0) {
        std::cerr << "[E1000] Error: Failed to allocate IRQ" << std::endl;
        return false;
    }
    std::cout << "[E1000] Allocated IRQ vector: " << (int)irq_vector_ << std::endl;

    if (!xinim::kernel::IRQ::register_handler(irq_vector_, e1000_irq_handler, this)) {
        std::cerr << "[E1000] Error: Failed to register IRQ handler" << std::endl;
        xinim::kernel::IRQ::free_irq(irq_vector_);
        return false;
    }

    // Setup descriptor rings and buffers
    setup_rx_descriptors();
    setup_tx_descriptors();

    // Initialize receive and transmit
    init_rx();
    init_tx();

    // Enable IRQ
    xinim::kernel::IRQ::enable_irq(irq_vector_);

    // Enable interrupts
    write_reg(Registers::IMS,
              InterruptBits::RXT0 |
              InterruptBits::LSC |
              InterruptBits::RXDMT0 |
              InterruptBits::RXO |
              InterruptBits::TXDW);

    // Set link up
    uint32_t ctrl = read_reg(Registers::CTRL);
    ctrl |= CtrlBits::SLU;
    write_reg(Registers::CTRL, ctrl);

    // Check link status
    static constexpr uint32_t STATUS_LU = 0x02;  // Link Up bit
    uint32_t status = read_reg(Registers::STATUS);
    link_up_ = (status & STATUS_LU) != 0;

    std::cout << "[E1000] Link status: " << (link_up_ ? "UP" : "DOWN") << std::endl;
    std::cout << "[E1000] Initialization complete" << std::endl;

    return true;
}

void E1000Driver::shutdown() {
    if (!mmio_base_) return;

    // Disable interrupts
    write_reg(Registers::IMC, 0xFFFFFFFF);

    // Disable receiver and transmitter
    write_reg(Registers::RCTL, 0);
    write_reg(Registers::TCTL, 0);

    // Unregister IRQ handler
    if (irq_vector_ != 0) {
        xinim::kernel::IRQ::disable_irq(irq_vector_);
        xinim::kernel::IRQ::unregister_handler(irq_vector_, e1000_irq_handler);
        xinim::kernel::IRQ::free_irq(irq_vector_);
        irq_vector_ = 0;
    }

    // Free RX buffers
    if (rx_buffers_) {
        for (size_t i = 0; i < RX_DESC_COUNT; ++i) {
            if (rx_buffers_[i] && rx_buffers_phys_[i]) {
                xinim::mm::DMABuffer buffer;
                buffer.virtual_addr = rx_buffers_[i];
                buffer.physical_addr = rx_buffers_phys_[i];
                buffer.size = RX_BUFFER_SIZE;
                xinim::mm::DMAAllocator::free(buffer);
            }
        }
        delete[] rx_buffers_;
        delete[] rx_buffers_phys_;
        rx_buffers_ = nullptr;
        rx_buffers_phys_ = nullptr;
    }

    // Free TX buffers
    if (tx_buffers_) {
        for (size_t i = 0; i < TX_DESC_COUNT; ++i) {
            if (tx_buffers_[i] && tx_buffers_phys_[i]) {
                xinim::mm::DMABuffer buffer;
                buffer.virtual_addr = tx_buffers_[i];
                buffer.physical_addr = tx_buffers_phys_[i];
                buffer.size = TX_BUFFER_SIZE;
                xinim::mm::DMAAllocator::free(buffer);
            }
        }
        delete[] tx_buffers_;
        delete[] tx_buffers_phys_;
        tx_buffers_ = nullptr;
        tx_buffers_phys_ = nullptr;
    }

    // Free RX descriptor ring
    if (rx_descriptors_ && rx_desc_dma_buffer_) {
        xinim::mm::DMABuffer buffer;
        buffer.virtual_addr = rx_desc_dma_buffer_;
        buffer.physical_addr = rx_descriptors_phys_;
        buffer.size = sizeof(RxDescriptor) * RX_DESC_COUNT;
        xinim::mm::DMAAllocator::free(buffer);
        rx_descriptors_ = nullptr;
        rx_desc_dma_buffer_ = nullptr;
    }

    // Free TX descriptor ring
    if (tx_descriptors_ && tx_desc_dma_buffer_) {
        xinim::mm::DMABuffer buffer;
        buffer.virtual_addr = tx_desc_dma_buffer_;
        buffer.physical_addr = tx_descriptors_phys_;
        buffer.size = sizeof(TxDescriptor) * TX_DESC_COUNT;
        xinim::mm::DMAAllocator::free(buffer);
        tx_descriptors_ = nullptr;
        tx_desc_dma_buffer_ = nullptr;
    }

    std::cout << "[E1000] Driver shutdown complete" << std::endl;
}

uint32_t E1000Driver::read_reg(uint32_t reg) const {
    if (!mmio_base_) return 0;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(mmio_base_ + reg);
    return *addr;
}

void E1000Driver::write_reg(uint32_t reg, uint32_t value) {
    if (!mmio_base_) return;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(mmio_base_ + reg);
    *addr = value;
}

void E1000Driver::write_flush() {
    read_reg(Registers::STATUS);
}

void E1000Driver::reset_hardware() {
    std::cout << "[E1000] Resetting hardware..." << std::endl;

    // Disable interrupts
    write_reg(Registers::IMC, 0xFFFFFFFF);

    // Issue global reset
    uint32_t ctrl = read_reg(Registers::CTRL);
    ctrl |= CtrlBits::RST;
    write_reg(Registers::CTRL, ctrl);
    write_flush();

    // Wait for reset to complete (typically takes ~1ms)
    // TODO: Proper delay mechanism
    for (volatile int i = 0; i < 1000000; i++) {}

    // Disable interrupts again after reset
    write_reg(Registers::IMC, 0xFFFFFFFF);

    // Clear pending interrupts
    read_reg(Registers::ICR);

    std::cout << "[E1000] Hardware reset complete" << std::endl;
}

bool E1000Driver::detect_eeprom() {
    // Try to read EEPROM control register
    write_reg(Registers::EECD, 0x01);
    write_flush();

    for (int i = 0; i < 1000; i++) {
        uint32_t eecd = read_reg(Registers::EECD);
        if (eecd & 0x10) {
            return true;
        }
    }

    return false;
}

uint16_t E1000Driver::read_eeprom(uint8_t addr) {
    uint32_t tmp = 0;

    // Set READ opcode and address
    write_reg(Registers::EERD, ((uint32_t)addr << 8) | 0x01);

    // Wait for read to complete
    for (int i = 0; i < 1000; i++) {
        tmp = read_reg(Registers::EERD);
        if (tmp & 0x10) {
            return (uint16_t)((tmp >> 16) & 0xFFFF);
        }
    }

    return 0xFFFF;
}

void E1000Driver::read_mac_from_eeprom() {
    if (!has_eeprom_) return;

    // MAC address is stored in EEPROM words 0-2
    uint16_t mac_word0 = read_eeprom(0);
    uint16_t mac_word1 = read_eeprom(1);
    uint16_t mac_word2 = read_eeprom(2);

    mac_address_[0] = mac_word0 & 0xFF;
    mac_address_[1] = (mac_word0 >> 8) & 0xFF;
    mac_address_[2] = mac_word1 & 0xFF;
    mac_address_[3] = (mac_word1 >> 8) & 0xFF;
    mac_address_[4] = mac_word2 & 0xFF;
    mac_address_[5] = (mac_word2 >> 8) & 0xFF;
}

void E1000Driver::init_rx() {
    std::cout << "[E1000] Initializing receiver..." << std::endl;

    // Program the Receive Address
    uint32_t ral = (mac_address_[0]) |
                   (mac_address_[1] << 8) |
                   (mac_address_[2] << 16) |
                   (mac_address_[3] << 24);
    uint32_t rah = (mac_address_[4]) |
                   (mac_address_[5] << 8) |
                   (1U << 31);  // Address Valid bit

    write_reg(Registers::RA, ral);
    write_reg(Registers::RA + 4, rah);

    // Program RX descriptor ring base address
    write_reg(Registers::RDBAL, static_cast<uint32_t>(rx_descriptors_phys_ & 0xFFFFFFFF));
    write_reg(Registers::RDBAH, static_cast<uint32_t>(rx_descriptors_phys_ >> 32));

    // Set RX descriptor ring length
    write_reg(Registers::RDLEN, RX_DESC_COUNT * sizeof(RxDescriptor));

    // Initialize head and tail
    write_reg(Registers::RDH, 0);
    write_reg(Registers::RDT, RX_DESC_COUNT - 1);
    rx_tail_ = RX_DESC_COUNT - 1;

    // Set up receive control
    uint32_t rctl = RctlBits::EN |           // Enable receiver
                    RctlBits::SBP |           // Store bad packets
                    RctlBits::BAM |           // Broadcast accept mode
                    RctlBits::BSIZE_2048 |    // 2KB buffer size
                    RctlBits::SECRC;          // Strip ethernet CRC

    write_reg(Registers::RCTL, rctl);

    std::cout << "[E1000] Receiver initialized" << std::endl;
}

void E1000Driver::init_tx() {
    std::cout << "[E1000] Initializing transmitter..." << std::endl;

    // Program TX descriptor ring base address
    write_reg(Registers::TDBAL, static_cast<uint32_t>(tx_descriptors_phys_ & 0xFFFFFFFF));
    write_reg(Registers::TDBAH, static_cast<uint32_t>(tx_descriptors_phys_ >> 32));

    // Set TX descriptor ring length
    write_reg(Registers::TDLEN, TX_DESC_COUNT * sizeof(TxDescriptor));

    // Initialize head and tail
    write_reg(Registers::TDH, 0);
    write_reg(Registers::TDT, 0);
    tx_tail_ = 0;

    // Configure transmit control
    uint32_t tctl = TctlBits::EN |           // Enable transmitter
                    TctlBits::PSP |           // Pad short packets
                    (15 << TctlBits::CT_SHIFT) |  // Collision threshold
                    (64 << TctlBits::COLD_SHIFT); // Collision distance

    write_reg(Registers::TCTL, tctl);

    // Set up Inter-Packet Gap
    write_reg(Registers::TIPG, 0x00602008);

    std::cout << "[E1000] Transmitter initialized" << std::endl;
}

bool E1000Driver::send_packet(const uint8_t* data, uint16_t length) {
    if (!data || length == 0 || length > TX_BUFFER_SIZE) {
        return false;
    }

    if (!tx_descriptors_ || !tx_buffers_) {
        return false;
    }

    // Get current tail (next descriptor to use)
    uint32_t tail = tx_tail_;
    TxDescriptor& desc = tx_descriptors_[tail];

    // Check if descriptor is available (DD bit set)
    if (!(desc.status & TxStatusBits::DD)) {
        // Descriptor still in use
        return false;
    }

    // Copy packet data to buffer
    std::memcpy(tx_buffers_[tail], data, length);

    // Sync buffer for device
    xinim::mm::DMABuffer buffer;
    buffer.virtual_addr = tx_buffers_[tail];
    buffer.physical_addr = tx_buffers_phys_[tail];
    buffer.size = length;
    buffer.is_coherent = false;
    xinim::mm::DMAAllocator::sync_for_device(buffer);

    // Setup descriptor
    desc.buffer_addr = tx_buffers_phys_[tail];
    desc.length = length;
    desc.cso = 0;
    desc.cmd = TxCmdBits::EOP | TxCmdBits::IFCS | TxCmdBits::RS;
    desc.status = 0;  // Clear status (including DD bit)
    desc.css = 0;
    desc.special = 0;

    // Advance tail
    advance_tx_tail();

    return true;
}

bool E1000Driver::receive_packet(uint8_t* buffer, uint16_t& length) {
    if (!buffer || !rx_descriptors_ || !rx_buffers_) {
        return false;
    }

    // Check next descriptor after tail
    uint32_t next = (rx_tail_ + 1) % RX_DESC_COUNT;
    RxDescriptor& desc = rx_descriptors_[next];

    // Check if descriptor has a packet (DD bit set)
    if (!(desc.status & RxStatusBits::DD)) {
        // No packet available
        return false;
    }

    // Check for errors
    if (desc.errors != 0) {
        // Packet has errors, skip it
        std::cerr << "[E1000] RX packet error: 0x" << std::hex << (int)desc.errors << std::dec << std::endl;

        // Reset descriptor
        desc.status = 0;
        advance_rx_tail();
        return false;
    }

    // Sync buffer for CPU
    xinim::mm::DMABuffer dma_buf;
    dma_buf.virtual_addr = rx_buffers_[next];
    dma_buf.physical_addr = rx_buffers_phys_[next];
    dma_buf.size = desc.length;
    dma_buf.is_coherent = false;
    xinim::mm::DMAAllocator::sync_for_cpu(dma_buf);

    // Copy packet to user buffer
    length = desc.length;
    std::memcpy(buffer, rx_buffers_[next], length);

    // Reset descriptor for reuse
    desc.status = 0;
    desc.errors = 0;
    desc.length = 0;

    // Advance tail to make descriptor available again
    advance_rx_tail();

    return true;
}

bool E1000Driver::link_up() const {
    static constexpr uint32_t STATUS_LU = 0x02;  // Link Up bit
    uint32_t status = read_reg(Registers::STATUS);
    return (status & STATUS_LU) != 0;
}

void E1000Driver::get_mac_address(uint8_t mac[6]) const {
    std::memcpy(mac, mac_address_, 6);
}

void E1000Driver::set_promiscuous_mode(bool enable) {
    uint32_t rctl = read_reg(Registers::RCTL);
    
    if (enable) {
        rctl |= RctlBits::UPE | RctlBits::MPE;
    } else {
        rctl &= ~(RctlBits::UPE | RctlBits::MPE);
    }
    
    write_reg(Registers::RCTL, rctl);
}

void E1000Driver::handle_interrupt() {
    // Read and clear interrupt cause
    uint32_t icr = read_reg(Registers::ICR);

    if (icr == 0) {
        // Spurious interrupt
        return;
    }

    if (icr & InterruptBits::LSC) {
        // Link status change
        link_up_ = link_up();
        std::cout << "[E1000] Link status changed: "
                  << (link_up_ ? "UP" : "DOWN") << std::endl;
    }

    if (icr & InterruptBits::RXT0) {
        // Received packets available
        // Process all available packets
        uint8_t temp_buffer[RX_BUFFER_SIZE];
        uint16_t length;
        int packets_received = 0;

        while (receive_packet(temp_buffer, length)) {
            packets_received++;
            // TODO: Pass packet to network stack
            // For now, just count them
        }

        if (packets_received > 0) {
            std::cout << "[E1000] Received " << packets_received << " packet(s)" << std::endl;
        }
    }

    if (icr & InterruptBits::TXDW) {
        // Transmit descriptor written back
        // Hardware has completed transmission
        // Descriptors are now available for reuse
    }

    if (icr & InterruptBits::RXO) {
        // Receiver overrun
        std::cerr << "[E1000] Warning: Receiver overrun" << std::endl;
    }

    if (icr & InterruptBits::RXDMT0) {
        // Receive descriptor minimum threshold reached
        // Running low on RX descriptors
        std::cout << "[E1000] Warning: RX descriptor threshold reached" << std::endl;
    }
}

void E1000Driver::setup_rx_descriptors() {
    std::cout << "[E1000] Setting up RX descriptors..." << std::endl;

    // Allocate descriptor ring
    xinim::mm::DMABuffer desc_buffer = xinim::mm::DMAAllocator::allocate(
        sizeof(RxDescriptor) * RX_DESC_COUNT,
        xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
    );

    if (!desc_buffer.is_valid()) {
        std::cerr << "[E1000] Error: Failed to allocate RX descriptor ring" << std::endl;
        return;
    }

    rx_descriptors_ = static_cast<RxDescriptor*>(desc_buffer.virtual_addr);
    rx_descriptors_phys_ = desc_buffer.physical_addr;
    rx_desc_dma_buffer_ = desc_buffer.virtual_addr;

    std::cout << "[E1000] RX descriptors allocated at phys: 0x"
              << std::hex << rx_descriptors_phys_ << std::dec << std::endl;

    // Allocate buffer arrays
    rx_buffers_ = new uint8_t*[RX_DESC_COUNT];
    rx_buffers_phys_ = new uint64_t[RX_DESC_COUNT];

    // Allocate individual packet buffers
    for (size_t i = 0; i < RX_DESC_COUNT; ++i) {
        xinim::mm::DMABuffer buffer = xinim::mm::DMAAllocator::allocate(
            RX_BUFFER_SIZE,
            xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
        );

        if (!buffer.is_valid()) {
            std::cerr << "[E1000] Error: Failed to allocate RX buffer " << i << std::endl;
            return;
        }

        rx_buffers_[i] = static_cast<uint8_t*>(buffer.virtual_addr);
        rx_buffers_phys_[i] = buffer.physical_addr;

        // Initialize descriptor
        rx_descriptors_[i].buffer_addr = buffer.physical_addr;
        rx_descriptors_[i].length = 0;
        rx_descriptors_[i].checksum = 0;
        rx_descriptors_[i].status = 0;
        rx_descriptors_[i].errors = 0;
        rx_descriptors_[i].special = 0;
    }

    std::cout << "[E1000] RX descriptors setup complete (" << RX_DESC_COUNT << " descriptors)" << std::endl;
}

void E1000Driver::setup_tx_descriptors() {
    std::cout << "[E1000] Setting up TX descriptors..." << std::endl;

    // Allocate descriptor ring
    xinim::mm::DMABuffer desc_buffer = xinim::mm::DMAAllocator::allocate(
        sizeof(TxDescriptor) * TX_DESC_COUNT,
        xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
    );

    if (!desc_buffer.is_valid()) {
        std::cerr << "[E1000] Error: Failed to allocate TX descriptor ring" << std::endl;
        return;
    }

    tx_descriptors_ = static_cast<TxDescriptor*>(desc_buffer.virtual_addr);
    tx_descriptors_phys_ = desc_buffer.physical_addr;
    tx_desc_dma_buffer_ = desc_buffer.virtual_addr;

    std::cout << "[E1000] TX descriptors allocated at phys: 0x"
              << std::hex << tx_descriptors_phys_ << std::dec << std::endl;

    // Allocate buffer arrays
    tx_buffers_ = new uint8_t*[TX_DESC_COUNT];
    tx_buffers_phys_ = new uint64_t[TX_DESC_COUNT];

    // Allocate individual packet buffers
    for (size_t i = 0; i < TX_DESC_COUNT; ++i) {
        xinim::mm::DMABuffer buffer = xinim::mm::DMAAllocator::allocate(
            TX_BUFFER_SIZE,
            xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_4GB
        );

        if (!buffer.is_valid()) {
            std::cerr << "[E1000] Error: Failed to allocate TX buffer " << i << std::endl;
            return;
        }

        tx_buffers_[i] = static_cast<uint8_t*>(buffer.virtual_addr);
        tx_buffers_phys_[i] = buffer.physical_addr;

        // Initialize descriptor (mark as done initially)
        tx_descriptors_[i].buffer_addr = 0;
        tx_descriptors_[i].length = 0;
        tx_descriptors_[i].cso = 0;
        tx_descriptors_[i].cmd = 0;
        tx_descriptors_[i].status = TxStatusBits::DD;  // Descriptor Done
        tx_descriptors_[i].css = 0;
        tx_descriptors_[i].special = 0;
    }

    std::cout << "[E1000] TX descriptors setup complete (" << TX_DESC_COUNT << " descriptors)" << std::endl;
}

uint32_t E1000Driver::get_rx_tail() const {
    return read_reg(Registers::RDT);
}

uint32_t E1000Driver::get_tx_tail() const {
    return read_reg(Registers::TDT);
}

void E1000Driver::advance_rx_tail() {
    rx_tail_ = (rx_tail_ + 1) % RX_DESC_COUNT;
    write_reg(Registers::RDT, rx_tail_);
}

void E1000Driver::advance_tx_tail() {
    tx_tail_ = (tx_tail_ + 1) % TX_DESC_COUNT;
    write_reg(Registers::TDT, tx_tail_);
}

} // namespace xinim::drivers
