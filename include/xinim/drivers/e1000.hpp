/**
 * @file e1000.hpp
 * @brief Intel E1000 Gigabit Ethernet Driver for XINIM
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Based on specifications from:
 * - Intel 82540EP/EM Gigabit Ethernet Controller Datasheet
 * - OpenBSD em(4) driver
 * - MINIX 3 e1000 driver
 * - NetBSD wm(4) driver
 */

#pragma once

#include <cstdint>
#include <memory>
#include <xinim/hal/pci.hpp>

namespace xinim::drivers {

/**
 * @brief Intel E1000 Network Interface Card Driver
 * 
 * Supports Intel 82540, 82545, 82547, 82571, 82572, 82573, 82574, 82583 series
 * Optimized for QEMU emulation and physical hardware
 */
class E1000Driver {
public:
    /**
     * @brief E1000 PCI Device IDs
     */
    enum class DeviceID : uint16_t {
        E1000_82540EM      = 0x100E,  // QEMU default
        E1000_82545EM      = 0x100F,
        E1000_82546EB      = 0x1010,
        E1000_82545GM      = 0x1026,
        E1000_82566DM      = 0x1049,
        E1000_82571EB      = 0x105E,
        E1000_82572EI      = 0x107D,
        E1000_82573E       = 0x108B,
        E1000_82574L       = 0x10D3,
        E1000_82583V       = 0x150C,
    };

    /**
     * @brief E1000 Register Offsets
     */
    struct Registers {
        static constexpr uint32_t CTRL      = 0x00000;  // Device Control
        static constexpr uint32_t STATUS    = 0x00008;  // Device Status
        static constexpr uint32_t EECD      = 0x00010;  // EEPROM Control
        static constexpr uint32_t EERD      = 0x00014;  // EEPROM Read
        static constexpr uint32_t CTRL_EXT  = 0x00018;  // Extended Control
        static constexpr uint32_t MDIC      = 0x00020;  // MDI Control
        static constexpr uint32_t FCAL      = 0x00028;  // Flow Control Address Low
        static constexpr uint32_t FCAH      = 0x0002C;  // Flow Control Address High
        static constexpr uint32_t FCT       = 0x00030;  // Flow Control Type
        static constexpr uint32_t VET       = 0x00038;  // VLAN Ether Type
        static constexpr uint32_t ICR       = 0x000C0;  // Interrupt Cause Read
        static constexpr uint32_t ITR       = 0x000C4;  // Interrupt Throttling
        static constexpr uint32_t ICS       = 0x000C8;  // Interrupt Cause Set
        static constexpr uint32_t IMS       = 0x000D0;  // Interrupt Mask Set
        static constexpr uint32_t IMC       = 0x000D8;  // Interrupt Mask Clear
        static constexpr uint32_t RCTL      = 0x00100;  // Receive Control
        static constexpr uint32_t FCTTV     = 0x00170;  // Flow Control Transmit Timer
        static constexpr uint32_t TXCW      = 0x00178;  // Transmit Config Word
        static constexpr uint32_t RXCW      = 0x00180;  // Receive Config Word
        static constexpr uint32_t TCTL      = 0x00400;  // Transmit Control
        static constexpr uint32_t TIPG      = 0x00410;  // Transmit IPG
        static constexpr uint32_t LEDCTL    = 0x00E00;  // LED Control
        static constexpr uint32_t PBA       = 0x01000;  // Packet Buffer Allocation
        static constexpr uint32_t RDBAL     = 0x02800;  // RX Descriptor Base Low
        static constexpr uint32_t RDBAH     = 0x02804;  // RX Descriptor Base High
        static constexpr uint32_t RDLEN     = 0x02808;  // RX Descriptor Length
        static constexpr uint32_t RDH       = 0x02810;  // RX Descriptor Head
        static constexpr uint32_t RDT       = 0x02818;  // RX Descriptor Tail
        static constexpr uint32_t RDTR      = 0x02820;  // RX Delay Timer
        static constexpr uint32_t RXDCTL    = 0x02828;  // RX Descriptor Control
        static constexpr uint32_t RADV      = 0x0282C;  // RX Interrupt Absolute Delay
        static constexpr uint32_t RSRPD     = 0x02C00;  // RX Small Packet Detect
        static constexpr uint32_t TDBAL     = 0x03800;  // TX Descriptor Base Low
        static constexpr uint32_t TDBAH     = 0x03804;  // TX Descriptor Base High
        static constexpr uint32_t TDLEN     = 0x03808;  // TX Descriptor Length
        static constexpr uint32_t TDH       = 0x03810;  // TX Descriptor Head
        static constexpr uint32_t TDT       = 0x03818;  // TX Descriptor Tail
        static constexpr uint32_t TIDV      = 0x03820;  // TX Interrupt Delay Value
        static constexpr uint32_t TXDCTL    = 0x03828;  // TX Descriptor Control
        static constexpr uint32_t TADV      = 0x0382C;  // TX Absolute Interrupt Delay
        static constexpr uint32_t TSPMT     = 0x03830;  // TCP Segmentation PAD
        static constexpr uint32_t RA        = 0x05400;  // Receive Address
        static constexpr uint32_t MTA       = 0x05200;  // Multicast Table Array
    };

