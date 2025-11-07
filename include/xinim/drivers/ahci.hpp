/**
 * @file ahci.hpp
 * @brief AHCI (Advanced Host Controller Interface) SATA Driver
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Based on:
 * - AHCI Specification 1.3.1
 * - OpenBSD/NetBSD AHCI implementations
 * - MINIX 3 AHCI driver
 */

#pragma once

#include <cstdint>
#include <memory>

namespace xinim::drivers {

/**
 * @brief AHCI Host Bus Adapter (HBA) Driver
 * 
 * Supports SATA drives through AHCI interface
 * Compatible with QEMU Q35 machine and modern hardware
 */
class AHCIDriver {
public:
    /**
     * @brief AHCI HBA Generic Host Control Registers
     */
    struct HBARegisters {
        static constexpr uint32_t CAP      = 0x00;  // Host Capabilities
        static constexpr uint32_t GHC      = 0x04;  // Global Host Control
        static constexpr uint32_t IS       = 0x08;  // Interrupt Status
        static constexpr uint32_t PI       = 0x0C;  // Ports Implemented
        static constexpr uint32_t VS       = 0x10;  // Version
        static constexpr uint32_t CCC_CTL  = 0x14;  // Command Completion Coalescing Control
        static constexpr uint32_t CCC_PORTS= 0x18;  // Command Completion Coalescing Ports
        static constexpr uint32_t EM_LOC   = 0x1C;  // Enclosure Management Location
        static constexpr uint32_t EM_CTL   = 0x20;  // Enclosure Management Control
        static constexpr uint32_t CAP2     = 0x24;  // Host Capabilities Extended
        static constexpr uint32_t BOHC     = 0x28;  // BIOS/OS Handoff Control and Status
    };

    /**
     * @brief Global Host Control Register Bits
     */
    struct GHCBits {
        static constexpr uint32_t HR       = (1 << 0);   // HBA Reset
        static constexpr uint32_t IE       = (1 << 1);   // Interrupt Enable
        static constexpr uint32_t MRSM     = (1 << 2);   // MSI Revert to Single Message
        static constexpr uint32_t AE       = (1 << 31);  // AHCI Enable
    };

    /**
     * @brief Host Capabilities Register Bits
     */
    struct CAPBits {
        static constexpr uint32_t NP_MASK  = 0x1F;       // Number of Ports (bits 4:0)
        static constexpr uint32_t SXS      = (1 << 5);   // Supports External SATA
        static constexpr uint32_t EMS      = (1 << 6);   // Enclosure Management Supported
        static constexpr uint32_t CCCS     = (1 << 7);   // Command Completion Coalescing Supported
        static constexpr uint32_t NCS_SHIFT = 8;         // Number of Command Slots (bits 12:8)
        static constexpr uint32_t NCS_MASK = 0x1F << NCS_SHIFT;
        static constexpr uint32_t PSC      = (1 << 13);  // Partial State Capable
        static constexpr uint32_t SSC      = (1 << 14);  // Slumber State Capable
        static constexpr uint32_t PMD      = (1 << 15);  // PIO Multiple DRQ Block
        static constexpr uint32_t FBSS     = (1 << 16);  // FIS-based Switching Supported
        static constexpr uint32_t SPM      = (1 << 17);  // Supports Port Multiplier
        static constexpr uint32_t SAM      = (1 << 18);  // Supports AHCI mode only
        static constexpr uint32_t SCLO     = (1 << 24);  // Supports Command List Override
        static constexpr uint32_t SAL      = (1 << 25);  // Supports Activity LED
        static constexpr uint32_t SALP     = (1 << 26);  // Supports Aggressive Link Power Management
        static constexpr uint32_t SSS      = (1 << 27);  // Supports Staggered Spin-up
        static constexpr uint32_t SMPS     = (1 << 28);  // Supports Mechanical Presence Switch
        static constexpr uint32_t SSNTF    = (1 << 29);  // Supports SNotification Register
        static constexpr uint32_t SNCQ     = (1 << 30);  // Supports Native Command Queuing
        static constexpr uint32_t S64A     = (1U << 31); // Supports 64-bit Addressing
    };

