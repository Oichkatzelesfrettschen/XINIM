/**
 * @file ahci_blockdev.cpp
 * @brief AHCI BlockDevice Adapter Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/block/ahci_blockdev.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <cstdio>

// Error codes
#define EINVAL 22  // Invalid argument
#define EIO    5   // I/O error

namespace xinim::block {

AHCIBlockDevice::AHCIBlockDevice(
    std::shared_ptr<drivers::AHCIDriver> ahci_driver,
    uint8_t port_number)
    : ahci_driver_(ahci_driver),
      port_number_(port_number),
      block_size_(512),
      block_count_(0) {

    memset(&stats_, 0, sizeof(stats_));

    // Generate device name (sd + letter, e.g., sda, sdb, etc.)
    char name_buf[16];
    snprintf(name_buf, sizeof(name_buf), "sd%c", 'a' + port_number);
    name_ = name_buf;

    // Identify drive to get capacity
    if (!identify_drive()) {
        LOG_WARN("Block: Failed to identify AHCI drive on port %u", port_number);
    }
}

std::string AHCIBlockDevice::get_name() const {
    return name_;
}

BlockDeviceCapabilities AHCIBlockDevice::get_capabilities() const {
    BlockDeviceCapabilities caps = {};
    caps.supports_write = true;
    caps.supports_flush = true;
    caps.supports_trim = false;   // TODO: Support TRIM command
    caps.supports_async = false;  // TODO: Support async I/O
    caps.removable = false;
    caps.hotplug = true;          // AHCI supports hotplug
    return caps;
}

int AHCIBlockDevice::read_blocks(uint64_t lba, uint32_t count, uint8_t* buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!buffer) {
        return -EINVAL;
    }

    // Validate LBA range
    if (lba + count > block_count_) {
        LOG_ERROR("Block: Read beyond device boundary (%s: LBA %lu + %u > %lu)",
                  name_.c_str(), lba, count, block_count_);
        return -EINVAL;
    }

    // Read using AHCI driver
    bool success = ahci_driver_->read_sectors(port_number_, lba, count, buffer);

    if (success) {
        stats_.read_ops++;
        stats_.read_bytes += count * block_size_;
        return count;  // Return number of blocks read
    } else {
        stats_.read_errors++;
        LOG_ERROR("Block: Read failed on %s (LBA %lu, count %u)", name_.c_str(), lba, count);
        return -EIO;  // I/O error
    }
}

int AHCIBlockDevice::write_blocks(uint64_t lba, uint32_t count, const uint8_t* buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!buffer) {
        return -EINVAL;
    }

    // Validate LBA range
    if (lba + count > block_count_) {
        LOG_ERROR("Block: Write beyond device boundary (%s: LBA %lu + %u > %lu)",
                  name_.c_str(), lba, count, block_count_);
        return -EINVAL;
    }

    // Write using AHCI driver
    bool success = ahci_driver_->write_sectors(port_number_, lba, count, buffer);

    if (success) {
        stats_.write_ops++;
        stats_.write_bytes += count * block_size_;
        return count;  // Return number of blocks written
    } else {
        stats_.write_errors++;
        LOG_ERROR("Block: Write failed on %s (LBA %lu, count %u)", name_.c_str(), lba, count);
        return -EIO;
    }
}

int AHCIBlockDevice::flush() {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: Implement FLUSH CACHE command via AHCI
    // For now, just track statistics
    stats_.flush_ops++;

    return 0;
}

BlockDeviceStats AHCIBlockDevice::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void AHCIBlockDevice::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    memset(&stats_, 0, sizeof(stats_));
}

bool AHCIBlockDevice::identify_drive() {
    // TODO: Implement ATA IDENTIFY DEVICE command
    // For now, use default values

    // Typical SATA drive defaults
    block_size_ = 512;  // 512-byte sectors
    block_count_ = 0;   // Unknown capacity

    // Try to read a test sector to verify device is present
    uint8_t test_buffer[512];
    bool success = ahci_driver_->read_sectors(port_number_, 0, 1, test_buffer);

    if (success) {
        // Drive is present and readable
        // For now, assume a reasonable size (we need IDENTIFY DEVICE for real capacity)
        // Most emulators/VMs use standard sizes
        block_count_ = 20971520;  // 10 GB default (20971520 * 512 bytes)

        LOG_INFO("Block: Identified %s (%lu MB, %zu-byte sectors)",
                 name_.c_str(),
                 (block_count_ * block_size_) / (1024 * 1024),
                 block_size_);

        return true;
    } else {
        LOG_WARN("Block: No drive detected on AHCI port %u", port_number_);
        return false;
    }
}

} // namespace xinim::block
