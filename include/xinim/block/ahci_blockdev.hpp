/**
 * @file ahci_blockdev.hpp
 * @brief AHCI BlockDevice Adapter
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Wraps AHCIDriver to expose SATA drives as BlockDevice objects
 */

#pragma once

#include <xinim/block/blockdev.hpp>
#include <xinim/drivers/ahci.hpp>
#include <memory>
#include <mutex>

namespace xinim::block {

/**
 * @brief AHCI-backed block device
 *
 * Adapts an AHCI SATA port to the BlockDevice interface.
 * Each AHCI port with an attached drive becomes a separate block device.
 */
class AHCIBlockDevice : public BlockDevice {
public:
    /**
     * @brief Construct AHCI block device
     * @param ahci_driver AHCI driver instance
     * @param port_number AHCI port number (0-31)
     */
    AHCIBlockDevice(std::shared_ptr<drivers::AHCIDriver> ahci_driver, uint8_t port_number);
    virtual ~AHCIBlockDevice() = default;

    // BlockDevice interface
    std::string get_name() const override;
    BlockDeviceType get_type() const override { return BlockDeviceType::AHCI_SATA; }
    size_t get_block_size() const override { return block_size_; }
    uint64_t get_block_count() const override { return block_count_; }
    BlockDeviceCapabilities get_capabilities() const override;

    int read_blocks(uint64_t lba, uint32_t count, uint8_t* buffer) override;
    int write_blocks(uint64_t lba, uint32_t count, const uint8_t* buffer) override;
    int flush() override;

    BlockDeviceStats get_stats() const override;
    void reset_stats() override;

    // AHCI-specific methods
    uint8_t get_port_number() const { return port_number_; }
    std::shared_ptr<drivers::AHCIDriver> get_ahci_driver() const { return ahci_driver_; }

private:
    /**
     * @brief Identify drive and query capacity
     * @return true on success
     */
    bool identify_drive();

    std::shared_ptr<drivers::AHCIDriver> ahci_driver_;
    uint8_t port_number_;
    std::string name_;

    // Drive info
    size_t block_size_;
    uint64_t block_count_;

    // Statistics
    mutable std::mutex mutex_;
    BlockDeviceStats stats_;
};

} // namespace xinim::block