    /**
     * @brief Port Registers (per port, offset 0x100 + port * 0x80)
     */
    struct PortRegisters {
        static constexpr uint32_t CLB      = 0x00;  // Command List Base Address
        static constexpr uint32_t CLBU     = 0x04;  // Command List Base Address Upper
        static constexpr uint32_t FB       = 0x08;  // FIS Base Address
        static constexpr uint32_t FBU      = 0x0C;  // FIS Base Address Upper
        static constexpr uint32_t IS       = 0x10;  // Interrupt Status
        static constexpr uint32_t IE       = 0x14;  // Interrupt Enable
        static constexpr uint32_t CMD      = 0x18;  // Command and Status
        static constexpr uint32_t TFD      = 0x20;  // Task File Data
        static constexpr uint32_t SIG      = 0x24;  // Signature
        static constexpr uint32_t SSTS     = 0x28;  // Serial ATA Status
        static constexpr uint32_t SCTL     = 0x2C;  // Serial ATA Control
        static constexpr uint32_t SERR     = 0x30;  // Serial ATA Error
        static constexpr uint32_t SACT     = 0x34;  // Serial ATA Active
        static constexpr uint32_t CI       = 0x38;  // Command Issue
        static constexpr uint32_t SNTF     = 0x3C;  // SNotification
        static constexpr uint32_t FBS      = 0x40;  // FIS-based Switching Control
    };

    /**
     * @brief Port Command and Status Register Bits
     */
    struct PortCMDBits {
        static constexpr uint32_t ST       = (1 << 0);   // Start
        static constexpr uint32_t SUD      = (1 << 1);   // Spin-Up Device
        static constexpr uint32_t POD      = (1 << 2);   // Power On Device
        static constexpr uint32_t CLO      = (1 << 3);   // Command List Override
        static constexpr uint32_t FRE      = (1 << 4);   // FIS Receive Enable
        static constexpr uint32_t CCS_SHIFT = 8;         // Current Command Slot
        static constexpr uint32_t CCS_MASK = 0x1F << CCS_SHIFT;
        static constexpr uint32_t MPSS     = (1 << 13);  // Mechanical Presence Switch State
        static constexpr uint32_t FR       = (1 << 14);  // FIS Receive Running
        static constexpr uint32_t CR       = (1 << 15);  // Command List Running
        static constexpr uint32_t CPS      = (1 << 16);  // Cold Presence State
        static constexpr uint32_t PMA      = (1 << 17);  // Port Multiplier Attached
        static constexpr uint32_t HPCP     = (1 << 18);  // Hot Plug Capable Port
        static constexpr uint32_t MPSP     = (1 << 19);  // Mechanical Presence Switch Attached
        static constexpr uint32_t CPD      = (1 << 20);  // Cold Presence Detection
        static constexpr uint32_t ESP      = (1 << 21);  // External SATA Port
        static constexpr uint32_t FBSCP    = (1 << 22);  // FIS-based Switching Capable Port
        static constexpr uint32_t APSTE    = (1 << 23);  // Automatic Partial to Slumber Transitions Enabled
        static constexpr uint32_t ATAPI    = (1 << 24);  // Device is ATAPI
        static constexpr uint32_t DLAE     = (1 << 25);  // Drive LED on ATAPI Enable
        static constexpr uint32_t ALPE     = (1 << 26);  // Aggressive Link Power Management Enable
        static constexpr uint32_t ASP      = (1 << 27);  // Aggressive Slumber / Partial
        static constexpr uint32_t ICC_MASK = 0xF << 28;  // Interface Communication Control
        static constexpr uint32_t ICC_ACTIVE = 0x1 << 28;
        static constexpr uint32_t ICC_PARTIAL = 0x2 << 28;
        static constexpr uint32_t ICC_SLUMBER = 0x6 << 28;
    };

    /**
     * @brief Port Interrupt Status/Enable Bits
     */
    struct PortISBits {
        static constexpr uint32_t DHRS     = (1 << 0);   // Device to Host Register FIS Interrupt
        static constexpr uint32_t PSS      = (1 << 1);   // PIO Setup FIS Interrupt
        static constexpr uint32_t DSS      = (1 << 2);   // DMA Setup FIS Interrupt
        static constexpr uint32_t SDBS     = (1 << 3);   // Set Device Bits Interrupt
        static constexpr uint32_t UFS      = (1 << 4);   // Unknown FIS Interrupt
        static constexpr uint32_t DPS      = (1 << 5);   // Descriptor Processed
        static constexpr uint32_t PCS      = (1 << 6);   // Port Connect Change Status
        static constexpr uint32_t DMPS     = (1 << 7);   // Device Mechanical Presence Status
        static constexpr uint32_t PRCS     = (1 << 22);  // PhyRdy Change Status
        static constexpr uint32_t IPMS     = (1 << 23);  // Incorrect Port Multiplier Status
        static constexpr uint32_t OFS      = (1 << 24);  // Overflow Status
        static constexpr uint32_t INFS     = (1 << 26);  // Interface Non-fatal Error Status
        static constexpr uint32_t IFS      = (1 << 27);  // Interface Fatal Error Status
        static constexpr uint32_t HBDS     = (1 << 28);  // Host Bus Data Error Status
        static constexpr uint32_t HBFS     = (1 << 29);  // Host Bus Fatal Error Status
        static constexpr uint32_t TFES     = (1 << 30);  // Task File Error Status
        static constexpr uint32_t CPDS     = (1U << 31); // Cold Port Detect Status
    };

