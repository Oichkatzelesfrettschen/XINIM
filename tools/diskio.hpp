/**
 * @file diskio.hpp
 * @brief Cross-platform disk I/O interface for MINIX filesystem tools
 *
 * Provides a clean, safe interface for disk operations with proper
 * error handling and 32/64-bit portability. Implements RAII patterns
 * for resource management and strong typing for safety.
 *
 * @author C++23 conversion team
 * @version 2.0
 * @date 2024
 * @copyright BSD-MODERNMOST
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace diskio {

/**
 * @brief Disk I/O operation statistics
 */
struct IoStatistics {
    std::atomic<std::uint64_t> bytes_read{0};
    std::atomic<std::uint64_t> bytes_written{0};
    std::atomic<std::uint64_t> read_operations{0};
    std::atomic<std::uint64_t> write_operations{0};
    std::atomic<std::uint64_t> total_time_ms{0};

    void reset() noexcept {
        bytes_read = 0;
        bytes_written = 0;
        read_operations = 0;
        write_operations = 0;
        total_time_ms = 0;
    }

    [[nodiscard]] double get_read_throughput_mbps() const noexcept {
        const auto time_s = total_time_ms.load() / 1000.0;
        if (time_s <= 0.0)
            return 0.0;
        return (bytes_read.load() / (1024.0 * 1024.0)) / time_s;
    }

    [[nodiscard]] double get_write_throughput_mbps() const noexcept {
        const auto time_s = total_time_ms.load() / 1000.0;
        if (time_s <= 0.0)
            return 0.0;
        return (bytes_written.load() / (1024.0 * 1024.0)) / time_s;
    }
};

/**
 * @brief Disk I/O constants and configuration
 */
struct DiskConstants {
    static constexpr std::size_t SECTOR_SIZE = 512;               ///< Standard sector size
    static constexpr std::size_t MAX_SECTOR_SIZE = 4096;          ///< Maximum sector size
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 64 * 1024; ///< Default buffer size (64KB)
    static constexpr std::size_t MAX_BUFFER_SIZE = 1024 * 1024;   ///< Maximum buffer size (1MB)
    static constexpr std::uint32_t CACHE_LINE_SIZE = 64;          ///< CPU cache line size
};

/**
 * @brief Strong type for sector addressing
 *
 * Provides type safety for sector addresses and prevents
 * accidental mixing of different address types.
 */
struct SectorAddress {
    std::uint64_t value; ///< Raw sector address value

    explicit constexpr SectorAddress(std::uint64_t addr) noexcept : value(addr) {}
    constexpr operator std::uint64_t() const noexcept { return value; }

    // Arithmetic operations
    constexpr SectorAddress operator+(std::uint64_t offset) const noexcept {
        return SectorAddress(value + offset);
    }

    constexpr SectorAddress operator-(std::uint64_t offset) const noexcept {
        return SectorAddress(value - offset);
    }

    constexpr SectorAddress &operator+=(std::uint64_t offset) noexcept {
        value += offset;
        return *this;
    }

    constexpr SectorAddress &operator-=(std::uint64_t offset) noexcept {
        value -= offset;
        return *this;
    }

    // Comparison operations
    constexpr bool operator==(const SectorAddress &other) const noexcept {
        return value == other.value;
    }

    constexpr bool operator!=(const SectorAddress &other) const noexcept {
        return value != other.value;
    }

    constexpr bool operator<(const SectorAddress &other) const noexcept {
        return value < other.value;
    }

    constexpr bool operator<=(const SectorAddress &other) const noexcept {
        return value <= other.value;
    }

    constexpr bool operator>(const SectorAddress &other) const noexcept {
        return value > other.value;
    }

    constexpr bool operator>=(const SectorAddress &other) const noexcept {
        return value >= other.value;
    }
};

/**
 * @brief RAII buffer for sector data with alignment and bounds checking
 *
 * Provides safe access to sector data with automatic memory management,
 * proper alignment for performance, and bounds checking for safety.
 */
class SectorBuffer {
  private:
    std::vector<std::uint8_t> data_;

    /// @brief Align size to cache line boundary for optimal performance
    [[nodiscard]] static constexpr std::size_t align_size(std::size_t size) noexcept {
        return (size + DiskConstants::CACHE_LINE_SIZE - 1) & ~(DiskConstants::CACHE_LINE_SIZE - 1);
    }

