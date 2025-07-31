/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/**
 * @file build.cpp
 * @brief MINIX boot image builder implementation
 *
 * This file contains the complete implementation for building MINIX boot images from
 * component executables. The builder combines a bootblock, kernel, and system processes
 * into a single bootable image file with proper memory layout and cross-references.
 *
 * The build process involves:
 * 1. Copying the bootblock (boot sector)
 * 2. Loading and analyzing executable headers for kernel, mm, fs, init, and fsck
 * 3. Copying program code and data with proper alignment
 * 4. Patching the bootblock with total size and kernel entry point
 * 5. Creating kernel process table with memory layout information
 * 6. Setting up file system initialization data
 *
 * The resulting image follows the MINIX boot format with 512-byte sectors and
 * supports both separate and combined instruction/data space programs.
 *
 * Memory layout in the boot image:
 * - Sector 0: Bootblock (512 bytes)
 * - Sector 1+: Kernel, MM, FS, Init, FSCK (in that order)
 *
 * Key features:
 * - Strong typing for byte offsets to prevent parameter confusion
 * - RAII-based file management with automatic buffering
 * - Comprehensive error handling with descriptive messages
 * - Support for both 32-byte and 48-byte executable headers
 * - Automatic alignment and padding for memory layout requirements
 * - Magic number validation for data integrity
 *
 * This program assembles the MINIX operating system components into a bootable
 * disk image. It combines the bootloader, kernel, memory manager, file system,
 * init, and fsck into a single boot image with proper sector alignment and
 * patching for runtime execution.
 *
 * Modern C++17 implementation with proper error handling, type safety,
 * and architectural design patterns.
 *
 * @namespace minix::builder
 * @author MINIX build tools
 * @version 1.0
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace minix::builder {

/**
 * @brief Strong type for byte offsets to prevent parameter confusion
 *
 * This wrapper class provides type safety for byte offset values, preventing
 * accidental mixing of different numeric types in function parameters.
 * Uses explicit construction to avoid implicit conversions.
 */
struct ByteOffset {
    std::size_t value;

    /**
     * @brief Explicit constructor from size_t value
     * @param val The byte offset value
     * @note Explicit to prevent implicit conversions
     */
    explicit ByteOffset(std::size_t val) : value(val) {}

    /**
     * @brief Implicit conversion operator to size_t
     * @return The underlying byte offset value
     * @note Marked noexcept as it cannot fail
     */
    operator std::size_t() const noexcept { return value; }
};

/**
 * @brief Constants for the MINIX boot image format
 *
 * This struct contains all compile-time constants used throughout the boot
 * image building process. All values are based on the MINIX operating system
 * specification and memory layout requirements.
 */
struct BuildConstants {
    static constexpr std::size_t SECTOR_SIZE = 512; ///< Standard disk sector size in bytes
    static constexpr std::size_t PROGRAM_COUNT =
        5; ///< Number of programs: kernel + mm + fs + init + fsck
    static constexpr std::size_t PROGRAM_ORIGIN = 1536; ///< Base address where kernel loads (0x600)
    static constexpr std::size_t CLICK_SHIFT = 4;       ///< Bit shift for 16-byte memory clicks
    static constexpr std::size_t DS_OFFSET = 4;         ///< Offset for data segment value in kernel

    // Magic numbers for validation
    static constexpr std::uint16_t KERNEL_DATA_MAGIC =
        0x526F; ///< Magic number in kernel data space
    static constexpr std::uint16_t FS_DATA_MAGIC =
        0xDADA; ///< Magic number in file system data space

    // Header format constants
    static constexpr std::size_t HEADER_SHORT = 32; ///< Short executable header size
    static constexpr std::size_t HEADER_LONG = 48;  ///< Long executable header size
    static constexpr std::size_t SEP_ID_BIT =
        0x20; ///< Bit flag for separate instruction/data space

    // 64-bit kernel entry point
    static constexpr std::uint64_t KERNEL_ENTRY = 0x00100000ULL; ///< Kernel entry point address
};

/**
 * @brief Program type enumeration
 *
 * Defines the different types of programs that make up the MINIX system.
 * The order matters as it determines the loading sequence in the boot image.
 */
