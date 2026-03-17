#include "storage.hpp"

namespace xinim::i486::storage {
namespace {

BlockDeviceHandle g_boot_device = {false, 0U, 0U, {}, nullptr, nullptr};
PartitionHandle g_boot_partition = {false, 0U, 0U, 0U};

void copy_name(char* destination, const char* source, uint32_t capacity) noexcept {
    if (capacity == 0U) {
        return;
    }
    uint32_t index = 0U;
    for (; index + 1U < capacity && source[index] != '\0'; ++index) {
        destination[index] = source[index];
    }
    destination[index] = '\0';
}

} // namespace

void clear_boot_storage() noexcept {
    g_boot_device = {false, 0U, 0U, {}, nullptr, nullptr};
    g_boot_partition = {false, 0U, 0U, 0U};
}

void register_boot_storage(const BlockDeviceHandle& device,
                           const PartitionHandle& partition) noexcept {
    g_boot_device = device;
    copy_name(g_boot_device.name, device.name, sizeof(g_boot_device.name));
    g_boot_partition = partition;
}

const BlockDeviceHandle* boot_device() noexcept {
    return g_boot_device.present ? &g_boot_device : nullptr;
}

const PartitionHandle* boot_partition() noexcept {
    return g_boot_partition.present ? &g_boot_partition : nullptr;
}

bool read_boot_partition_sector(uint32_t relative_lba, uint8_t* buffer) noexcept {
    if (!g_boot_device.present || !g_boot_partition.present ||
        g_boot_device.read_sector == nullptr || buffer == nullptr) {
        return false;
    }
    if (relative_lba >= g_boot_partition.sector_count) {
        return false;
    }
    return g_boot_device.read_sector(g_boot_partition.start_lba + relative_lba, buffer);
}

bool write_boot_partition_sector(uint32_t relative_lba, const uint8_t* buffer) noexcept {
    if (!g_boot_device.present || !g_boot_partition.present ||
        g_boot_device.write_sector == nullptr || buffer == nullptr) {
        return false;
    }
    if (relative_lba >= g_boot_partition.sector_count) {
        return false;
    }
    return g_boot_device.write_sector(g_boot_partition.start_lba + relative_lba, buffer);
}

} // namespace xinim::i486::storage
