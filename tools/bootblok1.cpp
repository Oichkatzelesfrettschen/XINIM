#include "../include/defs.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

// Fallback filesystem implementation
namespace fs {
using path = std::string;
inline bool create_directories(const std::string &path) {
    return true; // Simplified for compatibility
}
} // namespace fs

// Simplified optional implementation
template <typename T> class optional {
  private:
    bool has_value_;
    T value_;

  public:
    optional() : has_value_(false) {}
    optional(T val) : has_value_(true), value_(val) {}

    explicit operator bool() const { return has_value_; }
    T operator*() const { return value_; }

    static optional nullopt() { return optional(); }
};

namespace minix::bootloader {

/**
 * @brief Boot sector configuration and validation
 *
 * This class encapsulates the boot sector binary and provides
 * validation, patching capabilities, and safe file operations.
 */
class BootSector {
  public:
    static constexpr std::size_t SECTOR_SIZE = 512;
    static constexpr std::size_t BOOT_SIGNATURE_OFFSET = 510;
    static constexpr std::array<u8_t, 2> BOOT_SIGNATURE = {0x55, 0xaa};

    // Offset constants for patchable fields
    static constexpr std::size_t KERNEL_LBA_OFFSET = 0xeb;   // Offset 235
    static constexpr std::size_t KERNEL_ENTRY_OFFSET = 0xf5; // Offset 245
    static constexpr std::size_t DRIVE_NUMBER_OFFSET = 0xfd; // Offset 253

  private:
    /**
     * @brief Pre-compiled boot sector binary
     *
     * This array contains the complete 512-byte boot sector extracted
     * from the original NASM assembly source. The bootloader performs:
     *
     * 1. Hardware initialization (CLI, segment setup)
     * 2. Disk parameter extraction and validation
     * 3. Protected mode transition with paging
     * 4. Long mode activation (64-bit)
     * 5. Kernel loading via BIOS INT 13h extensions
     * 6. Control transfer to loaded kernel
     *
     * Patchable fields:
     * - Bytes 235-238: Kernel LBA (32-bit little-endian)
     * - Bytes 245-252: Kernel entry point (64-bit little-endian)
     * - Byte 253: Boot drive number
     */
    static constexpr std::array<u8_t, SECTOR_SIZE> boot_sector_data = {
        0xfa, 0x31, 0xc0, 0x8e, 0xd8, 0x8e, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0xa1, 0xeb, 0x7c,
        0xa3, 0x02, 0x7d, 0x66, 0xa1, 0xf5, 0x7c, 0x66, 0xa3, 0x08, 0x7d, 0x66, 0xa1, 0xf9, 0x7c,
        0x66, 0xa3, 0x0c, 0x7d, 0x8a, 0x16, 0xfd, 0x7c, 0xbe, 0x00, 0x7d, 0xb4, 0x42, 0xcd, 0x13,
        0x0f, 0x82, 0xb9, 0x00, 0xe4, 0x92, 0x0c, 0x02, 0xe6, 0x92, 0x0f, 0x01, 0x16, 0x58, 0x7d,
        0x0f, 0x20, 0xc0, 0x66, 0x83, 0xc8, 0x01, 0x0f, 0x22, 0xc0, 0xea, 0x4b, 0x7c, 0x08, 0x00,
        0x66, 0xb8, 0x10, 0x00, 0x8e, 0xd8, 0x8e, 0xc0, 0x8e, 0xe0, 0x8e, 0xe8, 0x8e, 0xd0, 0xbc,
        0x00, 0x00, 0x09, 0x00, 0xb8, 0x18, 0x7d, 0x00, 0x00, 0x83, 0xc8, 0x03, 0xa3, 0x10, 0x7d,
        0x00, 0x00, 0xc7, 0x05, 0x14, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x20, 0x7d,
        0x00, 0x00, 0x83, 0xc8, 0x03, 0xa3, 0x18, 0x7d, 0x00, 0x00, 0xc7, 0x05, 0x10, 0x7d, 0x00,
        0x00, 0x1b, 0x7d, 0x00, 0x00, 0xc7, 0x05, 0x14, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xc7, 0x05, 0x18, 0x7d, 0x00, 0x00, 0x23, 0x7d, 0x00, 0x00, 0xb8, 0x10, 0x7d, 0x00, 0x00,
        0x0f, 0x22, 0xd8, 0x0f, 0x20, 0xe0, 0x83, 0xc8, 0x20, 0x0f, 0x22, 0xe0, 0xb9, 0x80, 0x00,
        0x00, 0xc0, 0x0f, 0x32, 0x0d, 0x00, 0x01, 0x00, 0x00, 0x0f, 0x30, 0x0f, 0x20, 0xc0, 0x0d,
        0x01, 0x00, 0x00, 0x80, 0x0f, 0x22, 0xc0, 0xea, 0xd1, 0x7c, 0x00, 0x00, 0x18, 0x00, 0x66,
        0xb8, 0x20, 0x00, 0x8e, 0xd8, 0x8e, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x00, 0x09, 0x00, 0x48,
        0x8b, 0x04, 0x25, 0xed, 0x7c, 0x00, 0x00, 0xff, 0xe0, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90,
        0x90, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00,
        0x00, 0x00, 0x9a, 0xcf, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xcf, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x9a, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0xa0, 0x00, 0x27,
        0x00, 0x30, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x55, 0xaa,
    };

