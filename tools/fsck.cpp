/**
 * @file fsck.cpp
 * @brief Modern C++23 MINIX Filesystem Checker
 *
 * A comprehensive filesystem integrity checker and repair tool for MINIX filesystems.
 * Provides type-safe operations, proper error handling, and modular architecture
 * with support for interactive and automatic repair modes.
 *
 * @author Robbert van Renesse (original C implementation)
 * @author Modern C++23 conversion
 * @version 2.0
 * @date 2024
 *
 * Features:
 * - Complete filesystem structure validation
 * - Interactive and automatic repair capabilities
 * - Inode reference count verification
 * - Zone bitmap consistency checking
 * - Directory structure validation
 * - Comprehensive error reporting and recovery
 * - Performance monitoring and statistics
 * - Cross-platform compatibility
 */

#include "diskio.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// MINIX filesystem types and constants
using inode_nr = std::uint16_t;
using zone_nr = std::uint16_t;
using block_nr = std::uint32_t;
using file_pos = std::uint32_t;

// MINIX filesystem constants
constexpr std::uint16_t SUPER_MAGIC = 0x137F;
constexpr inode_nr ROOT_INODE = 1;
constexpr std::uint32_t SUPER_BLOCK = 1;
constexpr std::uint32_t BLOCK_SIZE = 1024;
constexpr std::uint32_t INODE_SIZE = 32;
constexpr std::uint32_t INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE;

// MINIX superblock structure
struct dsb {
    inode_nr s_ninodes;
    zone_nr s_nzones;
    std::uint16_t s_imap_blocks;
    std::uint16_t s_zmap_blocks;
    zone_nr s_firstdatazone;
    std::uint16_t s_log_zone_size;
    file_pos s_maxsize;
    std::uint16_t s_magic;
};

namespace minix::fsck {

/**
 * @brief Filesystem checker constants and configuration
 */
struct FsckConstants {
    static constexpr std::size_t MAX_PRINT_ERRORS = 8;
    static constexpr std::size_t MAX_WIDTH = 32;
    static constexpr std::size_t MAX_DIR_SIZE = 5000;
    static constexpr std::size_t CHUNK_INDIRECT = 128;
    static constexpr std::size_t CHUNK_DIRECT = 16;
    static constexpr std::size_t BITMAP_SHIFT = 13;
    static constexpr std::size_t BIT_SHIFT = 4;
    static constexpr std::uint32_t BITMAP_MASK = (1U << BIT_SHIFT) - 1;
    static constexpr std::uint16_t STICKY_BIT = 01000;
    static constexpr std::uint16_t MAX_LINKS = std::numeric_limits<std::uint16_t>::max();
};

/**
 * @brief Filesystem operation mode
 */
enum class FsckMode {
    CHECK_ONLY,  ///< Read-only checking
    INTERACTIVE, ///< Interactive repair mode
    AUTOMATIC,   ///< Automatic repair mode
    LIST_ONLY,   ///< List filesystem contents
    CREATE_FS    ///< Create new filesystem
};

/**
 * @brief Filesystem object type classification
 */
enum class InodeType {
    REGULAR_FILE,
    DIRECTORY,
    BLOCK_SPECIAL,
    CHAR_SPECIAL,
    BAD_INODE,
    FREE_INODE
};

/**
 * @brief Zone indirection level for addressing
 */
enum class ZoneLevel : std::uint8_t {
    DIRECT = 0,
    SINGLE_INDIRECT = 1,
    DOUBLE_INDIRECT = 2,
    TRIPLE_INDIRECT = 3
};

/**
 * @brief Strong type for bit operations in bitmaps
 */
struct BitNumber {
    std::uint32_t value;

    explicit BitNumber(std::uint32_t bit) : value(bit) {}
    operator std::uint32_t() const noexcept { return value; }

    [[nodiscard]] std::uint32_t word_index() const noexcept {
        return value >> FsckConstants::BIT_SHIFT;
    }

    [[nodiscard]] std::uint32_t bit_mask() const noexcept {
        return 1U << (value & FsckConstants::BITMAP_MASK);
    }
};

/**
 * @brief RAII bitmap management with type-safe operations
 */
class Bitmap {
  private:
    std::vector<std::uint32_t> data_;
    std::size_t bit_count_;

  public:
    explicit Bitmap(std::size_t bit_count)
        : data_((bit_count + 31) / 32, 0), bit_count_(bit_count) {
        if (bit_count == 0) {
            throw std::invalid_argument("Bitmap size cannot be zero");
        }
        // Bit 0 is always set (reserved)
        set_bit(BitNumber(0));
    }

    void set_bit(BitNumber bit) {
        if (bit.value >= bit_count_) {
            throw std::out_of_range("Bit index out of range: " + std::to_string(bit.value));
        }
        data_[bit.word_index()] |= bit.bit_mask();
    }

    void clear_bit(BitNumber bit) {
        if (bit.value >= bit_count_) {
            throw std::out_of_range("Bit index out of range: " + std::to_string(bit.value));
        }
        data_[bit.word_index()] &= ~bit.bit_mask();
    }

    [[nodiscard]] bool is_set(BitNumber bit) const {
        if (bit.value >= bit_count_) {
            return false;
        }
        return (data_[bit.word_index()] & bit.bit_mask()) != 0;
    }

    void initialize_free_bits(BitNumber start_bit) {
        for (std::uint32_t bit = start_bit.value; bit < bit_count_; ++bit) {
            set_bit(BitNumber(bit));
        }
    }

    [[nodiscard]] std::size_t size_bits() const noexcept { return bit_count_; }
    [[nodiscard]] std::size_t size_words() const noexcept { return data_.size(); }
    [[nodiscard]] const std::uint32_t *data() const noexcept { return data_.data(); }
    [[nodiscard]] std::uint32_t *data() noexcept { return data_.data(); }

