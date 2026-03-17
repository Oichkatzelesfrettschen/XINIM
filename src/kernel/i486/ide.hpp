#pragma once

#include <stdint.h>

namespace xinim::i486::ide {

struct DeviceInfo {
    bool present;
    bool read_only;
    uint32_t sector_count;
    char model[41];
};

void initialize() noexcept;
bool primary_master_present() noexcept;
uint32_t primary_master_sector_count() noexcept;
const DeviceInfo& primary_master_info() noexcept;
bool read_primary_master_sector(uint32_t lba, uint8_t* buffer) noexcept;
bool write_primary_master_sector(uint32_t lba, const uint8_t* buffer) noexcept;

} // namespace xinim::i486::ide
