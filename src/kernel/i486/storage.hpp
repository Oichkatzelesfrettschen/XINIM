#pragma once

#include <stdint.h>

namespace xinim::i486::storage {

using ReadSectorFn = bool (*)(uint32_t lba, uint8_t* buffer) noexcept;
using WriteSectorFn = bool (*)(uint32_t lba, const uint8_t* buffer) noexcept;

struct BlockDeviceHandle {
    bool present;
    uint32_t sector_size;
    uint32_t sector_count;
    char name[8];
    ReadSectorFn read_sector;
    WriteSectorFn write_sector;
};

struct PartitionHandle {
    bool present;
    uint8_t type;
    uint32_t start_lba;
    uint32_t sector_count;
};

void clear_boot_storage() noexcept;
void register_boot_storage(const BlockDeviceHandle& device,
                           const PartitionHandle& partition) noexcept;
const BlockDeviceHandle* boot_device() noexcept;
const PartitionHandle* boot_partition() noexcept;
bool read_boot_partition_sector(uint32_t relative_lba, uint8_t* buffer) noexcept;
bool write_boot_partition_sector(uint32_t relative_lba, const uint8_t* buffer) noexcept;

} // namespace xinim::i486::storage