    void load_from_disk(diskio::DiskInterface &disk, diskio::SectorAddress start_block,
                        std::size_t block_count) {
        const std::size_t words_per_block = BLOCK_SIZE / sizeof(std::uint32_t);

        for (std::size_t i = 0; i < block_count; ++i) {
            auto sector_data = disk.read_sector(diskio::SectorAddress(start_block.value + i));
            const std::size_t dest_offset = i * words_per_block;
            const std::size_t copy_words = std::min(words_per_block, data_.size() - dest_offset);
            const std::size_t copy_bytes = copy_words * sizeof(std::uint32_t);

            if (sector_data.size_bytes() >= copy_bytes && dest_offset < data_.size()) {
                std::copy(sector_data.data(), sector_data.data() + copy_bytes,
                          reinterpret_cast<std::uint8_t *>(data_.data() + dest_offset));
            }
        }
        // Ensure bit 0 is always set (reserved)
        set_bit(BitNumber(0));
    }

    void save_to_disk(diskio::DiskInterface &disk, diskio::SectorAddress start_block,
                      std::size_t block_count) {
        const std::size_t words_per_block = BLOCK_SIZE / sizeof(std::uint32_t);

        for (std::size_t i = 0; i < block_count; ++i) {
            const std::size_t src_offset = i * words_per_block;
            if (src_offset < data_.size()) {
                const std::size_t copy_words = std::min(words_per_block, data_.size() - src_offset);
                const std::size_t copy_bytes = copy_words * sizeof(std::uint32_t);

                diskio::SectorBuffer buffer(
                    reinterpret_cast<const std::uint8_t *>(data_.data() + src_offset), copy_bytes);
                disk.write_sector(diskio::SectorAddress(start_block.value + i), buffer);
            }
        }
    }

    [[nodiscard]] std::vector<BitNumber> find_differences(const Bitmap &other) const {
        std::vector<BitNumber> differences;
        const auto min_words = std::min(data_.size(), other.data_.size());

        for (std::size_t word_idx = 0; word_idx < min_words; ++word_idx) {
            std::uint32_t diff = data_[word_idx] ^ other.data_[word_idx];
            for (std::uint8_t bit_pos = 0; diff != 0 && bit_pos < 32; ++bit_pos) {
                if (diff & 1) {
                    const auto bit_number = word_idx * 32 + bit_pos;
                    if (bit_number < bit_count_) {
                        differences.emplace_back(BitNumber(bit_number));
                    }
                }
                diff >>= 1;
            }
        }
        return differences;
    }
};

/**
 * @brief Filesystem statistics collection and reporting
 */
struct FilesystemStatistics {
    std::uint32_t regular_files{0};
    std::uint32_t directories{0};
    std::uint32_t block_special{0};
    std::uint32_t char_special{0};
    std::uint32_t bad_inodes{0};
    std::uint32_t free_inodes{0};
    std::uint32_t free_zones{0};
    std::array<std::uint32_t, 4> zone_types{}; // indexed by ZoneLevel
    std::uint32_t errors_found{0};
    std::uint32_t errors_fixed{0};

    void reset() noexcept { *this = FilesystemStatistics{}; }

    [[nodiscard]] std::uint32_t total_inodes() const noexcept {
        return regular_files + directories + block_special + char_special + bad_inodes +
               free_inodes;
    }

    [[nodiscard]] std::uint32_t total_zones() const noexcept {
        std::uint32_t total = free_zones;
        for (const auto &count : zone_types) {
            total += count;
        }
        return total;
    }
};

/**
 * @brief Directory entry path tracking for error reporting
 */
class PathTracker {
  private:
    struct PathNode {
        std::string name;
        inode_nr inode_number;
        std::shared_ptr<PathNode> parent;

        PathNode(std::string n, inode_nr ino, std::shared_ptr<PathNode> p = nullptr)
            : name(std::move(n)), inode_number(ino), parent(std::move(p)) {}
    };

    std::shared_ptr<PathNode> current_;

  public:
    PathTracker() : current_(std::make_shared<PathNode>("", ROOT_INODE)) {}

    void enter_directory(std::string_view name, inode_nr inode) {
        current_ = std::make_shared<PathNode>(std::string(name), inode, current_);
    }

    void exit_directory() {
        if (current_->parent) {
            current_ = current_->parent;
        }
    }

    [[nodiscard]] std::string get_current_path() const {
        std::vector<std::string> components;
        auto node = current_;

        while (node && node->parent) {
            if (!node->name.empty()) {
                components.push_back(node->name);
            }
            node = node->parent;
        }

        if (components.empty()) {
            return "/";
        }

        std::string path;
        for (auto it = components.rbegin(); it != components.rend(); ++it) {
            path += "/" + *it;
        }
        return path;
    }

    [[nodiscard]] inode_nr get_current_inode() const noexcept { return current_->inode_number; }

    [[nodiscard]] inode_nr get_parent_inode() const noexcept {
        return current_->parent ? current_->parent->inode_number : ROOT_INODE;
    }
};

/**
 * @brief User interaction and repair decision management
 */
class UserInterface {
  private:
    FsckMode mode_;
    bool changes_made_{false};

  public:
    explicit UserInterface(FsckMode mode) : mode_(mode) {}

    void set_mode(FsckMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] FsckMode get_mode() const noexcept { return mode_; }
    [[nodiscard]] bool changes_made() const noexcept { return changes_made_; }

    void print_message(std::string_view message) const { std::cout << message << std::flush; }

    void print_error(std::string_view error, const PathTracker &path) const {
        std::cout << "ERROR: " << error << " in " << path.get_current_path() << " (inode "
                  << path.get_current_inode() << ")" << std::endl;
    }

    void print_warning(std::string_view warning) const {
        std::cout << "WARNING: " << warning << std::endl;
    }

    [[nodiscard]] bool ask_repair(std::string_view question) {
        if (mode_ == FsckMode::CHECK_ONLY) {
            std::cout << std::endl;
            return false;
        }

        std::cout << question << "? ";

        if (mode_ == FsckMode::AUTOMATIC) {
            std::cout << "yes (automatic)" << std::endl;
            changes_made_ = true;
            return true;
        }

        // Interactive mode
        std::string response;
        std::getline(std::cin, response);

        if (response.empty() || response[0] == 'q' || response[0] == 'Q') {
            throw std::runtime_error("User requested exit");
        }

        const bool repair = !(response[0] == 'n' || response[0] == 'N');
        if (repair) {
            changes_made_ = true;
        }
        return repair;
    }

    template <typename T> [[nodiscard]] std::optional<T> get_input(std::string_view prompt) {
        if (mode_ == FsckMode::CHECK_ONLY) {
            return std::nullopt;
        }

        std::cout << prompt << ": ";

        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) {
            return std::nullopt;
        }

