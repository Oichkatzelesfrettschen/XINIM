/**
 * @file blockdev.hpp
 * @brief Block Device Abstraction Layer
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Provides a unified interface for block storage devices (AHCI, NVMe, virtio-blk, etc.)
 * Supports partitions, caching, and async I/O
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace xinim::block {

/**
 * @brief Block device capabilities
 */
struct BlockDeviceCapabilities {
    bool supports_write;      // Read/write device
    bool supports_flush;      // Supports cache flush
    bool supports_trim;       // Supports TRIM/UNMAP
    bool supports_async;      // Supports async I/O
    bool removable;           // Removable media
    bool hotplug;             // Supports hotplug
};

/**
 * @brief Block device statistics
 */
struct BlockDeviceStats {
    uint64_t read_ops;        // Total read operations
    uint64_t write_ops;       // Total write operations
    uint64_t read_bytes;      // Total bytes read
    uint64_t write_bytes;     // Total bytes written
    uint64_t read_errors;     // Read errors
    uint64_t write_errors;    // Write errors
    uint64_t flush_ops;       // Cache flush operations
};

/**
 * @brief Block device type
 */
enum class BlockDeviceType {
    UNKNOWN,
    AHCI_SATA,
    NVME,
    VIRTIO_BLK,
    USB_STORAGE,
    RAMDISK,
    LOOPBACK,
};

/**
 * @brief Block device interface
 *
 * Abstract base class for all block devices. Drivers implement this interface
 * to expose their devices to the block layer.
 */
class BlockDevice {
public:
    virtual ~BlockDevice() = default;

    /**
     * @brief Get device name (e.g., "sda", "nvme0n1")
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Get device type
     */
    virtual BlockDeviceType get_type() const = 0;

    /**
     * @brief Get block size in bytes (typically 512 or 4096)
     */
    virtual size_t get_block_size() const = 0;

    /**
     * @brief Get total device size in blocks
     */
    virtual uint64_t get_block_count() const = 0;

    /**
     * @brief Get device capabilities
     */
    virtual BlockDeviceCapabilities get_capabilities() const = 0;

    /**
     * @brief Read blocks from device
     * @param lba Starting logical block address
     * @param count Number of blocks to read
     * @param buffer Output buffer (must be at least count * block_size bytes)
     * @return Number of blocks read, or -errno on error
     */
    virtual int read_blocks(uint64_t lba, uint32_t count, uint8_t* buffer) = 0;

    /**
     * @brief Write blocks to device
     * @param lba Starting logical block address
     * @param count Number of blocks to write
     * @param buffer Input buffer (must be at least count * block_size bytes)
     * @return Number of blocks written, or -errno on error
     */
    virtual int write_blocks(uint64_t lba, uint32_t count, const uint8_t* buffer) = 0;

    /**
     * @brief Flush device write cache
     * @return 0 on success, -errno on error
     */
    virtual int flush() = 0;

    /**
     * @brief Get device statistics
     */
    virtual BlockDeviceStats get_stats() const = 0;

    /**
     * @brief Reset device statistics
     */
    virtual void reset_stats() = 0;

    // Convenience methods
    uint64_t get_size_bytes() const {
        return get_block_count() * get_block_size();
    }

    uint64_t get_size_mb() const {
        return get_size_bytes() / (1024 * 1024);
    }

    uint64_t get_size_gb() const {
        return get_size_bytes() / (1024 * 1024 * 1024);
    }
};

/**
 * @brief Partition information
 */
struct Partition {
    uint64_t start_lba;       // Starting LBA
    uint64_t size_blocks;     // Size in blocks
    uint8_t type_guid[16];    // Partition type GUID (GPT) or type byte (MBR)
    uint8_t unique_guid[16];  // Unique partition GUID (GPT only)
    std::string name;         // Partition name (GPT) or label
    uint32_t flags;           // Partition flags
    bool bootable;            // Bootable flag
};

/**
 * @brief Partitioned block device
 *
 * Represents a partition on a physical block device.
 * Implements BlockDevice interface but operates on a subset of the parent device.
 */
class PartitionedBlockDevice : public BlockDevice {
public:
    PartitionedBlockDevice(std::shared_ptr<BlockDevice> parent, const Partition& partition);
    virtual ~PartitionedBlockDevice() = default;

    // BlockDevice interface
    std::string get_name() const override;
    BlockDeviceType get_type() const override { return parent_->get_type(); }
    size_t get_block_size() const override { return parent_->get_block_size(); }
    uint64_t get_block_count() const override { return partition_.size_blocks; }
    BlockDeviceCapabilities get_capabilities() const override;

    int read_blocks(uint64_t lba, uint32_t count, uint8_t* buffer) override;
    int write_blocks(uint64_t lba, uint32_t count, const uint8_t* buffer) override;
    int flush() override;

    BlockDeviceStats get_stats() const override;
    void reset_stats() override;

    // Partition-specific methods
    const Partition& get_partition_info() const { return partition_; }
    std::shared_ptr<BlockDevice> get_parent() const { return parent_; }

private:
    std::shared_ptr<BlockDevice> parent_;
    Partition partition_;
    mutable std::mutex mutex_;
    BlockDeviceStats stats_;
};

/**
 * @brief Block device manager
 *
 * Central registry for all block devices in the system.
 * Handles device enumeration, naming, and partition detection.
 */
class BlockDeviceManager {
public:
    static BlockDeviceManager& instance();

    /**
     * @brief Register a new block device
     * @param device Block device to register
     * @return Assigned device name, or empty string on error
     */
    std::string register_device(std::shared_ptr<BlockDevice> device);

    /**
     * @brief Unregister a block device
     * @param name Device name
     */
    void unregister_device(const std::string& name);

    /**
     * @brief Get block device by name
     * @param name Device name (e.g., "sda", "nvme0n1")
     * @return Block device pointer, or nullptr if not found
     */
    std::shared_ptr<BlockDevice> get_device(const std::string& name);

    /**
     * @brief Get all registered devices
     */
    std::vector<std::shared_ptr<BlockDevice>> get_all_devices();

    /**
     * @brief Scan device for partitions and register them
     * @param device Parent device to scan
     * @return Number of partitions found
     */
    int scan_partitions(std::shared_ptr<BlockDevice> device);

    /**
     * @brief Print device table to console
     */
    void print_device_table();

private:
    BlockDeviceManager() = default;
    ~BlockDeviceManager() = default;
    BlockDeviceManager(const BlockDeviceManager&) = delete;
    BlockDeviceManager& operator=(const BlockDeviceManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<BlockDevice>> devices_;
    std::mutex mutex_;
    uint32_t next_device_number_ = 0;
};

} // namespace xinim::block