  public:
    /**
     * @brief Construct buffer with specified size
     * @param size Buffer size in bytes (default: sector size)
     * @throws std::invalid_argument if size is zero or too large
     */
    explicit SectorBuffer(std::size_t size = DiskConstants::SECTOR_SIZE)
        : data_(align_size(size), 0) {
        if (size == 0) {
            throw std::invalid_argument("Buffer size cannot be zero");
        }
        if (size > DiskConstants::MAX_BUFFER_SIZE) {
            throw std::invalid_argument("Buffer size too large: " + std::to_string(size));
        }
    }

    /**
     * @brief Construct buffer from existing data
     * @param src Source data pointer
     * @param size Size of source data
     * @throws std::invalid_argument if src is null or size invalid
     */
    SectorBuffer(const void *src, std::size_t size)
        : data_(static_cast<const std::uint8_t *>(src),
                static_cast<const std::uint8_t *>(src) + size) {
        if (src == nullptr) {
            throw std::invalid_argument("Source pointer cannot be null");
        }
        if (size == 0) {
            throw std::invalid_argument("Size cannot be zero");
        }
        if (size > DiskConstants::MAX_BUFFER_SIZE) {
            throw std::invalid_argument("Size too large: " + std::to_string(size));
        }
    }

    // Accessors
    [[nodiscard]] std::uint8_t *data() noexcept { return data_.data(); }
    [[nodiscard]] const std::uint8_t *data() const noexcept { return data_.data(); }
    [[nodiscard]] std::size_t size_bytes() const noexcept { return data_.size(); }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    /**
     * @brief Get element at specified offset with bounds checking
     * @param offset Byte offset
     * @return Reference to byte at offset
     * @throws std::out_of_range if offset is invalid
     */
    [[nodiscard]] std::uint8_t &at(std::size_t offset) {
        if (offset >= data_.size()) {
            throw std::out_of_range("Buffer offset out of range: " + std::to_string(offset));
        }
        return data_[offset];
    }

    [[nodiscard]] const std::uint8_t &at(std::size_t offset) const {
        if (offset >= data_.size()) {
            throw std::out_of_range("Buffer offset out of range: " + std::to_string(offset));
        }
        return data_[offset];
    }

    // Mutators
    void resize(std::size_t new_size) {
        if (new_size > DiskConstants::MAX_BUFFER_SIZE) {
            throw std::invalid_argument("New size too large: " + std::to_string(new_size));
        }
        data_.resize(align_size(new_size));
    }

    void clear() { data_.clear(); }

    /**
     * @brief Fill buffer with specified byte value
     * @param value Byte value to fill with
     */
    void fill(std::uint8_t value) noexcept { std::fill(data_.begin(), data_.end(), value); }

    /**
     * @brief Zero out entire buffer
     */
    void zero() noexcept { fill(0); }
};

/**
 * @brief Cross-platform disk interface with caching and statistics
 *
 * Provides safe, efficient disk I/O operations with automatic resource
 * management, error handling, and performance monitoring.
 */
class DiskInterface {
  private:
    std::string device_path_;         ///< Path to disk device
    bool read_only_;                  ///< Read-only mode flag
    int fd_{-1};                      ///< File descriptor (Unix) or handle (Windows)
    mutable IoStatistics statistics_; ///< I/O operation statistics

    // Simple cache for recently accessed sectors
    struct CacheEntry {
        SectorAddress address;
        SectorBuffer data;
        std::chrono::steady_clock::time_point last_access;
        bool dirty{false};

        CacheEntry(SectorAddress addr, SectorBuffer buf)
            : address(addr), data(std::move(buf)), last_access(std::chrono::steady_clock::now()) {}
    };

    static constexpr std::size_t CACHE_SIZE = 64; ///< Number of cached sectors
    mutable std::vector<CacheEntry> cache_;       ///< Sector cache
    mutable std::size_t cache_next_victim_{0};    ///< Round-robin cache replacement

  public:
    /**
     * @brief Construct disk interface
     * @param device_path Path to disk device file
     * @param read_only Open in read-only mode (default: true)
     * @throws std::runtime_error if device cannot be opened
     */
    explicit DiskInterface(const std::string &device_path, bool read_only = true);

    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~DiskInterface();

    // Non-copyable, movable
    DiskInterface(const DiskInterface &) = delete;
    DiskInterface &operator=(const DiskInterface &) = delete;
    DiskInterface(DiskInterface &&) noexcept;
    DiskInterface &operator=(DiskInterface &&) noexcept;