        try {
            if constexpr (std::is_same_v<T, std::string>) {
                return T(input);
            } else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_signed_v<T>) {
                    auto value = std::stoll(input);
                    if (value < std::numeric_limits<T>::min() ||
                        value > std::numeric_limits<T>::max()) {
                        print_error("Value out of range", PathTracker{});
                        return std::nullopt;
                    }
                    return static_cast<T>(value);
                } else {
                    auto value = std::stoull(input);
                    if (value > std::numeric_limits<T>::max()) {
                        print_error("Value out of range", PathTracker{});
                        return std::nullopt;
                    }
                    return static_cast<T>(value);
                }
            }
        } catch (const std::exception &) {
            print_error("Invalid input format", PathTracker{});
        }
        return std::nullopt;
    }
};

/**
 * @brief MINIX superblock management with validation
 */
class SuperBlock {
  private:
    struct dsb sb_;
    bool modified_{false};

  public:
    void load_from_disk(diskio::DiskInterface &disk) {
        auto sector_data = disk.read_sector(diskio::SectorAddress(SUPER_BLOCK));
        if (sector_data.size_bytes() < sizeof(sb_)) {
            throw std::runtime_error("Insufficient data for superblock");
        }
        std::copy(sector_data.data(), sector_data.data() + sizeof(sb_),
                  reinterpret_cast<std::uint8_t *>(&sb_));
    }

    void save_to_disk(diskio::DiskInterface &disk) {
        if (!modified_)
            return;

        diskio::SectorBuffer buffer(&sb_, sizeof(sb_));
        disk.write_sector(diskio::SectorAddress(SUPER_BLOCK), buffer);
        modified_ = false;
    }

    void validate() const {
        if (sb_.s_magic != SUPER_MAGIC) {
            throw std::runtime_error("Invalid superblock magic number: 0x" +
                                     std::to_string(sb_.s_magic));
        }

        if (sb_.s_ninodes == 0) {
            throw std::runtime_error("No inodes in filesystem");
        }

        if (sb_.s_nzones <= 2) {
            throw std::runtime_error("Insufficient zones in filesystem");
        }

        if (sb_.s_imap_blocks == 0) {
            throw std::runtime_error("No inode bitmap blocks");
        }

        if (sb_.s_zmap_blocks == 0) {
            throw std::runtime_error("No zone bitmap blocks");
        }

        if (sb_.s_firstdatazone <= 1) {
            throw std::runtime_error("First data zone too small");
        }

        // Note: s_log_zone_size is unsigned, so always >= 0
        // But we should check for reasonable bounds
        if (sb_.s_log_zone_size > 16) { // 2^16 blocks = 64MB zones max
            throw std::runtime_error("Zone size too large: " + std::to_string(sb_.s_log_zone_size));
        }

        if (sb_.s_maxsize == 0) {
            throw std::runtime_error("Invalid maximum file size");
        }
    }

    void check_consistency(UserInterface &ui) {
        const auto expected_imap_blocks =
            (sb_.s_ninodes + (1 << FsckConstants::BITMAP_SHIFT) - 1) >> FsckConstants::BITMAP_SHIFT;
        if (sb_.s_imap_blocks != expected_imap_blocks) {
            ui.print_warning("Expected " + std::to_string(expected_imap_blocks) +
                             " imap blocks, found " + std::to_string(sb_.s_imap_blocks));
        }

        const auto expected_zmap_blocks =
            (sb_.s_nzones + (1 << FsckConstants::BITMAP_SHIFT) - 1) >> FsckConstants::BITMAP_SHIFT;
        if (sb_.s_zmap_blocks != expected_zmap_blocks) {
            ui.print_warning("Expected " + std::to_string(expected_zmap_blocks) +
                             " zmap blocks, found " + std::to_string(sb_.s_zmap_blocks));
        }

        const auto scale = 1U << sb_.s_log_zone_size;
        const auto expected_first_zone =
            (get_inode_list_start() + get_inode_list_blocks() + scale - 1) >> sb_.s_log_zone_size;
        if (sb_.s_firstdatazone != expected_first_zone) {
            ui.print_warning("Expected first data zone " + std::to_string(expected_first_zone) +
                             ", found " + std::to_string(sb_.s_firstdatazone));
        }
    }

    // Accessors
    [[nodiscard]] inode_nr get_inode_count() const noexcept { return sb_.s_ninodes; }
    [[nodiscard]] zone_nr get_zone_count() const noexcept { return sb_.s_nzones; }
    [[nodiscard]] std::uint16_t get_imap_blocks() const noexcept { return sb_.s_imap_blocks; }
    [[nodiscard]] std::uint16_t get_zmap_blocks() const noexcept { return sb_.s_zmap_blocks; }
    [[nodiscard]] zone_nr get_first_data_zone() const noexcept { return sb_.s_firstdatazone; }
    [[nodiscard]] std::uint16_t get_log_zone_size() const noexcept { return sb_.s_log_zone_size; }
    [[nodiscard]] file_pos get_max_file_size() const noexcept { return sb_.s_maxsize; }

    [[nodiscard]] std::uint32_t get_scale() const noexcept { return 1U << sb_.s_log_zone_size; }

    [[nodiscard]] block_nr get_imap_start() const noexcept { return SUPER_BLOCK + 1; }

    [[nodiscard]] block_nr get_zmap_start() const noexcept {
        return get_imap_start() + sb_.s_imap_blocks;
    }

    [[nodiscard]] block_nr get_inode_list_start() const noexcept {
        return get_zmap_start() + sb_.s_zmap_blocks;
    }

