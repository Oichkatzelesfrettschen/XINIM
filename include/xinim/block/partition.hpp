/**
 * @file partition.hpp
 * @brief Partition Table Parsing (GPT and MBR)
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 *
 * Supports both GPT (GUID Partition Table) and MBR (Master Boot Record)
 * partition schemes.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace xinim::block {

// Forward declaration
class BlockDevice;
struct Partition;

// ============================================================================
// MBR (Master Boot Record) Structures
// ============================================================================

/**
 * @brief MBR Partition Entry (16 bytes)
 */
struct __attribute__((packed)) MBRPartitionEntry {
    uint8_t  status;          // 0x80 = bootable, 0x00 = non-bootable
    uint8_t  first_chs[3];    // CHS address of first sector
    uint8_t  partition_type;  // Partition type
    uint8_t  last_chs[3];     // CHS address of last sector
    uint32_t first_lba;       // LBA of first sector
    uint32_t sector_count;    // Number of sectors
};

/**
 * @brief MBR (Master Boot Record) - Sector 0
 */
struct __attribute__((packed)) MBR {
    uint8_t  bootstrap_code[440];
    uint32_t disk_signature;
    uint16_t reserved;
    MBRPartitionEntry partitions[4];
    uint16_t boot_signature;  // 0xAA55
};

static_assert(sizeof(MBR) == 512, "MBR must be 512 bytes");

/**
 * @brief MBR Partition Types
 */
namespace MBRType {
    constexpr uint8_t EMPTY           = 0x00;
    constexpr uint8_t FAT12           = 0x01;
    constexpr uint8_t FAT16_SMALL     = 0x04;
    constexpr uint8_t EXTENDED        = 0x05;
    constexpr uint8_t FAT16           = 0x06;
    constexpr uint8_t NTFS            = 0x07;
    constexpr uint8_t FAT32           = 0x0B;
    constexpr uint8_t FAT32_LBA       = 0x0C;
    constexpr uint8_t FAT16_LBA       = 0x0E;
    constexpr uint8_t EXTENDED_LBA    = 0x0F;
    constexpr uint8_t LINUX_SWAP      = 0x82;
    constexpr uint8_t LINUX_NATIVE    = 0x83;
    constexpr uint8_t LINUX_EXTENDED  = 0x85;
    constexpr uint8_t LINUX_LVM       = 0x8E;
    constexpr uint8_t GPT_PROTECTIVE  = 0xEE;
    constexpr uint8_t EFI_SYSTEM      = 0xEF;
}

// ============================================================================
// GPT (GUID Partition Table) Structures
// ============================================================================

/**
 * @brief GPT Header (Sector 1, typically)
 */
struct __attribute__((packed)) GPTHeader {
    uint8_t  signature[8];       // "EFI PART"
    uint32_t revision;           // GPT version (usually 0x00010000)
    uint32_t header_size;        // Header size in bytes (usually 92)
    uint32_t header_crc32;       // CRC32 of header
    uint32_t reserved;           // Must be zero
    uint64_t current_lba;        // LBA of this header
    uint64_t backup_lba;         // LBA of backup header
    uint64_t first_usable_lba;   // First usable LBA for partitions
    uint64_t last_usable_lba;    // Last usable LBA for partitions
    uint8_t  disk_guid[16];      // Disk GUID
    uint64_t partition_entries_lba;  // LBA of partition array
    uint32_t num_partition_entries;  // Number of partition entries
    uint32_t partition_entry_size;   // Size of each partition entry
    uint32_t partition_array_crc32;  // CRC32 of partition array
    // Remaining bytes up to block size are reserved (usually zeros)
};

static_assert(sizeof(GPTHeader) == 92, "GPT Header must be 92 bytes");

/**
 * @brief GPT Partition Entry (usually 128 bytes)
 */
struct __attribute__((packed)) GPTPartitionEntry {
    uint8_t  partition_type_guid[16];   // Partition type GUID
    uint8_t  unique_partition_guid[16]; // Unique partition GUID
    uint64_t first_lba;                 // First LBA
    uint64_t last_lba;                  // Last LBA (inclusive)
    uint64_t attributes;                // Attribute flags
    uint16_t partition_name[36];        // Partition name (UTF-16LE)
};

static_assert(sizeof(GPTPartitionEntry) == 128, "GPT Partition Entry must be 128 bytes");

/**
 * @brief Common GPT Partition Type GUIDs
 */
namespace GPTType {
    // EFI System Partition
    constexpr uint8_t EFI_SYSTEM[16] = {
        0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
        0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
    };

    // Linux filesystem
    constexpr uint8_t LINUX_FILESYSTEM[16] = {
        0xaf, 0x3d, 0xc6, 0x0f, 0x83, 0x84, 0x72, 0x47,
        0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4
    };

    // Linux swap
    constexpr uint8_t LINUX_SWAP[16] = {
        0x82, 0x65, 0x81, 0x06, 0x36, 0x40, 0xdd, 0x41,
        0xbc, 0x13, 0x9f, 0x66, 0x4d, 0x21, 0xb5, 0x31
    };

    // Microsoft Basic Data
    constexpr uint8_t MICROSOFT_BASIC_DATA[16] = {
        0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
        0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7
    };
}

// ============================================================================
// Partition Table Parser
// ============================================================================

/**
 * @brief Partition table parser
 *
 * Detects and parses both MBR and GPT partition tables.
 */
class PartitionTableParser {
public:
    /**
     * @brief Parse partition table from block device
     * @param device Block device to scan
     * @param partitions Output vector of detected partitions
     * @return Number of partitions found, or -errno on error
     */
    static int parse(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions);

    /**
     * @brief Detect partition table type
     * @param device Block device to check
     * @return "GPT", "MBR", or "NONE"
     */
    static std::string detect_type(std::shared_ptr<BlockDevice> device);

private:
    /**
     * @brief Parse MBR partition table
     */
    static int parse_mbr(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions);

    /**
     * @brief Parse GPT partition table
     */
    static int parse_gpt(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions);

    /**
     * @brief Parse extended MBR partition
     */
    static int parse_extended_mbr(std::shared_ptr<BlockDevice> device,
                                   uint64_t extended_lba,
                                   std::vector<Partition>& partitions);

    /**
     * @brief Validate GPT header CRC
     */
    static bool validate_gpt_header(const GPTHeader* header);

    /**
     * @brief Calculate CRC32
     */
    static uint32_t crc32(const uint8_t* data, size_t length);

    /**
     * @brief Convert UTF-16LE to UTF-8
     */
    static std::string utf16le_to_utf8(const uint16_t* utf16, size_t max_len);

    /**
     * @brief Get partition name from MBR type
     */
    static std::string get_mbr_type_name(uint8_t type);

    /**
     * @brief Get partition name from GPT GUID
     */
    static std::string get_gpt_type_name(const uint8_t guid[16]);

    /**
     * @brief Compare GUIDs
     */
    static bool guid_equal(const uint8_t a[16], const uint8_t b[16]);
};

} // namespace xinim::block
