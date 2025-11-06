/**
 * @file virtio.hpp
 * @brief VirtIO Device Framework for Paravirtualization
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 * 
 * Based on VirtIO 1.0+ specification
 * Optimized for QEMU virtualization performance
 */

#pragma once

#include <cstdint>
#include <memory>

namespace xinim::drivers::virtio {

/**
 * @brief VirtIO Device Types
 */
enum class DeviceType : uint32_t {
    INVALID           = 0,
    NETWORK           = 1,   // virtio-net
    BLOCK             = 2,   // virtio-blk
    CONSOLE           = 3,   // virtio-console
    RNG               = 4,   // virtio-rng
    BALLOON           = 5,   // virtio-balloon
    SCSI              = 8,   // virtio-scsi
    GPU               = 16,  // virtio-gpu
    INPUT             = 18,  // virtio-input
};

/**
 * @brief VirtIO Device Status Bits
 */
struct DeviceStatus {
    static constexpr uint8_t ACKNOWLEDGE    = 1;     // Guest OS found device
    static constexpr uint8_t DRIVER         = 2;     // Guest OS knows how to drive device
    static constexpr uint8_t DRIVER_OK      = 4;     // Driver is set up and ready
    static constexpr uint8_t FEATURES_OK    = 8;     // Driver has acknowledged feature bits
    static constexpr uint8_t DEVICE_NEEDS_RESET = 64; // Device needs reset
    static constexpr uint8_t FAILED         = 128;   // Something went wrong
};

/**
 * @brief VirtIO Feature Bits (common to all devices)
 */
struct FeatureBits {
    static constexpr uint64_t NOTIFY_ON_EMPTY      = (1ULL << 24);
    static constexpr uint64_t ANY_LAYOUT           = (1ULL << 27);
    static constexpr uint64_t RING_INDIRECT_DESC   = (1ULL << 28);
    static constexpr uint64_t RING_EVENT_IDX       = (1ULL << 29);
    static constexpr uint64_t VERSION_1            = (1ULL << 32);
    static constexpr uint64_t ACCESS_PLATFORM      = (1ULL << 33);
    static constexpr uint64_t RING_PACKED          = (1ULL << 34);
    static constexpr uint64_t IN_ORDER             = (1ULL << 35);
    static constexpr uint64_t ORDER_PLATFORM       = (1ULL << 36);
    static constexpr uint64_t SR_IOV               = (1ULL << 37);
    static constexpr uint64_t NOTIFICATION_DATA    = (1ULL << 38);
};

/**
 * @brief VirtIO PCI Capability Types
 */
enum class PCICapType : uint8_t {
    COMMON_CFG        = 1,   // Common configuration
    NOTIFY_CFG        = 2,   // Notifications
    ISR_CFG           = 3,   // ISR status
    DEVICE_CFG        = 4,   // Device-specific configuration
    PCI_CFG           = 5,   // PCI configuration access
};

/**
 * @brief VirtIO Queue Descriptor
 */
struct [[gnu::packed]] VirtqDesc {
    uint64_t addr;       // Buffer physical address
    uint32_t len;        // Buffer length
    uint16_t flags;      // Descriptor flags
    uint16_t next;       // Next descriptor index (if flags & NEXT)
    
    // Flags
    static constexpr uint16_t NEXT     = 1;  // Buffer continues via next field
    static constexpr uint16_t WRITE    = 2;  // Buffer is write-only (device writes, driver reads)
    static constexpr uint16_t INDIRECT = 4;  // Buffer contains list of buffer descriptors
};

/**
 * @brief VirtIO Queue Available Ring
 */
struct [[gnu::packed]] VirtqAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];    // Variable size
    
    // After ring[]: uint16_t used_event (if VIRTIO_F_EVENT_IDX)
    
    static constexpr uint16_t NO_INTERRUPT = 1;
};

/**
 * @brief VirtIO Queue Used Element
 */
struct [[gnu::packed]] VirtqUsedElem {
    uint32_t id;    // Index of start of used descriptor chain
    uint32_t len;   // Total length written to descriptor chain
};

/**
 * @brief VirtIO Queue Used Ring
 */
struct [[gnu::packed]] VirtqUsed {
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[];  // Variable size
    
    // After ring[]: uint16_t avail_event (if VIRTIO_F_EVENT_IDX)
    
    static constexpr uint16_t NO_NOTIFY = 1;
};