    std::array<u8_t, SECTOR_SIZE> sector_buffer;

  public:
    /**
     * @brief Construct a new BootSector object
     * @throws std::runtime_error if boot sector validation fails
     */
    BootSector() : sector_buffer(boot_sector_data) { validate(); }

    /**
     * @brief Validate boot sector integrity
     * @throws std::runtime_error if validation fails
     */
    void validate() const {
        // Verify boot signature
        if (sector_buffer[BOOT_SIGNATURE_OFFSET] != BOOT_SIGNATURE[0] ||
            sector_buffer[BOOT_SIGNATURE_OFFSET + 1] != BOOT_SIGNATURE[1]) {
            throw std::runtime_error("Invalid boot sector signature");
        }

        // Verify sector size
        static_assert(boot_sector_data.size() == SECTOR_SIZE,
                      "Boot sector must be exactly 512 bytes");
    }

    /**
     * @brief Patch kernel LBA in boot sector
     * @param lba 32-bit LBA address of kernel on disk
     */
    void patch_kernel_lba(std::uint32_t lba) noexcept {
        sector_buffer[KERNEL_LBA_OFFSET] = static_cast<u8_t>(lba & 0xff);
        sector_buffer[KERNEL_LBA_OFFSET + 1] = static_cast<u8_t>((lba >> 8) & 0xff);
        sector_buffer[KERNEL_LBA_OFFSET + 2] = static_cast<u8_t>((lba >> 16) & 0xff);
        sector_buffer[KERNEL_LBA_OFFSET + 3] = static_cast<u8_t>((lba >> 24) & 0xff);
    }

    /**
     * @brief Patch kernel entry point in boot sector
     * @param entry_point 64-bit virtual address of kernel entry point
     */
    void patch_kernel_entry(std::uint64_t entry_point) noexcept {
        for (std::size_t i = 0; i < 8; ++i) {
            sector_buffer[KERNEL_ENTRY_OFFSET + i] =
                static_cast<u8_t>((entry_point >> (i * 8)) & 0xff);
        }
    }

    /**
     * @brief Patch boot drive number
     * @param drive_number BIOS drive number (0x80 for first hard disk)
     */
    void patch_drive_number(u8_t drive_number) noexcept {
        sector_buffer[DRIVE_NUMBER_OFFSET] = drive_number;
    }
    /**
     * @brief Write boot sector to file
     * @param file_path Output file path
     * @throws std::system_error on I/O errors
     */
    void write_to_file(const fs::path &file_path) const {
        // Open file with appropriate flags
        std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                    "Failed to open output file: " + file_path);
        }

