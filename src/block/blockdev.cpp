/**
 * @file blockdev.cpp
 * @brief Block Device Abstraction Layer Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/block/blockdev.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>
#include <unordered_map>

// Error codes
#define EINVAL 22  // Invalid argument
#define EIO    5   // I/O error

namespace xinim::block {

// ============================================================================
// PartitionedBlockDevice Implementation
// ============================================================================

PartitionedBlockDevice::PartitionedBlockDevice(
    std::shared_ptr<BlockDevice> parent,
    const Partition& partition)
    : parent_(parent), partition_(partition) {

    memset(&stats_, 0, sizeof(stats_));

    LOG_INFO("Block: Created partition %s on %s (LBA %lu, size %lu blocks)",
             partition.name.c_str(), parent->get_name().c_str(),
             partition.start_lba, partition.size_blocks);
}

std::string PartitionedBlockDevice::get_name() const {
    // Generate partition name: parent + partition number
    // E.g., "sda" -> "sda1", "nvme0n1" -> "nvme0n1p1"
    return partition_.name;
}

BlockDeviceCapabilities PartitionedBlockDevice::get_capabilities() const {
    // Inherit capabilities from parent
    return parent_->get_capabilities();
}

int PartitionedBlockDevice::read_blocks(uint64_t lba, uint32_t count, uint8_t* buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate LBA range
    if (lba + count > partition_.size_blocks) {
        LOG_ERROR("Block: Read beyond partition boundary (%s: LBA %lu + %u > %lu)",
                  get_name().c_str(), lba, count, partition_.size_blocks);
        return -EINVAL;  // Invalid argument
    }

    // Translate to parent LBA
    uint64_t parent_lba = partition_.start_lba + lba;

    // Forward to parent device
    int result = parent_->read_blocks(parent_lba, count, buffer);

    if (result > 0) {
        stats_.read_ops++;
        stats_.read_bytes += result * get_block_size();
    } else if (result < 0) {
        stats_.read_errors++;
    }

    return result;
}

int PartitionedBlockDevice::write_blocks(uint64_t lba, uint32_t count, const uint8_t* buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate LBA range
    if (lba + count > partition_.size_blocks) {
        LOG_ERROR("Block: Write beyond partition boundary (%s: LBA %lu + %u > %lu)",
                  get_name().c_str(), lba, count, partition_.size_blocks);
        return -EINVAL;
    }

    // Translate to parent LBA
    uint64_t parent_lba = partition_.start_lba + lba;

    // Forward to parent device
    int result = parent_->write_blocks(parent_lba, count, buffer);

    if (result > 0) {
        stats_.write_ops++;
        stats_.write_bytes += result * get_block_size();
    } else if (result < 0) {
        stats_.write_errors++;
    }

    return result;
}

int PartitionedBlockDevice::flush() {
    // Forward to parent device
    int result = parent_->flush();
    if (result == 0) {
        stats_.flush_ops++;
    }
    return result;
}

BlockDeviceStats PartitionedBlockDevice::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void PartitionedBlockDevice::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    memset(&stats_, 0, sizeof(stats_));
}

// ============================================================================
// BlockDeviceManager Implementation
// ============================================================================

BlockDeviceManager& BlockDeviceManager::instance() {
    static BlockDeviceManager instance;
    return instance;
}

std::string BlockDeviceManager::register_device(std::shared_ptr<BlockDevice> device) {
    if (!device) {
        LOG_ERROR("Block: Attempted to register null device");
        return "";
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if device is already registered
    std::string dev_name = device->get_name();
    if (!dev_name.empty() && devices_.find(dev_name) != devices_.end()) {
        LOG_WARN("Block: Device %s already registered", dev_name.c_str());
        return dev_name;
    }

    // If device doesn't have a name, generate one
    if (dev_name.empty()) {
        // Generate name based on device type
        const char* prefix = "unk";
        switch (device->get_type()) {
            case BlockDeviceType::AHCI_SATA:
                prefix = "sd";
                break;
            case BlockDeviceType::NVME:
                prefix = "nvme";
                break;
            case BlockDeviceType::VIRTIO_BLK:
                prefix = "vd";
                break;
            case BlockDeviceType::RAMDISK:
                prefix = "ram";
                break;
            case BlockDeviceType::LOOPBACK:
                prefix = "loop";
                break;
            default:
                break;
        }

        // Find next available number
        char name_buf[32];
        for (uint32_t i = next_device_number_; i < next_device_number_ + 100; i++) {
            snprintf(name_buf, sizeof(name_buf), "%s%c", prefix, 'a' + (i % 26));
            if (devices_.find(name_buf) == devices_.end()) {
                dev_name = name_buf;
                next_device_number_ = i + 1;
                break;
            }
        }

        if (dev_name.empty()) {
            LOG_ERROR("Block: Failed to generate device name");
            return "";
        }
    }

    // Register device
    devices_[dev_name] = device;

    LOG_INFO("Block: Registered device %s (%s, %lu MB, %zu-byte blocks)",
             dev_name.c_str(),
             device->get_type() == BlockDeviceType::AHCI_SATA ? "SATA" :
             device->get_type() == BlockDeviceType::NVME ? "NVMe" :
             device->get_type() == BlockDeviceType::VIRTIO_BLK ? "VirtIO" : "Unknown",
             device->get_size_mb(),
             device->get_block_size());

    return dev_name;
}

void BlockDeviceManager::unregister_device(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it != devices_.end()) {
        LOG_INFO("Block: Unregistered device %s", name.c_str());
        devices_.erase(it);
    }
}

std::shared_ptr<BlockDevice> BlockDeviceManager::get_device(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it != devices_.end()) {
        return it->second;
    }

    return nullptr;
}

std::vector<std::shared_ptr<BlockDevice>> BlockDeviceManager::get_all_devices() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<BlockDevice>> result;
    result.reserve(devices_.size());

    for (const auto& [name, device] : devices_) {
        result.push_back(device);
    }

    return result;
}

int BlockDeviceManager::scan_partitions(std::shared_ptr<BlockDevice> device) {
    if (!device) {
        return -EINVAL;
    }

    LOG_INFO("Block: Scanning %s for partitions...", device->get_name().c_str());

    // Read MBR/GPT header (first sector)
    size_t block_size = device->get_block_size();
    uint8_t* sector = new uint8_t[block_size];

    int result = device->read_blocks(0, 1, sector);
    if (result <= 0) {
        LOG_ERROR("Block: Failed to read partition table from %s", device->get_name().c_str());
        delete[] sector;
        return result;
    }

    int partitions_found = 0;

    // Check for GPT signature
    if (block_size >= 512 && memcmp(sector + 510, "\x55\xAA", 2) == 0) {
        // MBR signature present - could be MBR or protective MBR for GPT
        // Check if it's a protective MBR (partition type 0xEE)
        bool is_protective_mbr = (sector[0x1BE + 4] == 0xEE);

        if (is_protective_mbr) {
            LOG_INFO("Block: Detected GPT partition table (protected by MBR)");
            // TODO: Parse GPT partitions
            // For now, just log detection
        } else {
            LOG_INFO("Block: Detected MBR partition table");
            // TODO: Parse MBR partitions
            // For now, just log detection
        }
    } else {
        LOG_INFO("Block: No partition table found on %s", device->get_name().c_str());
    }

    delete[] sector;
    return partitions_found;
}

void BlockDeviceManager::print_device_table() {
    std::lock_guard<std::mutex> lock(mutex_);

    printf("\n");
    printf("=== Block Devices ===\n");
    printf("\n");
    printf("%-10s %-12s %-12s %-10s %s\n",
           "Device", "Type", "Size", "BlockSize", "Capabilities");
    printf("%-10s %-12s %-12s %-10s %s\n",
           "------", "----", "----", "---------", "------------");

    for (const auto& [name, device] : devices_) {
        std::string type_str;
        switch (device->get_type()) {
            case BlockDeviceType::AHCI_SATA:
                type_str = "SATA";
                break;
            case BlockDeviceType::NVME:
                type_str = "NVMe";
                break;
            case BlockDeviceType::VIRTIO_BLK:
                type_str = "VirtIO";
                break;
            case BlockDeviceType::RAMDISK:
                type_str = "RAM";
                break;
            case BlockDeviceType::LOOPBACK:
                type_str = "Loop";
                break;
            default:
                type_str = "Unknown";
                break;
        }

        char size_str[32];
        uint64_t size_mb = device->get_size_mb();
        if (size_mb >= 1024) {
            snprintf(size_str, sizeof(size_str), "%.1f GB", size_mb / 1024.0);
        } else {
            snprintf(size_str, sizeof(size_str), "%lu MB", size_mb);
        }

        auto caps = device->get_capabilities();
        std::string caps_str;
        if (caps.supports_write) caps_str += "RW ";
        else caps_str += "RO ";
        if (caps.supports_flush) caps_str += "FLUSH ";
        if (caps.supports_trim) caps_str += "TRIM ";
        if (caps.removable) caps_str += "REMOV ";

        printf("%-10s %-12s %-12s %-10zu %s\n",
               name.c_str(),
               type_str.c_str(),
               size_str,
               device->get_block_size(),
               caps_str.c_str());
    }

    printf("\n");
}

} // namespace xinim::block