    /**
     * @brief FIS (Frame Information Structure) Types
     */
    enum class FISType : uint8_t {
        REG_H2D       = 0x27,  // Register FIS - host to device
        REG_D2H       = 0x34,  // Register FIS - device to host
        DMA_ACT       = 0x39,  // DMA activate FIS - device to host
        DMA_SETUP     = 0x41,  // DMA setup FIS - bidirectional
        DATA          = 0x46,  // Data FIS - bidirectional
        BIST          = 0x58,  // BIST activate FIS - bidirectional
        PIO_SETUP     = 0x5F,  // PIO setup FIS - device to host
        DEV_BITS      = 0xA1,  // Set device bits FIS - device to host
    };

    /**
     * @brief SATA Device Signatures
     */
    enum class DeviceSignature : uint32_t {
        ATA           = 0x00000101,  // SATA drive
        ATAPI         = 0xEB140101,  // ATAPI (CD-ROM, etc.)
        SEMB          = 0xC33C0101,  // Enclosure management bridge
        PM            = 0x96690101,  // Port multiplier
    };

    /**
     * @brief ATA Command Codes
     */
    struct ATACommand {
        static constexpr uint8_t READ_DMA_EXT  = 0x25;
        static constexpr uint8_t WRITE_DMA_EXT = 0x35;
        static constexpr uint8_t READ_DMA      = 0xC8;
        static constexpr uint8_t WRITE_DMA     = 0xCA;
        static constexpr uint8_t IDENTIFY      = 0xEC;
        static constexpr uint8_t IDENTIFY_PACKET = 0xA1;
        static constexpr uint8_t SET_FEATURES  = 0xEF;
        static constexpr uint8_t FLUSH_CACHE   = 0xE7;
        static constexpr uint8_t FLUSH_CACHE_EXT = 0xEA;
    };

    // Constructor and destructor
    AHCIDriver();
    ~AHCIDriver();

    // Controller initialization
    bool probe(uint16_t vendor_id, uint16_t device_id);
    bool initialize();
    void shutdown();

    // Port operations
    uint32_t get_port_count() const;
    bool is_port_implemented(uint8_t port) const;
    bool probe_port(uint8_t port);
    
    // Disk I/O operations
    bool read_sectors(uint8_t port, uint64_t lba, uint16_t count, uint8_t* buffer);
    bool write_sectors(uint8_t port, uint64_t lba, uint16_t count, const uint8_t* buffer);
    
    // Status and information
    DeviceSignature get_device_type(uint8_t port) const;
    bool get_drive_info(uint8_t port, uint64_t& sectors, uint32_t& sector_size);
    
    // Interrupt handling
    void handle_interrupt();

private:
    // Hardware access
    uint32_t read_hba_reg(uint32_t reg) const;
    void write_hba_reg(uint32_t reg, uint32_t value);
    uint32_t read_port_reg(uint8_t port, uint32_t reg) const;
    void write_port_reg(uint8_t port, uint32_t reg, uint32_t value);

    // Initialization helpers
    void reset_hba();
    void enable_ahci();
    bool wait_for_not_busy(uint8_t port, uint32_t timeout_ms);
    void start_command_engine(uint8_t port);
    void stop_command_engine(uint8_t port);
    
    // Port management
    void init_port(uint8_t port);
    void rebase_port(uint8_t port);
    int find_command_slot(uint8_t port);
    
    // Command execution
    bool execute_command(uint8_t port, uint8_t* fis, size_t fis_size, 
                        uint8_t* buffer, size_t buffer_size, bool write);

    // Member variables
    volatile uint8_t* abar_;       // AHCI Base Address Register (memory mapped)
    uint64_t abar_phys_;
    size_t abar_size_;
    
    uint32_t ports_implemented_;   // Bitmask of implemented ports
    uint32_t num_ports_;           // Number of ports
    uint32_t num_command_slots_;   // Number of command slots per port
    bool supports_64bit_;          // 64-bit addressing support
    
    // Per-port data structures
    struct PortData {
        volatile uint8_t* regs;
        void* command_list;
        void* received_fis;
        uint64_t command_list_phys;
        uint64_t received_fis_phys;
        DeviceSignature signature;
        bool active;
    };
    
    PortData* ports_;
    
    // Constants
    static constexpr size_t MAX_PORTS = 32;
    static constexpr size_t COMMAND_LIST_SIZE = 1024;
    static constexpr size_t RECEIVED_FIS_SIZE = 256;
    static constexpr uint32_t PORT_BASE = 0x100;
    static constexpr uint32_t PORT_SIZE = 0x80;
};

} // namespace xinim::drivers
