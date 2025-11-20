/**
 * @file partition.cpp
 * @brief Partition Table Parsing Implementation
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include <xinim/block/partition.hpp>
#include <xinim/block/blockdev.hpp>
#include <xinim/log.hpp>
#include <cstring>
#include <algorithm>

// Error codes
#define EINVAL 22  // Invalid argument
#define EIO    5   // I/O error

namespace xinim::block {

// CRC32 table for fast computation
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t PartitionTableParser::crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

std::string PartitionTableParser::detect_type(std::shared_ptr<BlockDevice> device) {
    if (!device) {
        return "NONE";
    }

    size_t block_size = device->get_block_size();
    uint8_t* sector = new uint8_t[block_size];

    // Read first sector (MBR)
    int result = device->read_blocks(0, 1, sector);
    if (result <= 0) {
        delete[] sector;
        return "NONE";
    }

    // Check for MBR boot signature
    if (block_size >= 512 && sector[510] == 0x55 && sector[511] == 0xAA) {
        // Check if it's a protective MBR (GPT indicator)
        const MBR* mbr = reinterpret_cast<const MBR*>(sector);
        if (mbr->partitions[0].partition_type == MBRType::GPT_PROTECTIVE) {
            delete[] sector;
            return "GPT";
        }
        delete[] sector;
        return "MBR";
    }

    delete[] sector;
    return "NONE";
}

int PartitionTableParser::parse(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions) {
    if (!device) {
        return -EINVAL;
    }

    std::string type = detect_type(device);
    LOG_INFO("Partition: Detected %s partition table on %s", type.c_str(), device->get_name().c_str());

    if (type == "GPT") {
        return parse_gpt(device, partitions);
    } else if (type == "MBR") {
        return parse_mbr(device, partitions);
    }

    return 0;  // No partitions found
}

int PartitionTableParser::parse_mbr(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions) {
    size_t block_size = device->get_block_size();
    uint8_t* sector = new uint8_t[block_size];

    // Read MBR
    int result = device->read_blocks(0, 1, sector);
    if (result <= 0) {
        delete[] sector;
        return -EIO;
    }

    const MBR* mbr = reinterpret_cast<const MBR*>(sector);
    int partition_count = 0;

    // Parse primary partitions
    for (int i = 0; i < 4; i++) {
        const MBRPartitionEntry& entry = mbr->partitions[i];

        if (entry.partition_type == MBRType::EMPTY) {
            continue;  // Empty partition
        }

        if (entry.partition_type == MBRType::EXTENDED ||
            entry.partition_type == MBRType::EXTENDED_LBA ||
            entry.partition_type == MBRType::LINUX_EXTENDED) {
            // Extended partition - parse separately
            LOG_INFO("Partition: Extended partition detected at index %d", i);
            int extended_count = parse_extended_mbr(device, entry.first_lba, partitions);
            partition_count += extended_count;
            continue;
        }

        // Create partition
        Partition part = {};
        part.start_lba = entry.first_lba;
        part.size_blocks = entry.sector_count;
        part.type_guid[0] = entry.partition_type;  // Store type byte in first byte of GUID
        part.bootable = (entry.status == 0x80);

        // Generate partition name
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "%sp%d", device->get_name().c_str(), i + 1);
        part.name = name_buf;

        partitions.push_back(part);
        partition_count++;

        LOG_INFO("Partition: %s - LBA %lu, size %lu blocks (%s)",
                 part.name.c_str(), part.start_lba, part.size_blocks,
                 get_mbr_type_name(entry.partition_type).c_str());
    }

    delete[] sector;
    return partition_count;
}

int PartitionTableParser::parse_extended_mbr(std::shared_ptr<BlockDevice> device,
                                              uint64_t extended_lba,
                                              std::vector<Partition>& partitions) {
    // Extended partitions use a linked list structure
    // TODO: Implement extended partition parsing
    // For now, just log and return 0
    LOG_INFO("Partition: Extended partition parsing not yet implemented");
    return 0;
}

int PartitionTableParser::parse_gpt(std::shared_ptr<BlockDevice> device, std::vector<Partition>& partitions) {
    size_t block_size = device->get_block_size();
    uint8_t* sector = new uint8_t[block_size];

    // Read GPT header (LBA 1)
    int result = device->read_blocks(1, 1, sector);
    if (result <= 0) {
        delete[] sector;
        return -EIO;
    }

    const GPTHeader* header = reinterpret_cast<const GPTHeader*>(sector);

    // Validate signature
    const char gpt_sig[] = "EFI PART";
    if (memcmp(header->signature, gpt_sig, 8) != 0) {
        LOG_ERROR("Partition: Invalid GPT signature");
        delete[] sector;
        return -EINVAL;
    }

    // Validate CRC (basic check)
    // TODO: Implement full CRC validation
    LOG_INFO("Partition: GPT header found - %u partition entries at LBA %lu",
             header->num_partition_entries, header->partition_entries_lba);

    // Read partition entries
    uint32_t entries_per_block = block_size / header->partition_entry_size;
    uint32_t blocks_needed = (header->num_partition_entries + entries_per_block - 1) / entries_per_block;

    uint8_t* entries_buffer = new uint8_t[blocks_needed * block_size];
    result = device->read_blocks(header->partition_entries_lba, blocks_needed, entries_buffer);
    if (result <= 0) {
        delete[] entries_buffer;
        delete[] sector;
        return -EIO;
    }

    int partition_count = 0;

    // Parse partition entries
    for (uint32_t i = 0; i < header->num_partition_entries; i++) {
        const GPTPartitionEntry* entry = reinterpret_cast<const GPTPartitionEntry*>(
            entries_buffer + (i * header->partition_entry_size));

        // Check if partition is used (type GUID is non-zero)
        bool is_used = false;
        for (int j = 0; j < 16; j++) {
            if (entry->partition_type_guid[j] != 0) {
                is_used = true;
                break;
            }
        }

        if (!is_used) {
            continue;
        }

        // Create partition
        Partition part = {};
        part.start_lba = entry->first_lba;
        part.size_blocks = (entry->last_lba - entry->first_lba) + 1;
        memcpy(part.type_guid, entry->partition_type_guid, 16);
        memcpy(part.unique_guid, entry->unique_partition_guid, 16);
        part.flags = entry->attributes;
        part.bootable = (entry->attributes & 0x4) != 0;  // Legacy BIOS bootable flag

        // Convert partition name from UTF-16LE to UTF-8
        part.name = utf16le_to_utf8(entry->partition_name, 36);

        // If no name, generate one
        if (part.name.empty()) {
            char name_buf[32];
            snprintf(name_buf, sizeof(name_buf), "%sp%d", device->get_name().c_str(), partition_count + 1);
            part.name = name_buf;
        }

        partitions.push_back(part);
        partition_count++;

        LOG_INFO("Partition: %s - LBA %lu, size %lu blocks (%s)",
                 part.name.c_str(), part.start_lba, part.size_blocks,
                 get_gpt_type_name(entry->partition_type_guid).c_str());
    }

    delete[] entries_buffer;
    delete[] sector;
    return partition_count;
}

std::string PartitionTableParser::utf16le_to_utf8(const uint16_t* utf16, size_t max_len) {
    std::string result;
    result.reserve(max_len);

    for (size_t i = 0; i < max_len && utf16[i] != 0; i++) {
        uint16_t ch = utf16[i];

        // Simple ASCII conversion (for basic Latin characters)
        // TODO: Implement full UTF-16 to UTF-8 conversion
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else {
            result += '?';  // Placeholder for non-ASCII
        }
    }

    return result;
}

std::string PartitionTableParser::get_mbr_type_name(uint8_t type) {
    switch (type) {
        case MBRType::FAT12:           return "FAT12";
        case MBRType::FAT16_SMALL:     return "FAT16 (small)";
        case MBRType::EXTENDED:        return "Extended";
        case MBRType::FAT16:           return "FAT16";
        case MBRType::NTFS:            return "NTFS/exFAT";
        case MBRType::FAT32:           return "FAT32";
        case MBRType::FAT32_LBA:       return "FAT32 (LBA)";
        case MBRType::LINUX_SWAP:      return "Linux swap";
        case MBRType::LINUX_NATIVE:    return "Linux";
        case MBRType::LINUX_LVM:       return "Linux LVM";
        case MBRType::EFI_SYSTEM:      return "EFI System";
        default:
            char buf[32];
            snprintf(buf, sizeof(buf), "Type 0x%02X", type);
            return std::string(buf);
    }
}

std::string PartitionTableParser::get_gpt_type_name(const uint8_t guid[16]) {
    if (guid_equal(guid, GPTType::EFI_SYSTEM)) {
        return "EFI System";
    } else if (guid_equal(guid, GPTType::LINUX_FILESYSTEM)) {
        return "Linux filesystem";
    } else if (guid_equal(guid, GPTType::LINUX_SWAP)) {
        return "Linux swap";
    } else if (guid_equal(guid, GPTType::MICROSOFT_BASIC_DATA)) {
        return "Microsoft Basic Data";
    } else {
        return "Unknown GUID";
    }
}

bool PartitionTableParser::guid_equal(const uint8_t a[16], const uint8_t b[16]) {
    return memcmp(a, b, 16) == 0;
}

} // namespace xinim::block