enum class ProgramType : std::size_t {
    KERNEL = 0, ///< MINIX kernel - core OS functionality
    MM = 1,     ///< Memory Manager - handles process memory allocation
    FS = 2,     ///< File System - manages file operations
    INIT = 3,   ///< Init process - first user process
    FSCK = 4    ///< File System Checker - disk integrity tool
};

/**
 * @brief Program segment information
 *
 * Contains all the metadata about a program's memory layout including
 * text (code), data, and BSS (uninitialized data) segments. Also tracks
 * whether the program uses separate instruction and data spaces.
 */
struct ProgramInfo {
    std::uint32_t text_size{0}; ///< Size of text (code) segment in bytes
    std::uint32_t data_size{0}; ///< Size of initialized data segment in bytes
    std::uint32_t bss_size{0};  ///< Size of uninitialized data (BSS) segment in bytes
    bool separate_id{false};    ///< True if program uses separate instruction/data spaces
    std::string name;           ///< Human-readable program name for debugging

    /**
     * @brief Calculate total program size in memory
     * @return Sum of text, data, and BSS segments in bytes
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::uint32_t total_size() const noexcept {
        return text_size + data_size + bss_size;
    }

    /**
     * @brief Calculate program size aligned to 16-byte boundaries
     * @return Total size rounded up to next 16-byte boundary
     * @note MINIX requires 16-byte alignment for memory management
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::uint32_t aligned_size() const noexcept {
        const auto total = total_size();
        const auto remainder = total % 16;
        return total + (remainder ? 16 - remainder : 0);
    }

    /**
     * @brief Calculate text segment size in 16-byte clicks
     * @return Number of 16-byte clicks for text segment
     * @note Only relevant for separate I&D programs, returns 0 otherwise
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::uint32_t text_clicks() const noexcept {
        return separate_id ? (text_size >> BuildConstants::CLICK_SHIFT) : 0;
    }

    /**
     * @brief Calculate data segment size in 16-byte clicks
     * @return Number of 16-byte clicks for data segment
     * @note For separate I&D: data+BSS size, for combined: total size
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::uint32_t data_clicks() const noexcept {
        const auto size = separate_id ? (data_size + bss_size) : total_size();
        return size >> BuildConstants::CLICK_SHIFT;
    }
};

/**
 * @brief Boot image sector buffer with automatic alignment
 *
 * Manages a single 512-byte sector buffer with automatic padding and
 * alignment. Provides safe access methods and tracks usage to prevent
 * buffer overflows. Used for building the boot image sector by sector.
 */
class SectorBuffer {
  private:
    std::array<std::uint8_t, BuildConstants::SECTOR_SIZE> buffer_{}; ///< Fixed-size sector buffer
    std::size_t used_bytes_{0}; ///< Number of bytes currently used in buffer

  public:
    /**
     * @brief Default constructor initializes empty buffer
     * @note Buffer is zero-initialized by default
     */
    SectorBuffer() = default;

    /**
     * @brief Reset buffer to empty state with zero fill
     * @note Marked noexcept as fill() and assignment cannot fail
     */
    void clear() noexcept {
        buffer_.fill(0);
        used_bytes_ = 0;
    }

    /**
     * @brief Check if buffer is completely full
     * @return True if buffer contains exactly 512 bytes
     * @note Marked noexcept as comparison cannot fail
     */
    [[nodiscard]] bool is_full() const noexcept {
        return used_bytes_ >= BuildConstants::SECTOR_SIZE;
    }

    /**
     * @brief Check if buffer is completely empty
     * @return True if buffer contains no data
     * @note Marked noexcept as comparison cannot fail
     */
    [[nodiscard]] bool is_empty() const noexcept { return used_bytes_ == 0; }

    /**
     * @brief Get number of bytes available for writing
     * @return Number of free bytes remaining in buffer
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::size_t available() const noexcept {
        return BuildConstants::SECTOR_SIZE - used_bytes_;
    }

    /**
     * @brief Get number of bytes currently used
     * @return Number of bytes written to buffer
     * @note Marked noexcept as member access cannot fail
     */
    [[nodiscard]] std::size_t size() const noexcept { return used_bytes_; }

    /**
     * @brief Get read-only pointer to buffer data
     * @return Const pointer to buffer start
     * @note Marked noexcept as data() cannot fail
     */
    [[nodiscard]] const std::uint8_t *data() const noexcept { return buffer_.data(); }