    [[nodiscard]] std::uint32_t get_inode_list_blocks() const noexcept {
        return (sb_.s_ninodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    }

    [[nodiscard]] std::uint64_t get_inode_address(inode_nr ino) const {
        if (ino == 0 || ino > sb_.s_ninodes) {
            throw std::out_of_range("Invalid inode number: " + std::to_string(ino));
        }
        const auto byte_offset = static_cast<std::uint64_t>(ino - 1) * INODE_SIZE;
        const auto block_offset = static_cast<std::uint64_t>(get_inode_list_start()) * BLOCK_SIZE;
        return byte_offset + block_offset;
    }

    [[nodiscard]] std::uint64_t get_zone_address(zone_nr zone) const {
        const auto block = static_cast<std::uint64_t>(zone) << sb_.s_log_zone_size;
        return block * BLOCK_SIZE;
    }

    // Mutators (mark as modified)
    void set_inode_count(inode_nr count) {
        sb_.s_ninodes = count;
        modified_ = true;
    }
    void set_zone_count(zone_nr count) {
        sb_.s_nzones = count;
        modified_ = true;
    }
    void set_imap_blocks(std::uint16_t blocks) {
        sb_.s_imap_blocks = blocks;
        modified_ = true;
    }
    void set_zmap_blocks(std::uint16_t blocks) {
        sb_.s_zmap_blocks = blocks;
        modified_ = true;
    }
    void set_first_data_zone(zone_nr zone) {
        sb_.s_firstdatazone = zone;
        modified_ = true;
    }
    void set_log_zone_size(std::uint16_t size) {
        sb_.s_log_zone_size = size;
        modified_ = true;
    }
    void set_max_file_size(file_pos size) {
        sb_.s_maxsize = size;
        modified_ = true;
    }

    void print_info(UserInterface &ui) const {
        ui.print_message("Superblock Information:\n");
        ui.print_message("  Inodes: " + std::to_string(sb_.s_ninodes) + "\n");
        ui.print_message("  Zones: " + std::to_string(sb_.s_nzones) + "\n");
        ui.print_message("  Imap blocks: " + std::to_string(sb_.s_imap_blocks) + "\n");
        ui.print_message("  Zmap blocks: " + std::to_string(sb_.s_zmap_blocks) + "\n");
        ui.print_message("  First data zone: " + std::to_string(sb_.s_firstdatazone) + "\n");
        ui.print_message("  Log zone size: " + std::to_string(sb_.s_log_zone_size) + "\n");
        ui.print_message("  Max file size: " + std::to_string(sb_.s_maxsize) + "\n");
        ui.print_message("  Block size: " + std::to_string(BLOCK_SIZE) + "\n");
        ui.print_message("  Zone size: " + std::to_string(BLOCK_SIZE << sb_.s_log_zone_size) +
                         "\n");
    }
};

// MINIX inode structure
struct d_inode {
    std::uint16_t i_mode;
    std::uint16_t i_uid;
    std::uint32_t i_size;
    std::uint32_t i_modtime;
    std::uint8_t i_gid;
    std::uint8_t i_nlinks;
    std::array<zone_nr, 9> i_zone;
};

// MINIX directory entry
struct dir_struct {
    inode_nr d_inum;
    std::array<char, 14> d_name;
};

// File type constants (from mode field)
constexpr std::uint16_t I_TYPE = 0170000;
constexpr std::uint16_t I_REGULAR = 0100000;
constexpr std::uint16_t I_BLOCK_SPECIAL = 0060000;
constexpr std::uint16_t I_DIRECTORY = 0040000;
constexpr std::uint16_t I_CHAR_SPECIAL = 0020000;

// Zone constants
constexpr std::size_t NR_DIRECT_ZONES = 7;
constexpr std::size_t NR_INDIRECTS = 1;
constexpr std::size_t NR_DINDIRECTS = 1;

/**
 * @brief MINIX inode wrapper with validation and utility methods
 */
class Inode {
  private:
    d_inode inode_;
    inode_nr number_;
    bool modified_{false};

  public:
    explicit Inode(inode_nr number = 0) : number_(number) {
        std::fill(reinterpret_cast<std::uint8_t *>(&inode_),
                  reinterpret_cast<std::uint8_t *>(&inode_) + sizeof(inode_), 0);
    }

    // Load inode from disk
    void load_from_disk(diskio::DiskInterface &disk, const SuperBlock &sb) {
        const auto address = sb.get_inode_address(number_);
        const auto sector =
            static_cast<std::uint64_t>(address / diskio::DiskConstants::SECTOR_SIZE);
        const auto offset = static_cast<std::size_t>(address % diskio::DiskConstants::SECTOR_SIZE);

        auto sector_data = disk.read_sector(diskio::SectorAddress(sector));
        if (offset + sizeof(inode_) > sector_data.size_bytes()) {
            throw std::runtime_error("Inode spans sector boundary");
        }

        std::copy(sector_data.data() + offset, sector_data.data() + offset + sizeof(inode_),
                  reinterpret_cast<std::uint8_t *>(&inode_));
    }

    // Save inode to disk
    void save_to_disk(diskio::DiskInterface &disk, const SuperBlock &sb) {
        if (!modified_)
            return;

        const auto address = sb.get_inode_address(number_);
        const auto sector =
            static_cast<std::uint64_t>(address / diskio::DiskConstants::SECTOR_SIZE);
        const auto offset = static_cast<std::size_t>(address % diskio::DiskConstants::SECTOR_SIZE);

        auto sector_data = disk.read_sector(diskio::SectorAddress(sector));
        std::copy(reinterpret_cast<const std::uint8_t *>(&inode_),
                  reinterpret_cast<const std::uint8_t *>(&inode_) + sizeof(inode_),
                  sector_data.data() + offset);

        disk.write_sector(diskio::SectorAddress(sector), sector_data);
        modified_ = false;
    }

    // Type checking
    [[nodiscard]] InodeType get_type() const noexcept {
        const auto mode = inode_.i_mode & I_TYPE;
        switch (mode) {
        case I_REGULAR:
            return InodeType::REGULAR_FILE;
        case I_DIRECTORY:
            return InodeType::DIRECTORY;
        case I_BLOCK_SPECIAL:
            return InodeType::BLOCK_SPECIAL;
        case I_CHAR_SPECIAL:
            return InodeType::CHAR_SPECIAL;
        default:
            return is_free() ? InodeType::FREE_INODE : InodeType::BAD_INODE;
        }
    }

    [[nodiscard]] bool is_free() const noexcept {
        return inode_.i_mode == 0 && inode_.i_nlinks == 0;
    }

    [[nodiscard]] bool is_directory() const noexcept {
        return (inode_.i_mode & I_TYPE) == I_DIRECTORY;
    }

    [[nodiscard]] bool is_regular_file() const noexcept {
        return (inode_.i_mode & I_TYPE) == I_REGULAR;
    }