/**
 * @brief VirtIO Queue (Virtqueue)
 */
class Virtqueue {
public:
    Virtqueue(uint16_t queue_size);
    ~Virtqueue();

    // Queue operations
    bool add_buffer(uint64_t addr, uint32_t len, bool write_only);
    bool add_buffer_chain(const uint64_t* addrs, const uint32_t* lens, 
                         const bool* write_flags, size_t count);
    void kick();  // Notify device
    
    bool get_used(uint32_t& id, uint32_t& len);
    bool has_used_buffers() const;
    
    // Configuration
    uint64_t get_desc_addr() const { return desc_phys_; }
    uint64_t get_avail_addr() const { return avail_phys_; }
    uint64_t get_used_addr() const { return used_phys_; }
    uint16_t get_size() const { return queue_size_; }
    
    void disable_interrupts();
    void enable_interrupts();

private:
    uint16_t queue_size_;
    uint16_t next_avail_;
    uint16_t last_used_;
    
    // Queue memory
    VirtqDesc* desc_;
    VirtqAvail* avail_;
    VirtqUsed* used_;
    
    uint64_t desc_phys_;
    uint64_t avail_phys_;
    uint64_t used_phys_;
    
    // Free descriptor tracking
    uint16_t* free_desc_;
    uint16_t num_free_;
    
    int16_t alloc_desc();
    void free_desc(uint16_t idx);
    void free_desc_chain(uint16_t idx);
};

/**
 * @brief VirtIO Device Base Class
 */
class VirtIODevice {
public:
    VirtIODevice() = default;
    virtual ~VirtIODevice() = default;

    // Device lifecycle
    virtual bool probe(uint16_t vendor_id, uint16_t device_id);
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Feature negotiation
    virtual uint64_t get_required_features() const = 0;
    virtual uint64_t get_optional_features() const { return 0; }
    bool negotiate_features();
    
    // Queue management
    bool setup_queue(uint16_t queue_idx, uint16_t queue_size);
    Virtqueue* get_queue(uint16_t queue_idx);
    
    // Interrupt handling
    virtual void handle_interrupt() = 0;
    bool check_isr();
    
    // Status management
    void set_status(uint8_t status);
    uint8_t get_status() const;
    void reset();
    void set_failed();

protected:
    // Hardware access (implemented by transport layer)
    virtual uint8_t read_device_status() const = 0;
    virtual void write_device_status(uint8_t status) = 0;
    virtual uint32_t read_device_features(uint32_t select) const = 0;
    virtual void write_driver_features(uint32_t select, uint32_t value) = 0;
    virtual uint16_t read_queue_size(uint16_t queue_idx) const = 0;
    virtual void select_queue(uint16_t queue_idx) = 0;
    virtual void setup_queue_addrs(uint16_t queue_idx, 
                                   uint64_t desc_addr,
                                   uint64_t avail_addr,
                                   uint64_t used_addr) = 0;
    virtual void notify_queue(uint16_t queue_idx) = 0;
    virtual uint8_t read_isr_status() const = 0;
    
    // Device configuration
    DeviceType device_type_;
    uint64_t features_host_;
    uint64_t features_negotiated_;
    
    static constexpr size_t MAX_QUEUES = 16;
    std::unique_ptr<Virtqueue> queues_[MAX_QUEUES];
    uint16_t num_queues_;
};

/**
 * @brief VirtIO PCI Transport Layer
 */
class VirtIOPCI : public VirtIODevice {
public:
    VirtIOPCI();
    ~VirtIOPCI() override;

    // PCI-specific initialization
    bool init_pci(uint16_t vendor_id, uint16_t device_id);
    
protected:
    // VirtIODevice interface implementation
    uint8_t read_device_status() const override;
    void write_device_status(uint8_t status) override;
    uint32_t read_device_features(uint32_t select) const override;
    void write_driver_features(uint32_t select, uint32_t value) override;
    uint16_t read_queue_size(uint16_t queue_idx) const override;
    void select_queue(uint16_t queue_idx) override;
    void setup_queue_addrs(uint16_t queue_idx, 
                          uint64_t desc_addr,
                          uint64_t avail_addr,
                          uint64_t used_addr) override;
    void notify_queue(uint16_t queue_idx) override;
    uint8_t read_isr_status() const override;

private:
    // PCI capability structures
    struct CommonCfg {
        uint32_t device_feature_select;
        uint32_t device_feature;
        uint32_t driver_feature_select;
        uint32_t driver_feature;
        uint16_t msix_config;
        uint16_t num_queues;
        uint8_t device_status;
        uint8_t config_generation;
        