        // Write sector data
        file.write(reinterpret_cast<const char *>(sector_buffer.data()),
                   static_cast<std::streamsize>(sector_buffer.size()));

        if (!file.good()) {
            throw std::system_error(std::make_error_code(std::errc::io_error),
                                    "Failed to write boot sector data");
        }

        file.close();
        if (file.fail()) {
            throw std::system_error(std::make_error_code(std::errc::io_error),
                                    "Failed to close output file properly");
        }
    }

    /**
     * @brief Generate hexdump representation for debugging
     * @return String containing formatted hexdump
     */
    [[nodiscard]] std::string hexdump() const {
        std::string result;
        result.reserve(SECTOR_SIZE * 4); // Rough size estimate

        for (std::size_t i = 0; i < sector_buffer.size(); i += 16) {
            // Address offset
            result += std::to_string(i);
            result += ": ";

            // Hex bytes
            for (std::size_t j = 0; j < 16 && (i + j) < sector_buffer.size(); ++j) {
                if (j == 8)
                    result += " ";

                const auto byte = sector_buffer[i + j];
                result += "0123456789abcdef"[byte >> 4];
                result += "0123456789abcdef"[byte & 0xf];
                result += " ";
            }

            result += "\n";
        }

        return result;
    }
};

/**
 * @brief Boot sector extraction utility
 *
 * Command-line tool for extracting the pre-compiled boot sector
 * to a binary file for use in disk image creation or testing.
 */
class BootSectorExtractor {
  public:
    /**
     * @brief Extract boot sector to specified file
     */
    static void extract(const std::string &output_path = "bootblok",
                        optional<std::uint32_t> kernel_lba = optional<std::uint32_t>::nullopt(),
                        optional<std::uint64_t> kernel_entry = optional<std::uint64_t>::nullopt(),
                        optional<u8_t> drive_num = optional<u8_t>::nullopt()) {

        try {
            BootSector boot_sector;

            // Apply patches if provided
            if (kernel_lba) {
                boot_sector.patch_kernel_lba(*kernel_lba);
            }
            if (kernel_entry) {
                boot_sector.patch_kernel_entry(*kernel_entry);
            }
            if (drive_num) {
                boot_sector.patch_drive_number(*drive_num);
            }

            // Write to file
            boot_sector.write_to_file(output_path);

            std::cout << "Boot sector extracted to: " << output_path << '\n';
            std::cout << "Size: " << BootSector::SECTOR_SIZE << " bytes\n";

        } catch (const std::exception &e) {
            throw std::runtime_error("Boot sector extraction failed: " + std::string(e.what()));
        }
    }

    /**
     * @brief Display usage information
     */
    static void print_usage(std::string_view program_name) noexcept {
        std::cout << "Usage: " << program_name << " [output_file]\n"
                  << "Extract pre-compiled boot sector to binary file\n"
                  << "\nArguments:\n"
                  << "  output_file    Output file path (default: bootblok)\n"
                  << "\nThe boot sector is a 512-byte binary containing:\n"
                  << "  - x86 real mode initialization code\n"
                  << "  - Protected mode transition\n"
                  << "  - Long mode activation\n"
                  << "  - Disk I/O routines\n"
                  << "  - Kernel loading logic\n";
    }
};

} // namespace minix::bootloader

/**
 * @brief Application entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status (0 = success, non-zero = error)
 */
int main(int argc, char *argv[]) noexcept {
    try {
        if (argc > 2) {
            minix::bootloader::BootSectorExtractor::print_usage(argv[0]);
            return 1;
        }

        const std::string output_file = (argc > 1) ? argv[1] : "bootblok";
        minix::bootloader::BootSectorExtractor::extract(output_file);

        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}