    /**
     * @brief Get writable pointer to buffer data
     * @return Mutable pointer to buffer start
     * @note Marked noexcept as data() cannot fail
     */
    [[nodiscard]] std::uint8_t *data() noexcept { return buffer_.data(); }

    /**
     * @brief Write data to buffer with overflow protection
     * @param data Pointer to source data
     * @param bytes Number of bytes to write
     * @return Number of bytes actually written (may be less than requested)
     * @note Automatically limits write size to available space
     * @note Uses std::copy for safe memory copying
     */
    std::size_t write(const std::uint8_t *data, std::size_t bytes) {
        const auto to_write = std::min(bytes, available());
        std::copy(data, data + to_write, buffer_.data() + used_bytes_);
        used_bytes_ += to_write;
        return to_write;
    }

    /**
     * @brief Set a single byte at specific offset
     * @param offset Byte position within sector (0-511)
     * @param value Byte value to set
     * @note Bounds checking prevents buffer overflow
     * @note Does not update used_bytes_ counter
     */
    void set_byte(std::size_t offset, std::uint8_t value) {
        if (offset < BuildConstants::SECTOR_SIZE) {
            buffer_[offset] = value;
        }
    }

    /**
     * @brief Get a single byte at specific offset
     * @param offset Byte position within sector (0-511)
     * @return Byte value at offset, or 0 if offset is out of bounds
     * @note Bounds checking prevents buffer overflow
     */
    [[nodiscard]] std::uint8_t get_byte(std::size_t offset) const {
        return offset < BuildConstants::SECTOR_SIZE ? buffer_[offset] : 0;
    }
};

/**
 * @brief Boot image file manager with proper RAII
 *
 * Manages the output boot image file with sector-based I/O and automatic
 * buffering. Provides high-level operations for reading/writing sectors
 * and individual bytes with proper error handling and resource management.
 */
class ImageFile {
  private:
    std::fstream file_;             ///< File stream for read/write access
    std::size_t current_sector_{0}; ///< Current sector position for sequential writing
    SectorBuffer buffer_;           ///< Internal sector buffer for sequential writes

    /**
     * @brief Flush internal buffer to current sector
     * @throws std::system_error if write operation fails
     * @note Automatically advances to next sector after flush
     * @note Only flushes if buffer contains data
     */
    void flush_buffer() {
        if (!buffer_.is_empty()) {
            write_sector(current_sector_, buffer_);
            buffer_.clear();
            ++current_sector_;
        }
    }

