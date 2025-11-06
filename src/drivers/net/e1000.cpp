/**
 * @file e1000.cpp
 * @brief Intel E1000 Gigabit Ethernet Driver Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/drivers/e1000.hpp>
#include <cstring>
#include <iostream>

namespace xinim::drivers {

E1000Driver::E1000Driver()
    : mmio_base_(nullptr)
    , mmio_phys_(0)
    , mmio_size_(0)
    , has_eeprom_(false)
    , link_up_(false)
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

    // Initialize receive and transmit
    init_rx();
    init_tx();

    // Enable interrupts
    write_reg(Registers::IMS, 
              InterruptBits::RXT0 | 
              InterruptBits::LSC | 
              InterruptBits::RXDMT0 |
              InterruptBits::RXO);

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

    // TODO: Free allocated buffers and descriptors

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

    // TODO: Allocate descriptor ring and buffers
    // For now, just configure the registers

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

    // TODO: Allocate descriptor ring and buffers

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
    // TODO: Implement packet transmission
    (void)data;
    (void)length;
    return false;
}

bool E1000Driver::receive_packet(uint8_t* buffer, uint16_t& length) {
    // TODO: Implement packet reception
    (void)buffer;
    (void)length;
    return false;
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

    if (icr & InterruptBits::LSC) {
        // Link status change
        link_up_ = link_up();
        std::cout << "[E1000] Link status changed: " 
                  << (link_up_ ? "UP" : "DOWN") << std::endl;
    }

    if (icr & InterruptBits::RXT0) {
        // Received packets available
        // TODO: Process received packets
    }

    if (icr & InterruptBits::TXDW) {
        // Transmit descriptor written back
        // TODO: Handle TX completion
    }

    if (icr & InterruptBits::RXO) {
        // Receiver overrun
        std::cerr << "[E1000] Warning: Receiver overrun" << std::endl;
    }
}

void E1000Driver::setup_rx_descriptors() {
    // TODO: Implement descriptor setup with proper memory allocation
}

void E1000Driver::setup_tx_descriptors() {
    // TODO: Implement descriptor setup with proper memory allocation
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