    /**
     * @brief Read sector from disk with caching
     * @param sector Sector address to read
     * @return Buffer containing sector data
     * @throws std::runtime_error on I/O error
     */
    [[nodiscard]] SectorBuffer read_sector(SectorAddress sector);

    /**
     * @brief Write sector to disk with write-through caching
     * @param sector Sector address to write
     * @param buffer Data to write
     * @throws std::runtime_error on I/O error or if read-only
     */
    void write_sector(SectorAddress sector, const SectorBuffer &buffer);

    /**
     * @brief Read multiple consecutive sectors efficiently
     * @param start_sector First sector to read
     * @param count Number of sectors to read
     * @return Vector of sector buffers
     * @throws std::runtime_error on I/O error
     */
    [[nodiscard]] std::vector<SectorBuffer> read_sectors(SectorAddress start_sector,
                                                         std::size_t count);

    /**
     * @brief Write multiple consecutive sectors efficiently
     * @param start_sector First sector to write
     * @param buffers Vector of sector data to write
     * @throws std::runtime_error on I/O error or if read-only
     */
    void write_sectors(SectorAddress start_sector, const std::vector<SectorBuffer> &buffers);

    /**
     * @brief Flush all pending writes to disk
     * @throws std::runtime_error on I/O error
     */
    void sync();

    /**
     * @brief Flush and invalidate all cached data
     */
    void flush_cache();

    /**
     * @brief Get device size in bytes
     * @return Device size
     * @throws std::runtime_error if size cannot be determined
     */
    [[nodiscard]] std::uint64_t get_size() const;

    /**
     * @brief Get device size in sectors
     * @return Number of sectors
     */
    [[nodiscard]] std::uint64_t get_sector_count() const {
        return get_size() / DiskConstants::SECTOR_SIZE;
    }

    // Accessors
    [[nodiscard]] bool is_read_only() const noexcept { return read_only_; }
    [[nodiscard]] const std::string &get_device_path() const noexcept { return device_path_; }
    [[nodiscard]] const IoStatistics &get_statistics() const noexcept { return statistics_; }

    /**
     * @brief Reset I/O statistics
     */
    void reset_statistics() noexcept { statistics_.reset(); }

    /**
     * @brief Check if device is accessible
     * @return True if device can be accessed
     */
    [[nodiscard]] bool is_accessible() const noexcept;

  private:
    /**
     * @brief Open device file with appropriate flags
     * @throws std::runtime_error if open fails
     */
    void open_device();

    /**
     * @brief Close device file
     */
    void close_device() noexcept;

    /**
     * @brief Perform raw sector read without caching
     * @param sector Sector to read
     * @return Sector data
     * @throws std::runtime_error on I/O error
     */
    [[nodiscard]] SectorBuffer read_sector_raw(SectorAddress sector);

    /**
     * @brief Perform raw sector write without caching
     * @param sector Sector to write
     * @param buffer Data to write
     * @throws std::runtime_error on I/O error
     */
    void write_sector_raw(SectorAddress sector, const SectorBuffer &buffer);

    /**
     * @brief Find sector in cache
     * @param sector Sector address
     * @return Iterator to cache entry or end() if not found
     */
    [[nodiscard]] std::vector<CacheEntry>::iterator find_in_cache(SectorAddress sector) const;

    /**
     * @brief Add sector to cache, evicting old entry if necessary
     * @param sector Sector address
     * @param data Sector data
     */
    void add_to_cache(SectorAddress sector, SectorBuffer data) const;

    /**
     * @brief Write back all dirty cache entries
     */
    void writeback_cache();

    /**
     * @brief Validate sector address
     * @param sector Sector to validate
     * @throws std::out_of_range if sector is invalid
     */
    void validate_sector_address(SectorAddress sector) const;
};

/**
 * @brief Disk I/O error with context information
 */
class DiskIoError : public std::runtime_error {
  private:
    std::string device_path_;
    SectorAddress sector_;
    std::string operation_;

  public:
    DiskIoError(const std::string &device_path, SectorAddress sector, const std::string &operation,
                const std::string &message)
        : std::runtime_error("Disk I/O error on " + device_path + " sector " +
                             std::to_string(sector.value) + " during " + operation + ": " +
                             message),
          device_path_(device_path), sector_(sector), operation_(operation) {}

    [[nodiscard]] const std::string &get_device_path() const noexcept { return device_path_; }
    [[nodiscard]] SectorAddress get_sector() const noexcept { return sector_; }
    [[nodiscard]] const std::string &get_operation() const noexcept { return operation_; }
};

} // namespace diskio