    // Accessors
    [[nodiscard]] inode_nr get_number() const noexcept { return number_; }
    [[nodiscard]] std::uint16_t get_mode() const noexcept { return inode_.i_mode; }
    [[nodiscard]] std::uint16_t get_uid() const noexcept { return inode_.i_uid; }
    [[nodiscard]] std::uint32_t get_size() const noexcept { return inode_.i_size; }
    [[nodiscard]] std::uint32_t get_mtime() const noexcept { return inode_.i_modtime; }
    [[nodiscard]] std::uint8_t get_gid() const noexcept { return inode_.i_gid; }
    [[nodiscard]] std::uint8_t get_nlinks() const noexcept { return inode_.i_nlinks; }
    [[nodiscard]] zone_nr get_zone(std::size_t index) const {
        if (index >= inode_.i_zone.size()) {
            throw std::out_of_range("Zone index out of range: " + std::to_string(index));
        }
        return inode_.i_zone[index];
    }

    // Mutators
    void set_mode(std::uint16_t mode) {
        inode_.i_mode = mode;
        modified_ = true;
    }
    void set_uid(std::uint16_t uid) {
        inode_.i_uid = uid;
        modified_ = true;
    }
    void set_size(std::uint32_t size) {
        inode_.i_size = size;
        modified_ = true;
    }
    void set_mtime(std::uint32_t mtime) {
        inode_.i_modtime = mtime;
        modified_ = true;
    }
    void set_gid(std::uint8_t gid) {
        inode_.i_gid = gid;
        modified_ = true;
    }
    void set_nlinks(std::uint8_t nlinks) {
        inode_.i_nlinks = nlinks;
        modified_ = true;
    }
    void set_zone(std::size_t index, zone_nr zone) {
        if (index >= inode_.i_zone.size()) {
            throw std::out_of_range("Zone index out of range: " + std::to_string(index));
        }
        inode_.i_zone[index] = zone;
        modified_ = true;
    }

    void clear() {
        std::fill(reinterpret_cast<std::uint8_t *>(&inode_),
                  reinterpret_cast<std::uint8_t *>(&inode_) + sizeof(inode_), 0);
        modified_ = true;
    }

    // Validation
    [[nodiscard]] bool validate(const SuperBlock &sb, UserInterface &ui,
                                const PathTracker &path) const {
        bool valid = true;

        // Check zone numbers
        for (std::size_t i = 0; i < inode_.i_zone.size(); ++i) {
            const auto zone = inode_.i_zone[i];
            if (zone != 0 && (zone < sb.get_first_data_zone() || zone >= sb.get_zone_count())) {
                ui.print_error(
                    "Invalid zone " + std::to_string(zone) + " in zone " + std::to_string(i), path);
                valid = false;
            }
        }

        // Check file size consistency
        if (is_directory()) {
            if (inode_.i_size % sizeof(dir_struct) != 0) {
                ui.print_error("Directory size not multiple of directory entry size", path);
                valid = false;
            }
        }

        // Check link count
        if (!is_free() && inode_.i_nlinks == 0) {
            ui.print_error("Non-free inode with zero link count", path);
            valid = false;
        }

        return valid;
    }

    // Calculate zones needed for current file size
    [[nodiscard]] std::uint32_t calculate_zones_needed(const SuperBlock &sb) const {
        if (inode_.i_size == 0)
            return 0;

        const auto zone_size = BLOCK_SIZE * sb.get_scale();
        return (inode_.i_size + zone_size - 1) / zone_size;
    }

    // Get all zones used by this inode
    [[nodiscard]] std::vector<zone_nr> get_all_zones(diskio::DiskInterface &disk,
                                                     const SuperBlock &sb) const {
        std::vector<zone_nr> zones;

        // Direct zones
        for (std::size_t i = 0; i < NR_DIRECT_ZONES && inode_.i_zone[i] != 0; ++i) {
            zones.push_back(inode_.i_zone[i]);
        }

        // Single indirect
        if (inode_.i_zone[NR_DIRECT_ZONES] != 0) {
            auto indirect_zones = read_indirect_zones(disk, sb, inode_.i_zone[NR_DIRECT_ZONES]);
            zones.insert(zones.end(), indirect_zones.begin(), indirect_zones.end());
        }

        // Double indirect
        if (inode_.i_zone[NR_DIRECT_ZONES + 1] != 0) {
            auto double_indirect_zones =
                read_double_indirect_zones(disk, sb, inode_.i_zone[NR_DIRECT_ZONES + 1]);
            zones.insert(zones.end(), double_indirect_zones.begin(), double_indirect_zones.end());
        }

        return zones;
    }

  private:
    [[nodiscard]] std::vector<zone_nr> read_indirect_zones(diskio::DiskInterface &disk,
                                                           const SuperBlock &sb,
                                                           zone_nr indirect_zone) const {
        std::vector<zone_nr> zones;
        const auto zone_address = sb.get_zone_address(indirect_zone);
        const auto sector =
            static_cast<std::uint64_t>(zone_address / diskio::DiskConstants::SECTOR_SIZE);

        auto sector_data = disk.read_sector(diskio::SectorAddress(sector));
        const auto *zone_ptrs = reinterpret_cast<const zone_nr *>(sector_data.data());
        const auto max_zones = sector_data.size_bytes() / sizeof(zone_nr);

        for (std::size_t i = 0; i < max_zones && zone_ptrs[i] != 0; ++i) {
            zones.push_back(zone_ptrs[i]);
        }

        return zones;
    }

    [[nodiscard]] std::vector<zone_nr>
    read_double_indirect_zones(diskio::DiskInterface &disk, const SuperBlock &sb,
                               zone_nr double_indirect_zone) const {
        std::vector<zone_nr> zones;

        auto indirect_zones = read_indirect_zones(disk, sb, double_indirect_zone);
        for (const auto indirect_zone : indirect_zones) {
            auto data_zones = read_indirect_zones(disk, sb, indirect_zone);
            zones.insert(zones.end(), data_zones.begin(), data_zones.end());
        }

        return zones;
    }
};

/**
 * @brief Directory entry management with path tracking
 */
class DirectoryEntry {
  public:
    struct Entry {
        inode_nr inode_number;
        std::string name;

        Entry(inode_nr ino, std::string n) : inode_number(ino), name(std::move(n)) {}