    /**
     * @brief Device Control Register Bits
     */
    struct CtrlBits {
        static constexpr uint32_t FD        = (1 << 0);   // Full Duplex
        static constexpr uint32_t GIO_MD    = (1 << 2);   // GIO Master Disable
        static constexpr uint32_t LRST      = (1 << 3);   // Link Reset
        static constexpr uint32_t ASDE      = (1 << 5);   // Auto Speed Detection Enable
        static constexpr uint32_t SLU       = (1 << 6);   // Set Link Up
        static constexpr uint32_t ILOS      = (1 << 7);   // Invert Loss of Signal
        static constexpr uint32_t SPEED_MASK = (3 << 8);  // Speed Select
        static constexpr uint32_t SPEED_10  = (0 << 8);
        static constexpr uint32_t SPEED_100 = (1 << 8);
        static constexpr uint32_t SPEED_1000 = (2 << 8);
        static constexpr uint32_t FRCSPD    = (1 << 11);  // Force Speed
        static constexpr uint32_t FRCDPLX   = (1 << 12);  // Force Duplex
        static constexpr uint32_t SDP0_DATA = (1 << 18);  // Software Definable Pin 0
        static constexpr uint32_t SDP1_DATA = (1 << 19);  // Software Definable Pin 1
        static constexpr uint32_t ADVD3WUC  = (1 << 20);  // Advertise D3 Wake Up
        static constexpr uint32_t EN_PHY_PWR_MGMT = (1 << 21);
        static constexpr uint32_t SDP0_IODIR = (1 << 22); // SDP0 Pin Direction
        static constexpr uint32_t SDP1_IODIR = (1 << 23); // SDP1 Pin Direction
        static constexpr uint32_t RST       = (1 << 26);  // Device Reset
        static constexpr uint32_t RFCE      = (1 << 27);  // RX Flow Control Enable
        static constexpr uint32_t TFCE      = (1 << 28);  // TX Flow Control Enable
        static constexpr uint32_t VME       = (1 << 30);  // VLAN Mode Enable
        static constexpr uint32_t PHY_RST   = (1 << 31);  // PHY Reset
    };