  public:
    /**
     * @brief Construct image file manager and create output file
     * @param path Filesystem path for the output image
     * @throws std::system_error if file cannot be created or opened
     * @note Opens file in binary read/write mode with truncation
     * @note File is created if it doesn't exist
     */
    explicit ImageFile(const std::string &path) {
        // Create and open file for read/write
        file_.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!file_.is_open()) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                    "Failed to create image file: " + path);
        }
    }

    /**
     * @brief Destructor ensures buffer is flushed before destruction
     * @note Uses RAII pattern for automatic resource cleanup
     * @note Catches and logs exceptions to prevent destructor from throwing
     */
    ~ImageFile() {
        try {
            flush_buffer();
        } catch (const std::exception &) {
            // Log error but don't throw in destructor
            std::cerr << "Warning: Failed to flush buffer in destructor\n";
        }
    }

    /**
     * @brief Write arbitrary data with automatic sector management
     * @param data Pointer to source data
     * @param size Number of bytes to write
     * @throws std::system_error if write operation fails
     * @note Automatically handles sector boundaries and buffering
     * @note Large writes are split across multiple sectors as needed
     */
    void write_data(const std::uint8_t *data, std::size_t size) {
        std::size_t remaining = size;
        const std::uint8_t *ptr = data;

        while (remaining > 0) {
            const auto written = buffer_.write(ptr, remaining);
            ptr += written;
            remaining -= written;

            if (buffer_.is_full()) {
                flush_buffer();
            }
        }
    }

    /**
     * @brief Write complete sector to specific position
     * @param sector Zero-based sector number
     * @param data Sector buffer containing exactly 512 bytes
     * @throws std::system_error if seek or write operation fails
     * @note Performs random access to any sector position
     * @note Always writes exactly 512 bytes regardless of buffer content
     */
    void write_sector(std::size_t sector, const SectorBuffer &data) {
        if (sector > static_cast<std::size_t>(std::numeric_limits<std::streamoff>::max() /
                                              BuildConstants::SECTOR_SIZE)) {
            throw std::system_error(std::make_error_code(std::errc::value_too_large),
                                    "Sector number too large");
        }
        const auto pos = static_cast<std::streamoff>(sector) *
                         static_cast<std::streamoff>(BuildConstants::SECTOR_SIZE);
        file_.seekp(pos);
        file_.write(reinterpret_cast<const char *>(data.data()),
                    static_cast<std::streamsize>(BuildConstants::SECTOR_SIZE));
        if (!file_.good()) {
            throw std::system_error(std::make_error_code(std::errc::io_error),
                                    "Failed to write sector " + std::to_string(sector));
        }
    }

    /**
     * @brief Read complete sector from specific position
     * @param sector Zero-based sector number
     * @param data Sector buffer to receive 512 bytes
     * @throws std::system_error if seek or read operation fails
     * @note Performs random access to any sector position
     * @note Always reads exactly 512 bytes into the buffer
     */
    void read_sector(std::size_t sector, SectorBuffer &data) {
        if (sector > static_cast<std::size_t>(std::numeric_limits<std::streamoff>::max() /
                                              BuildConstants::SECTOR_SIZE)) {
            throw std::system_error(std::make_error_code(std::errc::value_too_large),
                                    "Sector number too large");
        }
        const auto pos = static_cast<std::streamoff>(sector) *
                         static_cast<std::streamoff>(BuildConstants::SECTOR_SIZE);
        file_.seekg(pos);
        file_.read(reinterpret_cast<char *>(data.data()),
                   static_cast<std::streamsize>(BuildConstants::SECTOR_SIZE));
        if (!file_.good()) {
            throw std::system_error(std::make_error_code(std::errc::io_error),
                                    "Failed to read sector " + std::to_string(sector));
        }
    }

    /**
     * @brief Modify single byte at absolute file offset
     * @param offset Byte offset from start of file
     * @param value New byte value
     * @throws std::system_error if read/write operations fail
     * @note Implements read-modify-write cycle for single byte
     * @note Automatically calculates sector and byte offset
     */
    void put_byte(ByteOffset offset, std::uint8_t value) {
        const auto sector = offset.value / BuildConstants::SECTOR_SIZE;
        const auto byte_offset = offset.value % BuildConstants::SECTOR_SIZE;

        SectorBuffer temp;
        read_sector(sector, temp);
        temp.set_byte(byte_offset, value);
        write_sector(sector, temp);
    }

    /**
     * @brief Read single byte from absolute file offset
     * @param offset Byte offset from start of file
     * @return Byte value at specified offset
     * @throws std::system_error if read operation fails
     * @note Automatically calculates sector and byte offset
     */
    [[nodiscard]] std::uint8_t get_byte(ByteOffset offset) {
        const auto sector = offset.value / BuildConstants::SECTOR_SIZE;
        const auto byte_offset = offset.value % BuildConstants::SECTOR_SIZE;

        SectorBuffer temp;
        read_sector(sector, temp);
        return temp.get_byte(byte_offset);
    }

    /**
     * @brief Force all buffered data to be written to disk
     * @throws std::system_error if flush operations fail
     * @note Ensures data persistence before program exit
     * @note Flushes both internal buffer and file stream
     */
    void flush() {
        flush_buffer();
        file_.flush();
    }

    /**
     * @brief Get current write position in bytes
     * @return Current file offset for next write operation
     * @note Combines sector position with buffer usage
     * @note Marked noexcept as arithmetic cannot fail
     */
    [[nodiscard]] std::size_t current_position() const noexcept {
        return current_sector_ * BuildConstants::SECTOR_SIZE + buffer_.size();
    }
};

/**
 * @brief MINIX executable header parser
 *
 * Parses MINIX executable file headers to extract program segment information.
 * Supports both 32-byte and 48-byte header formats and validates header
 * integrity during parsing.
 */
class ExecutableHeader {
  private:
    std::array<std::uint32_t, 12> header_data_{}; ///< Raw header data storage

