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

    // ========================================================================
    // Memory-Mapped Structures (AHCI 1.3.1 Specification)
    // All structures must be packed and properly aligned
    // ========================================================================

    /**
     * @brief FIS_REG_H2D: Register FIS - Host to Device
     * Used to send ATA commands from host to device
     */
    struct FIS_REG_H2D {
        uint8_t  fis_type;      // FISType::REG_H2D (0x27)
        uint8_t  pmport:4;      // Port multiplier
        uint8_t  rsv0:3;        // Reserved
        uint8_t  c:1;           // 1: Command, 0: Control
        uint8_t  command;       // ATA command register
        uint8_t  featurel;      // Feature register, 7:0

        uint8_t  lba0;          // LBA low register, 7:0
        uint8_t  lba1;          // LBA mid register, 15:8
        uint8_t  lba2;          // LBA high register, 23:16
        uint8_t  device;        // Device register

        uint8_t  lba3;          // LBA register, 31:24
        uint8_t  lba4;          // LBA register, 39:32
        uint8_t  lba5;          // LBA register, 47:40
        uint8_t  featureh;      // Feature register, 15:8

        uint8_t  countl;        // Count register, 7:0
        uint8_t  counth;        // Count register, 15:8
        uint8_t  icc;           // Isochronous command completion
        uint8_t  control;       // Control register

        uint8_t  rsv1[4];       // Reserved
    } __attribute__((packed));

    /**
     * @brief FIS_REG_D2H: Register FIS - Device to Host
     * Used to notify host of device status changes
     */
    struct FIS_REG_D2H {
        uint8_t  fis_type;      // FISType::REG_D2H (0x34)
        uint8_t  pmport:4;      // Port multiplier
        uint8_t  rsv0:2;        // Reserved
        uint8_t  i:1;           // Interrupt bit
        uint8_t  rsv1:1;        // Reserved
        uint8_t  status;        // Status register
        uint8_t  error;         // Error register

        uint8_t  lba0;          // LBA low register, 7:0
        uint8_t  lba1;          // LBA mid register, 15:8
        uint8_t  lba2;          // LBA high register, 23:16
        uint8_t  device;        // Device register

        uint8_t  lba3;          // LBA register, 31:24
        uint8_t  lba4;          // LBA register, 39:32
        uint8_t  lba5;          // LBA register, 47:40
        uint8_t  rsv2;          // Reserved

        uint8_t  countl;        // Count register, 7:0
        uint8_t  counth;        // Count register, 15:8
        uint8_t  rsv3[2];       // Reserved

        uint8_t  rsv4[4];       // Reserved
    } __attribute__((packed));

    /**
     * @brief FIS_DMA_SETUP: DMA Setup FIS - Bidirectional
     * Used to initiate DMA transfers
     */
    struct FIS_DMA_SETUP {
        uint8_t  fis_type;      // FISType::DMA_SETUP (0x41)
        uint8_t  pmport:4;      // Port multiplier
        uint8_t  rsv0:1;        // Reserved
        uint8_t  d:1;           // Data transfer direction, 1: device to host
        uint8_t  i:1;           // Interrupt bit
        uint8_t  a:1;           // Auto-activate
        uint8_t  rsv1[2];       // Reserved

        uint64_t dma_buffer_id; // DMA Buffer Identifier (from host)
        uint32_t rsv2;          // Reserved
        uint32_t dma_buffer_offset; // Byte offset into buffer
        uint32_t transfer_count;    // Number of bytes to transfer
        uint32_t rsv3;          // Reserved
    } __attribute__((packed));

    /**
     * @brief FIS_PIO_SETUP: PIO Setup FIS - Device to Host
     * Used for PIO data transfers
     */
    struct FIS_PIO_SETUP {
        uint8_t  fis_type;      // FISType::PIO_SETUP (0x5F)
        uint8_t  pmport:4;      // Port multiplier
        uint8_t  rsv0:1;        // Reserved
        uint8_t  d:1;           // Data transfer direction, 1: device to host
        uint8_t  i:1;           // Interrupt bit
        uint8_t  rsv1:1;        // Reserved
        uint8_t  status;        // Status register
        uint8_t  error;         // Error register

        uint8_t  lba0;          // LBA low register, 7:0
        uint8_t  lba1;          // LBA mid register, 15:8
        uint8_t  lba2;          // LBA high register, 23:16
        uint8_t  device;        // Device register

        uint8_t  lba3;          // LBA register, 31:24
        uint8_t  lba4;          // LBA register, 39:32
        uint8_t  lba5;          // LBA register, 47:40
        uint8_t  rsv2;          // Reserved

        uint8_t  countl;        // Count register, 7:0
        uint8_t  counth;        // Count register, 15:8
        uint8_t  rsv3;          // Reserved
        uint8_t  e_status;      // New value of status register

        uint16_t tc;            // Transfer count
        uint8_t  rsv4[2];       // Reserved
    } __attribute__((packed));

    /**
     * @brief HBAPRDTEntry: Physical Region Descriptor Table Entry
     * Describes a physical memory region for DMA transfer
     */
    struct HBAPRDTEntry {
        uint32_t dba;           // Data base address (lower 32 bits)
        uint32_t dbau;          // Data base address upper (upper 32 bits)
        uint32_t rsv0;          // Reserved
        uint32_t dbc:22;        // Byte count, 4M max (0-based, so 0 = 1 byte)
        uint32_t rsv1:9;        // Reserved
        uint32_t i:1;           // Interrupt on completion
    } __attribute__((packed));

    /**
     * @brief HBACommandTable: Command Table
     * Contains command FIS, ATAPI command, and PRDT
     */
    struct HBACommandTable {
        uint8_t  cfis[64];      // Command FIS
        uint8_t  acmd[16];      // ATAPI command (12 or 16 bytes)
        uint8_t  rsv[48];       // Reserved
        HBAPRDTEntry prdt_entry[1]; // Physical region descriptor table (variable length)
    } __attribute__((packed));

    /**
     * @brief HBACommandHeader: Command List Entry
     * Each command slot has one of these (32 bytes)
     */
    struct HBACommandHeader {
        uint8_t  cfl:5;         // Command FIS length in DWORDS, 2 ~ 16
        uint8_t  a:1;           // ATAPI
        uint8_t  w:1;           // Write, 1: H2D, 0: D2H
        uint8_t  p:1;           // Prefetchable
        uint8_t  r:1;           // Reset
        uint8_t  b:1;           // BIST
        uint8_t  c:1;           // Clear busy upon R_OK
        uint8_t  rsv0:1;        // Reserved
        uint8_t  pmp:4;         // Port multiplier port
        uint16_t prdtl;         // Physical region descriptor table length

        volatile uint32_t prdbc; // PRD byte count transferred
        uint32_t ctba;          // Command table descriptor base address (lower 32 bits)
        uint32_t ctbau;         // Command table descriptor base address (upper 32 bits)
        uint32_t rsv1[4];       // Reserved
    } __attribute__((packed));

    /**
     * @brief HBAFIS: Received FIS Structure
     * Contains all FIS types received from device (256 bytes)
     */
    struct HBAFIS {
        FIS_DMA_SETUP dsfis;    // DMA Setup FIS (0x00)
        uint8_t       pad0[4];  // Padding
        FIS_PIO_SETUP psfis;    // PIO Setup FIS (0x20)
        uint8_t       pad1[12]; // Padding
        FIS_REG_D2H   rfis;     // Register – Device to Host FIS (0x40)
        uint8_t       pad2[4];  // Padding
        uint8_t       sdbfis[8]; // Set Device Bit FIS (0x58)
        uint8_t       ufis[64]; // Unknown FIS (0x60)
        uint8_t       rsv[96];  // Reserved (0xA0)
    } __attribute__((packed));

    /**
     * @brief HBAPort: Port Registers (per port, 128 bytes)
     * Memory-mapped port control registers
     */
    struct HBAPort {
        volatile uint32_t clb;       // 0x00: Command list base address (lower 32 bits)
        volatile uint32_t clbu;      // 0x04: Command list base address (upper 32 bits)
        volatile uint32_t fb;        // 0x08: FIS base address (lower 32 bits)
        volatile uint32_t fbu;       // 0x0C: FIS base address (upper 32 bits)
        volatile uint32_t is;        // 0x10: Interrupt status
        volatile uint32_t ie;        // 0x14: Interrupt enable
        volatile uint32_t cmd;       // 0x18: Command and status
        volatile uint32_t rsv0;      // 0x1C: Reserved
        volatile uint32_t tfd;       // 0x20: Task file data
        volatile uint32_t sig;       // 0x24: Signature
        volatile uint32_t ssts;      // 0x28: SATA status (SCR0:SStatus)
        volatile uint32_t sctl;      // 0x2C: SATA control (SCR2:SControl)
        volatile uint32_t serr;      // 0x30: SATA error (SCR1:SError)
        volatile uint32_t sact;      // 0x34: SATA active (SCR3:SActive)
        volatile uint32_t ci;        // 0x38: Command issue
        volatile uint32_t sntf;      // 0x3C: SATA notification (SCR4:SNotification)
        volatile uint32_t fbs;       // 0x40: FIS-based switch control
        volatile uint32_t rsv1[11];  // 0x44 ~ 0x6F: Reserved
        volatile uint32_t vendor[4]; // 0x70 ~ 0x7F: Vendor specific
    } __attribute__((packed));

    /**
     * @brief HBAMemory: Generic Host Control Registers
     * Main AHCI HBA register set (256 bytes + 32 port register sets)
     */
    struct HBAMemory {
        // Generic Host Control Registers (0x00 - 0x2B)
        volatile uint32_t cap;       // 0x00: Host capabilities
        volatile uint32_t ghc;       // 0x04: Global host control
        volatile uint32_t is;        // 0x08: Interrupt status
        volatile uint32_t pi;        // 0x0C: Ports implemented
        volatile uint32_t vs;        // 0x10: Version
        volatile uint32_t ccc_ctl;   // 0x14: Command completion coalescing control
        volatile uint32_t ccc_ports; // 0x18: Command completion coalescing ports
        volatile uint32_t em_loc;    // 0x1C: Enclosure management location
        volatile uint32_t em_ctl;    // 0x20: Enclosure management control
        volatile uint32_t cap2;      // 0x24: Host capabilities extended
        volatile uint32_t bohc;      // 0x28: BIOS/OS handoff control and status

        // Reserved (0x2C - 0x9F)
        volatile uint32_t rsv[29];   // 0x2C ~ 0x9F

        // Vendor Specific (0xA0 - 0xFF)
        volatile uint32_t vendor[24]; // 0xA0 ~ 0xFF

        // Port Control Registers (0x100 - 0x10FF)
        HBAPort ports[32];           // 0x100 ~ 0x10FF: Port 0-31 registers
    } __attribute__((packed));

    // Structure size assertions to ensure proper layout
    static_assert(sizeof(FIS_REG_H2D) == 20, "FIS_REG_H2D must be 20 bytes");
    static_assert(sizeof(FIS_REG_D2H) == 20, "FIS_REG_D2H must be 20 bytes");
    static_assert(sizeof(FIS_DMA_SETUP) == 28, "FIS_DMA_SETUP must be 28 bytes");
    static_assert(sizeof(FIS_PIO_SETUP) == 20, "FIS_PIO_SETUP must be 20 bytes");
    static_assert(sizeof(HBAPRDTEntry) == 16, "HBAPRDTEntry must be 16 bytes");
    static_assert(sizeof(HBACommandHeader) == 32, "HBACommandHeader must be 32 bytes");
    static_assert(sizeof(HBAFIS) == 256, "HBAFIS must be 256 bytes");
    static_assert(sizeof(HBAPort) == 128, "HBAPort must be 128 bytes");
    static_assert(sizeof(HBAMemory) == 4352, "HBAMemory must be 4352 bytes (0x1100)");

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

    // Block device registration
    void register_block_device(uint8_t port);

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