    /**
     * @brief Receive Control Register Bits
     */
    struct RctlBits {
        static constexpr uint32_t EN        = (1 << 1);   // Receiver Enable
        static constexpr uint32_t SBP       = (1 << 2);   // Store Bad Packets
        static constexpr uint32_t UPE       = (1 << 3);   // Unicast Promiscuous
        static constexpr uint32_t MPE       = (1 << 4);   // Multicast Promiscuous
        static constexpr uint32_t LPE       = (1 << 5);   // Long Packet Enable
        static constexpr uint32_t LBM_MASK  = (3 << 6);   // Loopback Mode
        static constexpr uint32_t LBM_NONE  = (0 << 6);
        static constexpr uint32_t RDMTS_MASK = (3 << 8);  // RX Desc Min Threshold
        static constexpr uint32_t RDMTS_HALF = (0 << 8);
        static constexpr uint32_t RDMTS_QUARTER = (1 << 8);
        static constexpr uint32_t RDMTS_EIGHTH = (2 << 8);
        static constexpr uint32_t MO_MASK   = (3 << 12);  // Multicast Offset
        static constexpr uint32_t BAM       = (1 << 15);  // Broadcast Accept Mode
        static constexpr uint32_t BSIZE_MASK = (3 << 16); // Buffer Size
        static constexpr uint32_t BSIZE_2048 = (0 << 16);
        static constexpr uint32_t BSIZE_1024 = (1 << 16);
        static constexpr uint32_t BSIZE_512  = (2 << 16);
        static constexpr uint32_t BSIZE_256  = (3 << 16);
        static constexpr uint32_t VFE       = (1 << 18);  // VLAN Filter Enable
        static constexpr uint32_t CFIEN     = (1 << 19);  // Canonical Form Indicator Enable
        static constexpr uint32_t CFI       = (1 << 20);  // Canonical Form Indicator
        static constexpr uint32_t DPF       = (1 << 22);  // Discard Pause Frames
        static constexpr uint32_t PMCF      = (1 << 23);  // Pass MAC Control Frames
        static constexpr uint32_t BSEX      = (1 << 25);  // Buffer Size Extension
        static constexpr uint32_t SECRC     = (1 << 26);  // Strip Ethernet CRC
    };

    /**
     * @brief Transmit Control Register Bits
     */
    struct TctlBits {
        static constexpr uint32_t EN        = (1 << 1);   // Transmit Enable
        static constexpr uint32_t PSP       = (1 << 3);   // Pad Short Packets
        static constexpr uint32_t CT_MASK   = (0xFF << 4); // Collision Threshold
        static constexpr uint32_t CT_SHIFT  = 4;
        static constexpr uint32_t COLD_MASK = (0x3FF << 12); // Collision Distance
        static constexpr uint32_t COLD_SHIFT = 12;
        static constexpr uint32_t SWXOFF    = (1 << 22);  // Software XOFF Transmission
        static constexpr uint32_t RTLC      = (1 << 24);  // Re-transmit on Late Collision
        static constexpr uint32_t NRTU      = (1 << 25);  // No Re-transmit Underrun
    };

    /**
     * @brief Interrupt Cause/Mask Bits
     */
    struct InterruptBits {
        static constexpr uint32_t TXDW      = (1 << 0);   // TX Descriptor Written Back
        static constexpr uint32_t TXQE      = (1 << 1);   // TX Queue Empty
        static constexpr uint32_t LSC       = (1 << 2);   // Link Status Change
        static constexpr uint32_t RXSEQ     = (1 << 3);   // RX Sequence Error
        static constexpr uint32_t RXDMT0    = (1 << 4);   // RX Desc Min Threshold
        static constexpr uint32_t RXO       = (1 << 6);   // RX Overrun
        static constexpr uint32_t RXT0      = (1 << 7);   // RX Timer Interrupt
        static constexpr uint32_t MDAC      = (1 << 9);   // MDI/O Access Complete
        static constexpr uint32_t RXCFG     = (1 << 10);  // RX Config
        static constexpr uint32_t GPI_EN0   = (1 << 11);  // General Purpose Int 0
        static constexpr uint32_t GPI_EN1   = (1 << 12);  // General Purpose Int 1
        static constexpr uint32_t GPI_EN2   = (1 << 13);  // General Purpose Int 2
        static constexpr uint32_t GPI_EN3   = (1 << 14);  // General Purpose Int 3
        static constexpr uint32_t TXD_LOW   = (1 << 15);  // TX Desc Low Threshold
        static constexpr uint32_t SRPD      = (1 << 16);  // Small RX Packet Detected
    };

    /**
     * @brief RX Descriptor Structure
     */
    struct [[gnu::packed]] RxDescriptor {
        uint64_t buffer_addr;  // Physical address of the RX buffer
        uint16_t length;       // Length of received packet
        uint16_t checksum;     // Packet checksum
        uint8_t  status;       // Descriptor status
        uint8_t  errors;       // Packet errors
        uint16_t special;      // VLAN tag or other special info
    };

    /**
     * @brief TX Descriptor Structure
     */
    struct [[gnu::packed]] TxDescriptor {
        uint64_t buffer_addr;  // Physical address of the TX buffer
        uint16_t length;       // Length of packet to transmit
        uint8_t  cso;          // Checksum offset
        uint8_t  cmd;          // Command byte
        uint8_t  status;       // Descriptor status
        uint8_t  css;          // Checksum start
        uint16_t special;      // VLAN tag or other special info
    };