  public:
    /**
     * @brief Parse executable header from file stream
     * @param file Input file stream positioned at header start
     * @throws std::runtime_error if header is invalid or read fails
     * @note Automatically detects header length (32 or 48 bytes)
     * @note File stream must be opened in binary mode
     */
    explicit ExecutableHeader(std::ifstream &file) {
        // Read first 8 bytes to get header length
        std::array<std::uint16_t, 4> initial_data{};
        file.read(reinterpret_cast<char *>(initial_data.data()), 8);
        if (!file.good()) {
            throw std::runtime_error("Failed to read executable header");
        }

        const auto header_len = initial_data[2]; // HDR_LEN position
        if (header_len != BuildConstants::HEADER_SHORT &&
            header_len != BuildConstants::HEADER_LONG) {
            throw std::runtime_error("Invalid header length: " + std::to_string(header_len));
        }

        // Read rest of header
        const auto remaining = header_len - 8;
        file.read(reinterpret_cast<char *>(header_data_.data()), remaining);
        if (!file.good()) {
            throw std::runtime_error("Failed to read complete header");
        }

        // Store the initial data
        std::copy(initial_data.begin(), initial_data.end(),
                  reinterpret_cast<std::uint16_t *>(header_data_.data()));
    }

    /**
     * @brief Check if program uses separate instruction and data spaces
     * @return True if program has separate I&D space
     * @note Separate I&D allows larger programs by using distinct address spaces
     * @note Marked noexcept as bit operations cannot fail
     */
    [[nodiscard]] bool is_separate_id() const noexcept {
        const auto *hd = reinterpret_cast<const std::uint16_t *>(header_data_.data());
        return (hd[1] & BuildConstants::SEP_ID_BIT) != 0;
    }

    /**
     * @brief Get text (code) segment size
     * @return Size of text segment in bytes
     * @note Text segment contains executable machine code
     * @note Marked noexcept as array access cannot fail
     */
    [[nodiscard]] std::uint32_t text_size() const noexcept {
        return header_data_[2]; // TEXT_POS
    }

    /**
     * @brief Get initialized data segment size
     * @return Size of data segment in bytes
     * @note Data segment contains initialized global variables
     * @note Marked noexcept as array access cannot fail
     */
    [[nodiscard]] std::uint32_t data_size() const noexcept {
        return header_data_[3]; // DATA_POS
    }

    /**
     * @brief Get uninitialized data (BSS) segment size
     * @return Size of BSS segment in bytes
     * @note BSS segment contains uninitialized global variables
     * @note Marked noexcept as array access cannot fail
     */
    [[nodiscard]] std::uint32_t bss_size() const noexcept {
        return header_data_[4]; // BSS_POS
    }
};

/**
 * @brief Main boot image builder class
 *
 * Orchestrates the entire boot image building process. Manages program
 * information, handles file I/O, and applies all necessary patches to
 * create a bootable MINIX image.
 */
class BootImageBuilder {
  private:
    std::array<ProgramInfo, BuildConstants::PROGRAM_COUNT>
        programs_;                     ///< Information for all 5 programs
    std::unique_ptr<ImageFile> image_; ///< Output image file manager
    std::uint64_t os_size_{0};         ///< Size of OS without fsck
    std::uint64_t total_size_{0};      ///< Total size including fsck

    /// Program names for display and debugging
    static constexpr std::array<const char *, 5> program_names_ = {"kernel", "mm", "fs", "init",
                                                                   "fsck"};

