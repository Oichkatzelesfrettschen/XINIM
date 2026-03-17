#include "ide.hpp"

#include "console.hpp"
#include "storage.hpp"

namespace xinim::i486::ide {
namespace {

constexpr uint16_t kPrimaryIoBase = 0x1F0U;
constexpr uint16_t kPrimaryControlBase = 0x3F6U;

constexpr uint16_t kRegisterData = 0x00U;
constexpr uint16_t kRegisterSectorCount = 0x02U;
constexpr uint16_t kRegisterLbaLow = 0x03U;
constexpr uint16_t kRegisterLbaMid = 0x04U;
constexpr uint16_t kRegisterLbaHigh = 0x05U;
constexpr uint16_t kRegisterDriveHead = 0x06U;
constexpr uint16_t kRegisterStatusCommand = 0x07U;

constexpr uint8_t kDriveMaster = 0xE0U;
constexpr uint8_t kCommandIdentify = 0xECU;
constexpr uint8_t kCommandReadSectors = 0x20U;
constexpr uint8_t kCommandWriteSectors = 0x30U;

constexpr uint8_t kStatusErr = 0x01U;
constexpr uint8_t kStatusDrq = 0x08U;
constexpr uint8_t kStatusDfq = 0x20U;
constexpr uint8_t kStatusDrdy = 0x40U;
constexpr uint8_t kStatusBusy = 0x80U;

constexpr uint32_t kPollIterations = 100000U;
constexpr uint32_t kSectorSize = 512U;
constexpr uint32_t kIdentifyWordCount = 256U;
constexpr uint32_t kModelWordStart = 27U;
constexpr uint32_t kModelWordEnd = 46U;
constexpr uint32_t kLbaSectorCountLowWord = 60U;
constexpr uint32_t kLbaSectorCountHighWord = 61U;
constexpr uint16_t kMbrSignatureOffset = 510U;
constexpr uint16_t kPartitionEntryOffset = 446U;
constexpr uint8_t kLinuxPartitionType = 0x83U;
constexpr uint16_t kExt2SuperblockMagic = 0xEF53U;

DeviceInfo g_primary_master = {false, true, 0U, {}};
bool g_initialized = false;

struct __attribute__((packed)) MbrPartitionEntry {
    uint8_t status;
    uint8_t first_chs[3];
    uint8_t partition_type;
    uint8_t last_chs[3];
    uint32_t first_lba;
    uint32_t sector_count;
};

struct __attribute__((packed)) Ext2Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t reserved_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    int32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mount_time;
    uint32_t write_time;
    uint16_t mount_count;
    int16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t last_check;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
};

inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port) noexcept {
    uint8_t value = 0U;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline uint16_t inw(uint16_t port) noexcept {
    uint16_t value = 0U;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline void outw(uint16_t port, uint16_t value) noexcept {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void delay_400ns() noexcept {
    (void)inb(static_cast<uint16_t>(kPrimaryControlBase));
    (void)inb(static_cast<uint16_t>(kPrimaryControlBase));
    (void)inb(static_cast<uint16_t>(kPrimaryControlBase));
    (void)inb(static_cast<uint16_t>(kPrimaryControlBase));
}

void select_primary_master() noexcept {
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterDriveHead), kDriveMaster);
    delay_400ns();
}

bool wait_while_busy() noexcept {
    for (uint32_t attempt = 0U; attempt < kPollIterations; ++attempt) {
        const uint8_t status = inb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand));
        if ((status & kStatusBusy) == 0U) {
            return true;
        }
    }
    return false;
}

bool wait_for_data_request() noexcept {
    for (uint32_t attempt = 0U; attempt < kPollIterations; ++attempt) {
        const uint8_t status = inb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand));
        if ((status & kStatusBusy) != 0U) {
            continue;
        }
        if ((status & kStatusErr) != 0U || (status & kStatusDfq) != 0U) {
            return false;
        }
        if ((status & kStatusDrq) != 0U) {
            return true;
        }
        if ((status & kStatusDrdy) == 0U) {
            continue;
        }
    }
    return false;
}

void read_words(uint16_t* buffer, uint32_t word_count) noexcept {
    for (uint32_t index = 0U; index < word_count; ++index) {
        buffer[index] = inw(static_cast<uint16_t>(kPrimaryIoBase + kRegisterData));
    }
}

void clear_info(DeviceInfo& info) noexcept {
    info.present = false;
    info.read_only = true;
    info.sector_count = 0U;
    info.model[0] = '\0';
}

void write_key(const char* key) noexcept {
    console::write_string(key);
    console::write_string(": ");
}