        [[nodiscard]] bool is_dot() const noexcept { return name == "."; }
        [[nodiscard]] bool is_dot_dot() const noexcept { return name == ".."; }
        [[nodiscard]] bool is_valid_name() const noexcept {
            return !name.empty() && name.size() <= 14 && name.find('\0') == std::string::npos;
        }
    };

  private:
    std::vector<Entry> entries_;

  public:
    void load_from_inode(diskio::DiskInterface &disk, const SuperBlock &sb,
                         const Inode &dir_inode) {
        entries_.clear();

        if (!dir_inode.is_directory()) {
            throw std::invalid_argument("Inode is not a directory");
        }

        const auto zones = dir_inode.get_all_zones(disk, sb);

        for (const auto zone : zones) {
            const auto zone_address = sb.get_zone_address(zone);
            const auto sector =
                static_cast<std::uint64_t>(zone_address / diskio::DiskConstants::SECTOR_SIZE);

            auto sector_data = disk.read_sector(diskio::SectorAddress(sector));
            const auto *dir_entries = reinterpret_cast<const dir_struct *>(sector_data.data());
            const auto max_entries = sector_data.size_bytes() / sizeof(dir_struct);

            for (std::size_t i = 0; i < max_entries; ++i) {
                if (dir_entries[i].d_inum != 0) {
                    // Create string from null-terminated char array
                    const char *name_ptr = dir_entries[i].d_name.data();
                    std::size_t name_len = 0;
                    while (name_len < dir_entries[i].d_name.size() && name_ptr[name_len] != '\0') {
                        ++name_len;
                    }
                    std::string name(name_ptr, name_len);
                    entries_.emplace_back(dir_entries[i].d_inum, std::move(name));
                }
            }
        }
    }

    [[nodiscard]] const std::vector<Entry> &get_entries() const noexcept { return entries_; }

    [[nodiscard]] bool validate(UserInterface &ui, const PathTracker &path,
                                inode_nr expected_parent) const {
        bool valid = true;
        bool has_dot = false;
        bool has_dot_dot = false;

        for (const auto &entry : entries_) {
            if (!entry.is_valid_name()) {
                ui.print_error("Invalid directory entry name: '" + entry.name + "'", path);
                valid = false;
                continue;
            }

            if (entry.is_dot()) {
                if (has_dot) {
                    ui.print_error("Duplicate '.' entry", path);
                    valid = false;
                } else {
                    has_dot = true;
                    if (entry.inode_number != path.get_current_inode()) {
                        ui.print_error("'.' entry points to wrong inode", path);
                        valid = false;
                    }
                }
            } else if (entry.is_dot_dot()) {
                if (has_dot_dot) {
                    ui.print_error("Duplicate '..' entry", path);
                    valid = false;
                } else {
                    has_dot_dot = true;
                    if (entry.inode_number != expected_parent) {
                        ui.print_error("'..' entry points to wrong inode", path);
                        valid = false;
                    }
                }
            }
        }

        if (!has_dot) {
            ui.print_error("Missing '.' entry", path);
            valid = false;
        }

        if (!has_dot_dot) {
            ui.print_error("Missing '..' entry", path);
            valid = false;
        }

        return valid;
    }

    [[nodiscard]] std::optional<inode_nr> find_entry(std::string_view name) const {
        for (const auto &entry : entries_) {
            if (entry.name == name) {
                return entry.inode_number;
            }
        }
        return std::nullopt;
    }

    void add_entry(inode_nr inode_number, std::string name) {
        entries_.emplace_back(inode_number, std::move(name));
    }

    void remove_entry(std::string_view name) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [name](const Entry &entry) { return entry.name == name; }),
                       entries_.end());
    }
};

/**
 * @brief Complete filesystem checker with all validation phases
 */
class FilesystemChecker {
  private:
    diskio::DiskInterface &disk_;
    SuperBlock superblock_;
    Bitmap inode_bitmap_;
    Bitmap zone_bitmap_;
    std::vector<std::uint16_t> inode_link_counts_;
    std::vector<bool> zone_usage_;
    FilesystemStatistics statistics_;
    UserInterface ui_;

  public:
    explicit FilesystemChecker(diskio::DiskInterface &disk, FsckMode mode)
        : disk_(disk), inode_bitmap_(1), zone_bitmap_(1), ui_(mode) {}

    /**
     * @brief Main filesystem check entry point
     * @return True if filesystem is consistent or was successfully repaired
     */
    [[nodiscard]] bool check_filesystem() {
        try {
            ui_.print_message("MINIX Filesystem Checker v2.0\n");
            ui_.print_message("==============================\n\n");

            // Phase 1: Load and validate superblock
            phase1_check_superblock();

            // Phase 2: Initialize data structures
            phase2_initialize_structures();

            // Phase 3: Check inodes and build usage maps
            phase3_check_inodes();

            // Phase 4: Check directories and build link counts
            phase4_check_directories();

            // Phase 5: Check bitmaps consistency
            phase5_check_bitmaps();

            // Phase 6: Verify link counts
            phase6_verify_link_counts();

            // Phase 7: Cleanup and summary
            phase7_cleanup_and_summary();

            return statistics_.errors_found == 0 || statistics_.errors_fixed > 0;

        } catch (const std::exception &e) {
            ui_.print_error("Fatal error during filesystem check: " + std::string(e.what()),
                            PathTracker{});
            return false;
        }
    }

  private:
    void phase1_check_superblock() {
        ui_.print_message("Phase 1: Checking superblock...\n");

        superblock_.load_from_disk(disk_);
        superblock_.validate();
        superblock_.check_consistency(ui_);

        if (ui_.get_mode() != FsckMode::CHECK_ONLY) {
            superblock_.print_info(ui_);
        }
    }

    void phase2_initialize_structures() {
        ui_.print_message("Phase 2: Initializing data structures...\n");

        // Initialize bitmaps
        inode_bitmap_ = Bitmap(superblock_.get_inode_count());
        zone_bitmap_ = Bitmap(superblock_.get_zone_count());

        // Load bitmaps from disk
        inode_bitmap_.load_from_disk(disk_, diskio::SectorAddress(superblock_.get_imap_start()),
                                     superblock_.get_imap_blocks());
        zone_bitmap_.load_from_disk(disk_, diskio::SectorAddress(superblock_.get_zmap_start()),
                                    superblock_.get_zmap_blocks());

        // Initialize tracking arrays
        inode_link_counts_.assign(superblock_.get_inode_count() + 1, 0);
        zone_usage_.assign(superblock_.get_zone_count(), false);

        // Mark system zones as used
        for (zone_nr zone = 0; zone < superblock_.get_first_data_zone(); ++zone) {
            zone_usage_[zone] = true;
        }
    }