    /**
     * @brief RX Descriptor Status Bits
     */
    struct RxStatusBits {
        static constexpr uint8_t DD   = (1 << 0);  // Descriptor Done
        static constexpr uint8_t EOP  = (1 << 1);  // End of Packet
        static constexpr uint8_t IXSM = (1 << 2);  // Ignore Checksum
        static constexpr uint8_t VP   = (1 << 3);  // Packet is 802.1Q
        static constexpr uint8_t TCPCS = (1 << 5); // TCP Checksum Calculated
        static constexpr uint8_t IPCS = (1 << 6);  // IP Checksum Calculated
        static constexpr uint8_t PIF  = (1 << 7);  // Passed In-exact Filter
    };

    /**
     * @brief TX Descriptor Command Bits
     */
    struct TxCmdBits {
        static constexpr uint8_t EOP  = (1 << 0);  // End of Packet
        static constexpr uint8_t IFCS = (1 << 1);  // Insert FCS
        static constexpr uint8_t IC   = (1 << 2);  // Insert Checksum
        static constexpr uint8_t RS   = (1 << 3);  // Report Status
        static constexpr uint8_t RPS  = (1 << 4);  // Report Packet Sent
        static constexpr uint8_t DEXT = (1 << 5);  // Extension
        static constexpr uint8_t VLE  = (1 << 6);  // VLAN Enable
        static constexpr uint8_t IDE  = (1 << 7);  // Interrupt Delay Enable
    };

    /**
     * @brief TX Descriptor Status Bits
     */
    struct TxStatusBits {
        static constexpr uint8_t DD   = (1 << 0);  // Descriptor Done
        static constexpr uint8_t EC   = (1 << 1);  // Excess Collisions
        static constexpr uint8_t LC   = (1 << 2);  // Late Collision
        static constexpr uint8_t TU   = (1 << 3);  // Transmit Underrun
    };

    // Constructor and destructor
    E1000Driver();
    ~E1000Driver();

    // Device initialization
    bool probe(uint16_t vendor_id, uint16_t device_id);
    bool initialize();
    void shutdown();

    // Network operations
    bool send_packet(const uint8_t* data, uint16_t length);
    bool receive_packet(uint8_t* buffer, uint16_t& length);
    
    // Status and configuration
    bool link_up() const;
    void get_mac_address(uint8_t mac[6]) const;
    void set_promiscuous_mode(bool enable);
    
    // Interrupt handling
    void handle_interrupt();

private:
    // Hardware access
    uint32_t read_reg(uint32_t reg) const;
    void write_reg(uint32_t reg, uint32_t value);
    void write_flush();

    // Initialization helpers
    bool detect_eeprom();
    uint16_t read_eeprom(uint8_t addr);
    void read_mac_from_eeprom();
    void reset_hardware();
    void init_rx();
    void init_tx();

    // Descriptor ring management
    void setup_rx_descriptors();
    void setup_tx_descriptors();
    uint32_t get_rx_tail() const;
    uint32_t get_tx_tail() const;
    void advance_rx_tail();
    void advance_tx_tail();

    // Member variables
    volatile uint8_t* mmio_base_;
    uint64_t mmio_phys_;
    size_t mmio_size_;
    
    uint8_t mac_address_[6];
    bool has_eeprom_;
    bool link_up_;

    // Descriptor rings
    static constexpr size_t RX_DESC_COUNT = 256;
    static constexpr size_t TX_DESC_COUNT = 256;
    static constexpr size_t RX_BUFFER_SIZE = 2048;
    static constexpr size_t TX_BUFFER_SIZE = 2048;

    RxDescriptor* rx_descriptors_;
    TxDescriptor* tx_descriptors_;
    uint64_t rx_descriptors_phys_;
    uint64_t tx_descriptors_phys_;

    uint8_t** rx_buffers_;
    uint8_t** tx_buffers_;
    uint64_t* rx_buffers_phys_;
    uint64_t* tx_buffers_phys_;

    uint32_t rx_tail_;
    uint32_t tx_tail_;
};

} // namespace xinim::drivers