void write_preview_char(uint8_t byte) noexcept {
    if (byte >= 32U && byte <= 126U) {
        console::write_char(static_cast<char>(byte));
        return;
    }
    console::write_char('.');
}

void trim_model(char* text) noexcept {
    int end = 39;
    while (end >= 0 && text[end] == ' ') {
        text[end] = '\0';
        --end;
    }
}

void parse_identify_model(const uint16_t* identify_words, char* out_model) noexcept {
    uint32_t out_index = 0U;
    for (uint32_t word = kModelWordStart; word <= kModelWordEnd; ++word) {
        const uint16_t value = identify_words[word];
        out_model[out_index++] = static_cast<char>((value >> 8U) & 0xFFU);
        out_model[out_index++] = static_cast<char>(value & 0xFFU);
    }
    out_model[40] = '\0';
    trim_model(out_model);
}

bool identify_primary_master(DeviceInfo& info) noexcept {
    clear_info(info);
    select_primary_master();

    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterSectorCount), 0U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaLow), 0U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaMid), 0U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaHigh), 0U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand), kCommandIdentify);

    const uint8_t initial_status =
        inb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand));
    if (initial_status == 0U) {
        return false;
    }

    if (!wait_while_busy()) {
        return false;
    }

    const uint8_t lba_mid = inb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaMid));
    const uint8_t lba_high = inb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaHigh));
    if (lba_mid != 0U || lba_high != 0U) {
        return false;
    }

    if (!wait_for_data_request()) {
        return false;
    }

    uint16_t identify_words[kIdentifyWordCount]{};
    read_words(identify_words, kIdentifyWordCount);

    info.present = true;
    info.read_only = false;
    info.sector_count =
        static_cast<uint32_t>(identify_words[kLbaSectorCountLowWord]) |
        (static_cast<uint32_t>(identify_words[kLbaSectorCountHighWord]) << 16U);
    parse_identify_model(identify_words, info.model);
    return true;
}

bool read_sector_internal(uint32_t lba, uint8_t* buffer) noexcept {
    if (buffer == nullptr || lba >= 0x10000000U) {
        return false;
    }

    select_primary_master();
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterSectorCount), 1U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaLow),
         static_cast<uint8_t>(lba & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaMid),
         static_cast<uint8_t>((lba >> 8U) & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaHigh),
         static_cast<uint8_t>((lba >> 16U) & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterDriveHead),
         static_cast<uint8_t>(kDriveMaster | ((lba >> 24U) & 0x0FU)));
    delay_400ns();
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand), kCommandReadSectors);

    if (!wait_while_busy() || !wait_for_data_request()) {
        return false;
    }

    for (uint32_t index = 0U; index < kSectorSize; index += 2U) {
        const uint16_t value = inw(static_cast<uint16_t>(kPrimaryIoBase + kRegisterData));
        buffer[index] = static_cast<uint8_t>(value & 0xFFU);
        buffer[index + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    }
    return true;
}

bool write_sector_internal(uint32_t lba, const uint8_t* buffer) noexcept {
    if (buffer == nullptr || lba >= 0x10000000U) {
        return false;
    }

    select_primary_master();
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterSectorCount), 1U);
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaLow),
         static_cast<uint8_t>(lba & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaMid),
         static_cast<uint8_t>((lba >> 8U) & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterLbaHigh),
         static_cast<uint8_t>((lba >> 16U) & 0xFFU));
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterDriveHead),
         static_cast<uint8_t>(kDriveMaster | ((lba >> 24U) & 0x0FU)));
    delay_400ns();
    outb(static_cast<uint16_t>(kPrimaryIoBase + kRegisterStatusCommand), kCommandWriteSectors);

    if (!wait_while_busy() || !wait_for_data_request()) {
        return false;
    }

    for (uint32_t index = 0U; index < kSectorSize; index += 2U) {
        const uint16_t value =
            static_cast<uint16_t>(static_cast<uint16_t>(buffer[index]) |
                                  static_cast<uint16_t>(static_cast<uint16_t>(buffer[index + 1U]) << 8U));
        outw(static_cast<uint16_t>(kPrimaryIoBase + kRegisterData), value);
    }

    delay_400ns();
    return wait_while_busy();
}

void log_sector_zero_preview() noexcept {
    uint8_t sector[kSectorSize]{};
    if (!read_sector_internal(0U, sector)) {
        write_key("ATA primary master sector0");
        console::write_string("read failed");
        console::newline();
        return;
    }

    write_key("ATA primary master sector0");
    for (uint32_t index = 0U; index < 8U; ++index) {
        write_preview_char(sector[index]);
    }
    console::write_string(" mbr=");
    if (sector[510] == 0x55U && sector[511] == 0xAAU) {
        console::write_string("yes");
    } else {
        console::write_string("no");
    }
    console::newline();
}