        uint16_t queue_select;
        uint16_t queue_size;
        uint16_t queue_msix_vector;
        uint16_t queue_enable;
        uint16_t queue_notify_off;
        uint64_t queue_desc;
        uint64_t queue_avail;
        uint64_t queue_used;
    };

    // PCI resources
    volatile uint8_t* bar_;
    uint64_t bar_phys_;
    size_t bar_size_;
    
    // VirtIO capabilities (offsets in BAR)
    volatile CommonCfg* common_cfg_;
    volatile uint8_t* notify_base_;
    uint32_t notify_off_multiplier_;
    volatile uint8_t* isr_status_;
    volatile uint8_t* device_cfg_;
    
    bool map_capabilities();
    void* map_capability(uint8_t bar, uint32_t offset, uint32_t length);
};

/**
 * @brief VirtIO Network Device
 */
class VirtIONet : public VirtIOPCI {
public:
    VirtIONet();
    ~VirtIONet() override;

    // VirtIODevice interface
    bool initialize() override;
    void shutdown() override;
    uint64_t get_required_features() const override;
    uint64_t get_optional_features() const override;
    void handle_interrupt() override;

    // Network operations
    bool send_packet(const uint8_t* data, uint16_t length);
    bool receive_packet(uint8_t* buffer, uint16_t& length);
    void get_mac_address(uint8_t mac[6]) const;

private:
    // VirtIO-Net specific feature bits
    static constexpr uint64_t FEATURE_CSUM       = (1ULL << 0);
    static constexpr uint64_t FEATURE_GUEST_CSUM = (1ULL << 1);
    static constexpr uint64_t FEATURE_MTU        = (1ULL << 3);
    static constexpr uint64_t FEATURE_MAC        = (1ULL << 5);
    static constexpr uint64_t FEATURE_GSO        = (1ULL << 6);
    static constexpr uint64_t FEATURE_GUEST_TSO4 = (1ULL << 7);
    static constexpr uint64_t FEATURE_GUEST_TSO6 = (1ULL << 8);
    static constexpr uint64_t FEATURE_STATUS     = (1ULL << 16);
    static constexpr uint64_t FEATURE_CTRL_VQ    = (1ULL << 17);
    
    // Queues
    static constexpr uint16_t RX_QUEUE = 0;
    static constexpr uint16_t TX_QUEUE = 1;
    static constexpr uint16_t CTRL_QUEUE = 2;
    
    uint8_t mac_address_[6];
    void read_mac_from_config();
    void fill_rx_queue();
};

/**
 * @brief VirtIO Block Device
 */
class VirtIOBlock : public VirtIOPCI {
public:
    VirtIOBlock();
    ~VirtIOBlock() override;

    // VirtIODevice interface
    bool initialize() override;
    void shutdown() override;
    uint64_t get_required_features() const override;
    uint64_t get_optional_features() const override;
    void handle_interrupt() override;

    // Block operations
    bool read_sectors(uint64_t sector, uint32_t count, uint8_t* buffer);
    bool write_sectors(uint64_t sector, uint32_t count, const uint8_t* buffer);
    uint64_t get_capacity() const { return capacity_; }
    uint32_t get_block_size() const { return block_size_; }

private:
    // VirtIO-Block specific feature bits
    static constexpr uint64_t FEATURE_SIZE_MAX   = (1ULL << 1);
    static constexpr uint64_t FEATURE_SEG_MAX    = (1ULL << 2);
    static constexpr uint64_t FEATURE_GEOMETRY   = (1ULL << 4);
    static constexpr uint64_t FEATURE_RO         = (1ULL << 5);
    static constexpr uint64_t FEATURE_BLK_SIZE   = (1ULL << 6);
    static constexpr uint64_t FEATURE_FLUSH      = (1ULL << 9);
    static constexpr uint64_t FEATURE_TOPOLOGY   = (1ULL << 10);
    static constexpr uint64_t FEATURE_CONFIG_WCE = (1ULL << 11);
    
    uint64_t capacity_;
    uint32_t block_size_;
    
    void read_capacity_from_config();
};

} // namespace xinim::drivers::virtio