    /**
     * @brief Copy bootblock (boot sector) to image
     * @param bootblock_path Path to bootblock binary file
     * @throws std::system_error if file cannot be opened
     * @throws std::runtime_error if file is empty
     * @note Bootblock must be exactly 512 bytes or less
     * @note Bootblock contains the initial boot code loaded by BIOS
     */
    void copy_bootblock(const std::string &bootblock_path) {
        std::ifstream file(bootblock_path, std::ios::binary);
        if (!file.is_open()) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                    "Cannot open bootblock: " + bootblock_path);
        }

        std::array<std::uint8_t, BuildConstants::SECTOR_SIZE> buffer{};
        file.read(reinterpret_cast<char *>(buffer.data()), BuildConstants::SECTOR_SIZE);
        const auto bytes_read = file.gcount();

        if (bytes_read <= 0) {
            throw std::runtime_error("Empty bootblock file");
        }

        image_->write_data(buffer.data(), bytes_read);
    }

    /**
     * @brief Copy program executable to image with header parsing
     * @param type Program type (KERNEL, MM, FS, INIT, or FSCK)
     * @param program_path Path to executable file
     * @throws std::system_error if file cannot be opened
     * @throws std::runtime_error if header is invalid or read fails
     * @note Parses executable header and extracts segment information
     * @note Applies 16-byte alignment padding as required
     * @note Updates global size counters for later use
     */
    void copy_program(ProgramType type, const std::string &program_path) {
        const auto index = static_cast<std::size_t>(type);
        auto &prog = programs_[index];
        prog.name = program_names_[index];

        std::ifstream file(program_path, std::ios::binary);
        if (!file.is_open()) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory),
                                    "Cannot open program: " + program_path);
        }

        ExecutableHeader header(file);
        prog.text_size = header.text_size();
        prog.data_size = header.data_size();
        prog.bss_size = header.bss_size();
        prog.separate_id = header.is_separate_id();

        // Validate separate I&D alignment
        if (prog.separate_id && (prog.text_size % 16) != 0) {
            throw std::runtime_error("Separate I&D requires 16-byte aligned text size in " +
                                     program_path);
        }

        const auto total = prog.total_size();
        const auto remainder = total % 16;
        const auto filler = remainder ? 16 - remainder : 0;
        prog.bss_size += filler;

        const auto final_size = prog.aligned_size();
        if (type != ProgramType::FSCK) {
            os_size_ += final_size;
        }
        total_size_ += final_size;

        // Print program info
        if (type != ProgramType::FSCK) {
            std::cout << std::setw(8) << prog.name << "  text=" << std::setw(5) << prog.text_size
                      << "  data=" << std::setw(5) << prog.data_size << "  bss=" << std::setw(5)
                      << prog.bss_size << "  tot=" << std::setw(5) << final_size
                      << "  hex=" << std::setw(4) << std::hex << final_size << std::dec
                      << (prog.separate_id ? "  Separate I & D" : "") << '\n';
        }

        // Copy text and data
        const auto code_size = prog.text_size + prog.data_size;
        std::vector<std::uint8_t> code_buffer(code_size);
        file.read(reinterpret_cast<char *>(code_buffer.data()), code_size);
        if (file.gcount() != static_cast<std::streamsize>(code_size)) {
            throw std::runtime_error("Failed to read program code from " + program_path);
        }

        image_->write_data(code_buffer.data(), code_size);

        // Write BSS (zeros)
        std::vector<std::uint8_t> bss_buffer(prog.bss_size, 0);
        image_->write_data(bss_buffer.data(), prog.bss_size);
    }

    /**
     * @brief Patch bootblock with total size and kernel entry point
     * @throws std::system_error if read/write operations fail
     * @note Updates the last 8 bytes of bootblock with:
     *       - Total sectors in image (16-bit)
     *       - Kernel entry point address (32-bit)
     *       - Reserved field (16-bit)
     * @note All values stored in little-endian format
     * @note Bootloader reads these values to load the complete image
     */
    void patch_bootblock() {
        const auto sectors =
            (total_size_ + BuildConstants::SECTOR_SIZE - 1) / BuildConstants::SECTOR_SIZE;

        SectorBuffer boot_sector;
        image_->read_sector(0, boot_sector);

        // Patch sector count and kernel entry (little-endian)
        const auto entry = BuildConstants::KERNEL_ENTRY;
        const auto base_offset = BuildConstants::SECTOR_SIZE - 8;

        auto *data = reinterpret_cast<std::uint16_t *>(boot_sector.data());
        data[(base_offset / 2) + 0] = static_cast<std::uint16_t>(sectors + 1);
        data[(base_offset / 2) + 1] = static_cast<std::uint16_t>(entry & 0xFFFF);
        data[(base_offset / 2) + 2] = static_cast<std::uint16_t>((entry >> 16) & 0xFFFF);
        data[(base_offset / 2) + 3] = 0; // Reserved

        image_->write_sector(0, boot_sector);
    }

    /**
     * @brief Patch kernel data space with process table information
     * @throws std::runtime_error if kernel magic number not found
     * @throws std::system_error if read/write operations fail
     * @note Updates kernel's process table with memory layout for each program
     * @note Writes text and data click counts for memory manager
     * @note Updates kernel's data segment (DS) register value
     * @note Validates kernel data space using magic number
     */
    void patch_kernel_table() {
        // Find kernel data space
        const auto data_offset = BuildConstants::SECTOR_SIZE + programs_[0].text_size;

        // Verify magic number
        const auto magic =
            (static_cast<std::uint16_t>(image_->get_byte(ByteOffset(data_offset + 1))) << 8) |
            image_->get_byte(ByteOffset(data_offset));
        if (magic != BuildConstants::KERNEL_DATA_MAGIC) {
            throw std::runtime_error("Kernel data magic number not found");
        }

        // Write program table
        for (std::size_t i = 0; i < BuildConstants::PROGRAM_COUNT - 1; ++i) {
            const auto &prog = programs_[i];
            const auto text_clicks = prog.text_clicks();
            const auto data_clicks = prog.data_clicks();

            const auto offset = data_offset + 4 * i;
            image_->put_byte(ByteOffset(offset + 0), text_clicks & 0xFF);
            image_->put_byte(ByteOffset(offset + 1), (text_clicks >> 8) & 0xFF);
            image_->put_byte(ByteOffset(offset + 2), data_clicks & 0xFF);
            image_->put_byte(ByteOffset(offset + 3), (data_clicks >> 8) & 0xFF);
        }

        // Write kernel DS value
        const auto kernel_ds = programs_[0].separate_id
                                   ? (BuildConstants::PROGRAM_ORIGIN + programs_[0].text_size) >>
                                         BuildConstants::CLICK_SHIFT
                                   : BuildConstants::PROGRAM_ORIGIN >> BuildConstants::CLICK_SHIFT;

        const auto ds_offset = BuildConstants::SECTOR_SIZE + BuildConstants::DS_OFFSET;
        image_->put_byte(ByteOffset(ds_offset), kernel_ds & 0xFF);
        image_->put_byte(ByteOffset(ds_offset + 1), (kernel_ds >> 8) & 0xFF);
    }

    /**
     * @brief Patch file system data space with init process information
     * @throws std::runtime_error if file system magic number not found
     * @throws std::system_error if read/write operations fail
     * @note Updates file system's data space with init process location
     * @note Provides memory layout information for launching init
     * @note Validates file system data space using magic number
     * @note Init is the first user process started by the file system
     */
    void patch_fs_init_info() {
        // Calculate file system data offset
        auto fs_offset = BuildConstants::SECTOR_SIZE;
        fs_offset += programs_[0].aligned_size(); // kernel
        fs_offset += programs_[1].text_size;      // mm text

        // Verify FS magic
        const auto magic =
            (static_cast<std::uint16_t>(image_->get_byte(ByteOffset(fs_offset + 1))) << 8) |
            image_->get_byte(ByteOffset(fs_offset));
        if (magic != BuildConstants::FS_DATA_MAGIC) {
            throw std::runtime_error("File system data magic number not found");
        }

        // Calculate init program location
        auto init_org = BuildConstants::PROGRAM_ORIGIN;
        init_org += programs_[0].aligned_size(); // kernel
        init_org += programs_[1].aligned_size(); // mm
        init_org += programs_[2].aligned_size(); // fs

        const auto init_org_clicks = init_org >> BuildConstants::CLICK_SHIFT;
        const auto init_text_clicks = programs_[3].text_clicks();
        const auto init_data_clicks = programs_[3].data_clicks();

        // Write init info to FS data space
        image_->put_byte(ByteOffset(fs_offset + 4), init_org_clicks & 0xFF);
        image_->put_byte(ByteOffset(fs_offset + 5), (init_org_clicks >> 8) & 0xFF);
        image_->put_byte(ByteOffset(fs_offset + 6), init_text_clicks & 0xFF);
        image_->put_byte(ByteOffset(fs_offset + 7), (init_text_clicks >> 8) & 0xFF);
        image_->put_byte(ByteOffset(fs_offset + 8), init_data_clicks & 0xFF);
        image_->put_byte(ByteOffset(fs_offset + 9), (init_data_clicks >> 8) & 0xFF);
    }

  public:
    /**
     * @brief Construct boot image builder with output file
     * @param output_path Path where boot image will be created
     * @throws std::system_error if output file cannot be created
     * @note Creates and opens output file immediately
     * @note Uses RAII for automatic resource management
     */
    explicit BootImageBuilder(const std::string &output_path) {
        image_ = std::make_unique<ImageFile>(output_path);
    }

    /**
     * @brief Build complete boot image from input files
     * @param input_files Vector of paths: [bootblock, kernel, mm, fs, init, fsck]
     * @throws std::invalid_argument if wrong number of input files provided
     * @throws std::system_error for file I/O errors
     * @throws std::runtime_error for data validation errors
     * @note Coordinates entire build process:
     *       1. Copy bootblock and all programs
     *       2. Apply size and alignment calculations
     *       3. Patch bootblock with metadata
     *       4. Update kernel process table
     *       5. Initialize file system data
     */
    void build(const std::vector<std::string> &input_files) {
        if (input_files.size() != 6) {
            throw std::invalid_argument("Expected 6 input files");
        }

        std::cout << "Building MINIX boot image...\n\n";

        // Copy bootblock
        copy_bootblock(input_files[0]);

        // Copy programs
        for (std::size_t i = 0; i < BuildConstants::PROGRAM_COUNT; ++i) {
            copy_program(static_cast<ProgramType>(i), input_files[i + 1]);
        }

        image_->flush();

        // Print summary
        std::cout << "                                               -----     -----\n";
        std::cout << "Operating system size  " << std::setw(29) << os_size_ << "     "
                  << std::setw(5) << std::hex << os_size_ << std::dec << '\n';
        std::cout << "\nTotal size including fsck is " << total_size_ << ".\n\n";

        // Apply patches
        std::cout << "Applying patches...\n";
        patch_bootblock();
        patch_kernel_table();
        patch_fs_init_info();

        std::cout << "Boot image successfully created.\n";
    }

    /**
     * @brief Command line argument parser
     *
     * Parses and validates command line arguments for the boot image builder.
     * Ensures all required files are provided and exist before building begins.
     */
    class ArgumentParser {
      public:
        /**
         * @brief Parsed command line arguments structure
         */
        struct Arguments {
            std::string output_file;              ///< Path for output boot image
            std::vector<std::string> input_files; ///< Paths for input components
        };

        /**
         * @brief Parse command line arguments with validation
         * @param argc Number of command line arguments
         * @param argv Array of command line argument strings
         * @return Parsed and validated arguments structure
         * @throws std::invalid_argument if wrong number of arguments
         * @throws std::system_error if input files don't exist
         * @note Expects exactly 8 arguments: program name + 6 inputs + 1 output
         * @note Validates existence of all input files before proceeding
         * @note Prints usage information on error
         */
        static Arguments parse(int argc, char *argv[]) {
            if (argc != 8) {
                print_usage(argv[0]);
                throw std::invalid_argument("Invalid number of arguments");
            }

            Arguments args;
            args.output_file = argv[7];

            for (int i = 1; i < 7; ++i) {
                args.input_files.emplace_back(argv[i]);

                // Verify file exists
                std::ifstream test_file(args.input_files.back());
                if (!test_file.good()) {
                    throw std::system_error(
                        std::make_error_code(std::errc::no_such_file_or_directory),
                        "Input file not found: " + args.input_files.back());
                }
            }

            return args;
        }

      private:
        /**
         * @brief Print program usage information
         * @param program_name Name of the program executable
         * @note Displays expected argument format and descriptions
         * @note Called automatically when argument parsing fails
         */
        static void print_usage(const char *program_name) {
            std::cout << "Usage: " << program_name
                      << " bootblock kernel mm fs init fsck output_image\n"
                      << "\nBuilds a MINIX boot image from component files.\n"
                      << "\nArguments:\n"
                      << "  bootblock    Boot sector binary (512 bytes)\n"
                      << "  kernel       MINIX kernel executable\n"
                      << "  mm           Memory manager executable\n"
                      << "  fs           File system executable\n"
                      << "  init         Init process executable\n"
                      << "  fsck         File system checker executable\n"
                      << "  output_image Output boot image file\n";
        }
    };

}; // End of BootImageBuilder class

} // namespace minix::builder

/**
 * @brief Application entry point
 *
 * Main function coordinates argument parsing and boot image building
 * with comprehensive error handling and proper exit codes.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code: 0 for success, 1 for errors, 2 for unknown errors
 * @note Marked noexcept to prevent exceptions from escaping main
 * @note Catches all exceptions and converts to appropriate exit codes
 * @note Provides user-friendly error messages for all failure cases
 */
int main(int argc, char *argv[]) noexcept {
    try {
        const auto args = minix::builder::ArgumentParser::parse(argc, argv);

        minix::builder::BootImageBuilder builder(args.output_file);
        builder.build(args.input_files);

        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 2;
    }
}