bool read_partition_entry(MbrPartitionEntry& entry) noexcept {
    uint8_t sector[kSectorSize]{};
    if (!read_sector_internal(0U, sector)) {
        return false;
    }
    if (sector[kMbrSignatureOffset] != 0x55U || sector[kMbrSignatureOffset + 1U] != 0xAAU) {
        return false;
    }
    const auto* partition =
        reinterpret_cast<const MbrPartitionEntry*>(sector + kPartitionEntryOffset);
    entry = *partition;
    return true;
}

void log_partition_probe() noexcept {
    MbrPartitionEntry entry{};
    if (!read_partition_entry(entry)) {
        write_key("ATA primary master partition1");
        console::write_string("unavailable");
        console::newline();
        return;
    }

    write_key("ATA primary master partition1");
    console::write_string("type=");
    console::write_hex32(static_cast<uint32_t>(entry.partition_type));
    console::write_string(" start=");
    console::write_dec32(entry.first_lba);
    console::write_string(" sectors=");
    console::write_dec32(entry.sector_count);
    console::newline();
}

void register_boot_partition() noexcept {
    storage::clear_boot_storage();

    if (!g_primary_master.present) {
        return;
    }

    storage::BlockDeviceHandle device = {
        true,
        kSectorSize,
        g_primary_master.sector_count,
        "hda",
        read_sector_internal,
        write_sector_internal,
    };

    MbrPartitionEntry entry{};
    if (!read_partition_entry(entry) || entry.partition_type == 0U ||
        entry.first_lba == 0U || entry.sector_count == 0U) {
        storage::register_boot_storage(device, {false, 0U, 0U, 0U});
        return;
    }

    storage::register_boot_storage(
        device,
        {
            true,
            entry.partition_type,
            entry.first_lba,
            entry.sector_count,
        });
}

void log_ext2_probe() noexcept {
    MbrPartitionEntry entry{};
    if (!read_partition_entry(entry) || entry.partition_type != kLinuxPartitionType ||
        entry.first_lba == 0U) {
        write_key("ATA primary master ext2");
        console::write_string("absent");
        console::newline();
        return;
    }

    uint8_t sector[kSectorSize]{};
    if (!read_sector_internal(entry.first_lba + 2U, sector)) {
        write_key("ATA primary master ext2");
        console::write_string("superblock read failed");
        console::newline();
        return;
    }

    const auto* superblock = reinterpret_cast<const Ext2Superblock*>(sector);
    if (superblock->magic != kExt2SuperblockMagic) {
        write_key("ATA primary master ext2");
        console::write_string("magic mismatch");
        console::newline();
        return;
    }

    const uint32_t block_size = 1024U << superblock->log_block_size;
    write_key("ATA primary master ext2");
    console::write_string("block_size=");
    console::write_dec32(block_size);
    console::write_string(" blocks=");
    console::write_dec32(superblock->blocks_count);
    console::write_string(" inodes=");
    console::write_dec32(superblock->inodes_count);
    console::newline();
}

} // namespace

void initialize() noexcept {
    if (g_initialized) {
        return;
    }
    g_initialized = true;

    write_key("ATA primary master");
    console::write_string("probing");
    console::newline();

    if (!identify_primary_master(g_primary_master)) {
        write_key("ATA primary master");
        console::write_string("absent");
        console::newline();
        return;
    }

    write_key("ATA primary master");
    console::write_string("present model=");
    console::write_string(g_primary_master.model[0] == '\0' ? "<unknown>" : g_primary_master.model);
    console::write_string(" sectors=");
    console::write_dec32(g_primary_master.sector_count);
    console::newline();

    log_sector_zero_preview();
    register_boot_partition();
    log_partition_probe();
    log_ext2_probe();
}

bool primary_master_present() noexcept {
    return g_primary_master.present;
}

uint32_t primary_master_sector_count() noexcept {
    return g_primary_master.sector_count;
}

const DeviceInfo& primary_master_info() noexcept {
    return g_primary_master;
}

bool read_primary_master_sector(uint32_t lba, uint8_t* buffer) noexcept {
    if (!g_primary_master.present) {
        return false;
    }
    return read_sector_internal(lba, buffer);
}

bool write_primary_master_sector(uint32_t lba, const uint8_t* buffer) noexcept {
    if (!g_primary_master.present || g_primary_master.read_only) {
        return false;
    }
    return write_sector_internal(lba, buffer);
}

} // namespace xinim::i486::ide