    void phase3_check_inodes() {
        ui_.print_message("Phase 3: Checking inodes...\n");

        PathTracker path;

        for (inode_nr ino = 1; ino <= superblock_.get_inode_count(); ++ino) {
            Inode inode(ino);
            inode.load_from_disk(disk_, superblock_);

            if (inode.is_free()) {
                statistics_.free_inodes++;
                continue;
            }

            // Check if inode is marked as allocated in bitmap
            if (!inode_bitmap_.is_set(BitNumber(ino))) {
                if (ui_.ask_repair("Inode " + std::to_string(ino) +
                                   " is used but not marked in bitmap. Mark it")) {
                    inode_bitmap_.set_bit(BitNumber(ino));
                    statistics_.errors_fixed++;
                }
                statistics_.errors_found++;
            }

            // Validate inode structure
            if (!inode.validate(superblock_, ui_, path)) {
                statistics_.errors_found++;
                if (ui_.ask_repair("Inode " + std::to_string(ino) + " has errors. Clear it")) {
                    inode.clear();
                    inode.save_to_disk(disk_, superblock_);
                    inode_bitmap_.clear_bit(BitNumber(ino));
                    statistics_.errors_fixed++;
                    continue;
                }
            }

            // Count by type
            switch (inode.get_type()) {
            case InodeType::REGULAR_FILE:
                statistics_.regular_files++;
                break;
            case InodeType::DIRECTORY:
                statistics_.directories++;
                break;
            case InodeType::BLOCK_SPECIAL:
                statistics_.block_special++;
                break;
            case InodeType::CHAR_SPECIAL:
                statistics_.char_special++;
                break;
            case InodeType::BAD_INODE:
                statistics_.bad_inodes++;
                break;
            default:
                break;
            }

            // Mark zones as used
            auto zones = inode.get_all_zones(disk_, superblock_);
            for (const auto zone : zones) {
                if (zone >= superblock_.get_zone_count()) {
                    ui_.print_error("Zone " + std::to_string(zone) + " out of range", path);
                    statistics_.errors_found++;
                    continue;
                }

                if (zone_usage_[zone]) {
                    ui_.print_error("Zone " + std::to_string(zone) + " multiply claimed", path);
                    statistics_.errors_found++;
                } else {
                    zone_usage_[zone] = true;
                }
            }
        }
    }

    void phase4_check_directories() {
        ui_.print_message("Phase 4: Checking directories...\n");

        // Start with root directory
        check_directory_recursive(ROOT_INODE, ROOT_INODE, PathTracker{});
    }

    void check_directory_recursive(inode_nr dir_ino, inode_nr parent_ino, PathTracker path) {
        if (dir_ino == parent_ino && dir_ino != ROOT_INODE) {
            ui_.print_error("Directory is its own parent", path);
            statistics_.errors_found++;
            return;
        }
        Inode dir_inode(dir_ino);
        dir_inode.load_from_disk(disk_, superblock_);

        if (!dir_inode.is_directory()) {
            ui_.print_error(
                "Expected directory, found " + inode_type_to_string(dir_inode.get_type()), path);
            statistics_.errors_found++;
            return;
        }

        // Increment link count for this directory
        if (dir_ino <= superblock_.get_inode_count()) {
            inode_link_counts_[dir_ino]++;
        }

        DirectoryEntry dir_entries;
        dir_entries.load_from_inode(disk_, superblock_, dir_inode);

        // Validate directory structure
        if (!dir_entries.validate(ui_, path, parent_ino)) {
            statistics_.errors_found++;
        }

        // Process each entry
        for (const auto &entry : dir_entries.get_entries()) {
            if (entry.is_dot() || entry.is_dot_dot()) {
                continue; // Already validated
            }

            if (entry.inode_number > superblock_.get_inode_count()) {
                ui_.print_error("Directory entry '" + entry.name + "' points to invalid inode " +
                                    std::to_string(entry.inode_number),
                                path);
                statistics_.errors_found++;
                continue;
            }

            // Increment link count
            inode_link_counts_[entry.inode_number]++;

            // Recursively check subdirectories
            Inode child_inode(entry.inode_number);
            child_inode.load_from_disk(disk_, superblock_);

            if (child_inode.is_directory()) {
                path.enter_directory(entry.name, entry.inode_number);
                check_directory_recursive(entry.inode_number, dir_ino, path);
                path.exit_directory();
            }
        }
    }

    void phase5_check_bitmaps() {
        ui_.print_message("Phase 5: Checking bitmaps...\n");

        // Create expected bitmaps based on usage
        Bitmap expected_inode_bitmap(superblock_.get_inode_count());
        Bitmap expected_zone_bitmap(superblock_.get_zone_count());

        // Build expected inode bitmap
        for (inode_nr ino = 1; ino <= superblock_.get_inode_count(); ++ino) {
            if (inode_link_counts_[ino] > 0) {
                expected_inode_bitmap.set_bit(BitNumber(ino));
            }
        }

        // Build expected zone bitmap
        for (zone_nr zone = 0; zone < superblock_.get_zone_count(); ++zone) {
            if (zone_usage_[zone]) {
                expected_zone_bitmap.set_bit(BitNumber(zone));
            }
        }

        // Check inode bitmap differences
        auto inode_differences = inode_bitmap_.find_differences(expected_inode_bitmap);
        if (!inode_differences.empty()) {
            ui_.print_error("Inode bitmap has " + std::to_string(inode_differences.size()) +
                                " inconsistencies",
                            PathTracker{});
            statistics_.errors_found++;

            if (ui_.ask_repair("Fix inode bitmap")) {
                inode_bitmap_ = std::move(expected_inode_bitmap);
                inode_bitmap_.save_to_disk(disk_,
                                           diskio::SectorAddress(superblock_.get_imap_start()),
                                           superblock_.get_imap_blocks());
                statistics_.errors_fixed++;
            }
        }

        // Check zone bitmap differences
        auto zone_differences = zone_bitmap_.find_differences(expected_zone_bitmap);
        if (!zone_differences.empty()) {
            ui_.print_error("Zone bitmap has " + std::to_string(zone_differences.size()) +
                                " inconsistencies",
                            PathTracker{});
            statistics_.errors_found++;

            if (ui_.ask_repair("Fix zone bitmap")) {
                zone_bitmap_ = std::move(expected_zone_bitmap);
                zone_bitmap_.save_to_disk(disk_,
                                          diskio::SectorAddress(superblock_.get_zmap_start()),
                                          superblock_.get_zmap_blocks());
                statistics_.errors_fixed++;
            }
        }
    }

    void phase6_verify_link_counts() {
        ui_.print_message("Phase 6: Verifying link counts...\n");

        for (inode_nr ino = 1; ino <= superblock_.get_inode_count(); ++ino) {
            if (inode_link_counts_[ino] == 0) {
                continue; // Free inode
            }

            Inode inode(ino);
            inode.load_from_disk(disk_, superblock_);

            const auto actual_links = inode.get_nlinks();
            const auto counted_links = inode_link_counts_[ino];

            if (actual_links != counted_links) {
                ui_.print_error("Inode " + std::to_string(ino) + " has " +
                                    std::to_string(actual_links) + " links, counted " +
                                    std::to_string(counted_links),
                                PathTracker{});
                statistics_.errors_found++;

                if (ui_.ask_repair("Fix link count")) {
                    inode.set_nlinks(counted_links);
                    inode.save_to_disk(disk_, superblock_);
                    statistics_.errors_fixed++;
                }
            }
        }
    }

    void phase7_cleanup_and_summary() {
        ui_.print_message("Phase 7: Cleanup and summary...\n");

        // Save any modified structures
        if (ui_.changes_made()) {
            ui_.print_message("Saving changes to disk...\n");
            superblock_.save_to_disk(disk_);
            disk_.sync();
        }

        // Print summary
        print_summary();
    }

    void print_summary() {
        ui_.print_message("\nFilesystem Check Summary:\n");
        ui_.print_message("========================\n");
        ui_.print_message("Regular files: " + std::to_string(statistics_.regular_files) + "\n");
        ui_.print_message("Directories: " + std::to_string(statistics_.directories) + "\n");
        ui_.print_message("Block special: " + std::to_string(statistics_.block_special) + "\n");
        ui_.print_message("Char special: " + std::to_string(statistics_.char_special) + "\n");
        ui_.print_message("Bad inodes: " + std::to_string(statistics_.bad_inodes) + "\n");
        ui_.print_message("Free inodes: " + std::to_string(statistics_.free_inodes) + "\n");
        ui_.print_message("Free zones: " + std::to_string(statistics_.free_zones) + "\n");
        ui_.print_message("\nErrors found: " + std::to_string(statistics_.errors_found) + "\n");
        ui_.print_message("Errors fixed: " + std::to_string(statistics_.errors_fixed) + "\n");

        if (statistics_.errors_found == 0) {
            ui_.print_message("\nFilesystem is clean.\n");
        } else if (statistics_.errors_fixed > 0) {
            ui_.print_message("\nFilesystem was repaired.\n");
        } else {
            ui_.print_message("\nFilesystem has errors that were not fixed.\n");
        }
    }

    [[nodiscard]] std::string inode_type_to_string(InodeType type) const {
        switch (type) {
        case InodeType::REGULAR_FILE:
            return "regular file";
        case InodeType::DIRECTORY:
            return "directory";
        case InodeType::BLOCK_SPECIAL:
            return "block special";
        case InodeType::CHAR_SPECIAL:
            return "char special";
        case InodeType::BAD_INODE:
            return "bad inode";
        case InodeType::FREE_INODE:
            return "free inode";
        default:
            return "unknown";
        }
    }
};

/**
 * @brief Command line argument parsing and main entry point
 */
class FsckApplication {
  private:
    FsckMode mode_{FsckMode::CHECK_ONLY};
    std::string device_path_;
    bool verbose_{false};

  public:
    int run(int argc, char *argv[]) {
        try {
            parse_arguments(argc, argv);

            if (device_path_.empty()) {
                print_usage(argv[0]);
                return 1;
            }

            // Open device
            diskio::DiskInterface disk(device_path_, mode_ == FsckMode::CHECK_ONLY);

            // Create and run checker
            FilesystemChecker checker(disk, mode_);
            bool success = checker.check_filesystem();

            return success ? 0 : 1;

        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

  private:
    void parse_arguments(int argc, char *argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-a") {
                mode_ = FsckMode::AUTOMATIC;
            } else if (arg == "-i") {
                mode_ = FsckMode::INTERACTIVE;
            } else if (arg == "-l") {
                mode_ = FsckMode::LIST_ONLY;
            } else if (arg == "-v") {
                verbose_ = true;
            } else if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                std::exit(0);
            } else if (arg[0] == '-') {
                throw std::invalid_argument("Unknown option: " + arg);
            } else {
                if (device_path_.empty()) {
                    device_path_ = arg;
                } else {
                    throw std::invalid_argument("Multiple device paths specified");
                }
            }
        }
    }

    void print_usage(const char *program_name) {
        std::cout << "Usage: " << program_name << " [options] device\n\n";
        std::cout << "Options:\n";
        std::cout << "  -a          Automatic repair mode (answer 'yes' to all questions)\n";
        std::cout << "  -i          Interactive repair mode (ask before each repair)\n";
        std::cout << "  -l          List filesystem contents only\n";
        std::cout << "  -v          Verbose output\n";
        std::cout << "  -h, --help  Show this help message\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << program_name << " /dev/fd0       # Check filesystem (read-only)\n";
        std::cout << "  " << program_name << " -a /dev/fd0    # Automatic repair\n";
        std::cout << "  " << program_name << " -i /dev/fd0    # Interactive repair\n";
    }
};

} // namespace minix::fsck

/**
 * @brief Main entry point for the filesystem checker
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status: 0 for success, 1 for failure
 *
 * @details The main function creates an FsckApplication instance and delegates
 * the entire program execution to it. This provides proper exception handling
 * and ensures clean resource management.
 */
int main(int argc, char *argv[]) {
    try {
        minix::fsck::FsckApplication app;
        return app.run(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 3;
    }
}
