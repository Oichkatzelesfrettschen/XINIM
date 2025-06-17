#pragma once

/**
 * @file inode.hpp
 * @brief C++23 modernized inode management with enhanced safety and semantics
 */

#include "buffer.hpp"
#include "const.hpp"
#include "extent.hpp"
#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace minix::fs {
/**
 * @enum InodeError
 * @brief Error conditions for inode operations
 */

enum class InodeError {
    TableFull,
    NotFound,
    AlreadyExists,
    PermissionDenied,
    InvalidOperation,
    DiskError,
    CorruptedData,
    OutOfSpace,
    InvalidArgument,
    ResourceBusy
};

/**
 * @concept FileType
 * @brief Concept defining valid file types
 */
template <typename T>
concept FileType = requires {
    std::is_enum_v<T>;
    T::Regular;
    T::Directory;
    T::CharSpecial;
    T::BlockSpecial;
    T::Pipe;
};
/**
 * @enum InodeType
 * @brief Modern file type enumeration with enhanced semantics
 */

enum class InodeType : mask_bits {
    Regular = std::to_underlying(FileTypes::kRegular),
    Directory = std::to_underlying(FileTypes::kDirectory),
    CharSpecial = std::to_underlying(FileTypes::kCharSpecial),
    BlockSpecial = std::to_underlying(FileTypes::kBlockSpecial),
    Pipe = std::to_underlying(FileTypes::kPipe)
};
/**
 * @class Permissions
 * @brief Permission set with type-safe operations
 */

class Permissions {
  private:
    mask_bits bits_;

  public:
    constexpr explicit Permissions(mask_bits bits = mask_bits{0}) : bits_{bits} {}

    [[nodiscard]] static constexpr auto owner_read_write() -> Permissions {
        return Permissions{mask_bits{0600}};
    }

    [[nodiscard]] static constexpr auto owner_all() -> Permissions {
        return Permissions{mask_bits{0700}};
    }

    [[nodiscard]] static constexpr auto all_read() -> Permissions {
        return Permissions{mask_bits{0444}};
    }

    [[nodiscard]] static constexpr auto all_read_write() -> Permissions {
        return Permissions{mask_bits{0666}};
    }

    [[nodiscard]] constexpr bool can_read(bool owner, bool group) const noexcept {
        const auto shift = owner ? 6 : (group ? 3 : 0);
        return (std::to_underlying(bits_) >> shift) & std::to_underlying(Permissions::kReadBit);
    }

    [[nodiscard]] constexpr bool can_write(bool owner, bool group) const noexcept {
        const auto shift = owner ? 6 : (group ? 3 : 0);
        return (std::to_underlying(bits_) >> shift) & std::to_underlying(Permissions::kWriteBit);
    }

    [[nodiscard]] constexpr bool can_execute(bool owner, bool group) const noexcept {
        const auto shift = owner ? 6 : (group ? 3 : 0);
        return (std::to_underlying(bits_) >> shift) & std::to_underlying(Permissions::kExecBit);
    }

    [[nodiscard]] constexpr auto operator|(const Permissions &other) const -> Permissions {
        return Permissions{mask_bits{std::to_underlying(bits_) | std::to_underlying(other.bits_)}};
    }

    [[nodiscard]] constexpr auto operator&(const Permissions &other) const -> Permissions {
        return Permissions{mask_bits{std::to_underlying(bits_) & std::to_underlying(other.bits_)}};
    }

    [[nodiscard]] constexpr auto raw() const noexcept -> mask_bits { return bits_; }
};
/**
 * @class FileTime
 * @brief Modern time management for inodes
 */

class FileTime {
  private:
    std::chrono::system_clock::time_point time_point_;

  public:
    FileTime() : time_point_{std::chrono::system_clock::now()} {}

    explicit FileTime(real_time legacy_time)
        : time_point_{std::chrono::system_clock::from_time_t(
              static_cast<std::time_t>(std::to_underlying(legacy_time)))} {}

    [[nodiscard]] auto to_legacy() const -> real_time {
        return real_time{std::chrono::system_clock::to_time_t(time_point_)};
    }

    [[nodiscard]] auto time_point() const -> std::chrono::system_clock::time_point {
        return time_point_;
    }

    void update() { time_point_ = std::chrono::system_clock::now(); }

    [[nodiscard]] auto operator<=>(const FileTime &) const = default;
};

class InodeTable;

/**
 * @class InodeHandle
 * @brief RAII inode handle with automatic reference management
 */
class InodeHandle {
  private:
    class Inode *inode_;
    bool owned_;

  public:
    explicit InodeHandle(Inode *inode = nullptr, bool owned = true)
        : inode_{inode}, owned_{owned} {}

    ~InodeHandle();

    InodeHandle(const InodeHandle &) = delete;
    InodeHandle &operator=(const InodeHandle &) = delete;

    InodeHandle(InodeHandle &&other) noexcept
        : inode_{std::exchange(other.inode_, nullptr)}, owned_{std::exchange(other.owned_, false)} {
    }

    InodeHandle &operator=(InodeHandle &&other) noexcept {
        if (this != &other) {
            reset();
            inode_ = std::exchange(other.inode_, nullptr);
            owned_ = std::exchange(other.owned_, false);
        }
        return *this;
    }

    [[nodiscard]] auto get() -> Inode * { return inode_; }
    [[nodiscard]] auto get() const -> const Inode * { return inode_; }

    [[nodiscard]] auto operator->() -> Inode * { return inode_; }
    [[nodiscard]] auto operator->() const -> const Inode * { return inode_; }

    [[nodiscard]] auto operator*() -> Inode & { return *inode_; }
    [[nodiscard]] auto operator*() const -> const Inode & { return *inode_; }

    [[nodiscard]] bool valid() const noexcept { return inode_ != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    void reset();
    [[nodiscard]] auto release() -> Inode *;
};
/**
 * @class Inode
 * @brief Modern inode with enhanced type safety and functionality
 */

class Inode {
  private:
    friend class InodeTable;
    friend class InodeHandle;

    InodeType type_{InodeType::Regular};
    uid owner_{uid{0}};
    file_pos size_{file_pos{0}};
    file_pos64 size64_{file_pos64{0}};
    FileTime modification_time_{};
    gid group_{gid{0}};
    links link_count_{links{1}};
    std::array<zone_nr, DefaultFsConstants::kNrZoneNums> zones_{};

    dev_nr device_{kNoDev};
    inode_nr number_{kNoInode};
    std::atomic<std::int32_t> reference_count_{0};
    std::atomic<bool> dirty_{false};
    std::atomic<bool> pipe_{false};
    std::atomic<bool> mounted_{false};
    std::atomic<bool> seek_flag_{false};

    std::unique_ptr<ExtentTable> extents_;

  public:
    Inode() = default;

    explicit Inode(InodeType type, uid owner, gid group)
        : type_{type}, owner_{owner}, group_{group} {
        modification_time_.update();
        std::fill(zones_.begin(), zones_.end(), kNoZone);
    }

    [[nodiscard]] auto type() const noexcept -> InodeType { return type_; }

    [[nodiscard]] constexpr bool is_regular_file() const noexcept {
        return type_ == InodeType::Regular;
    }

    [[nodiscard]] constexpr bool is_directory() const noexcept {
        return type_ == InodeType::Directory;
    }

    [[nodiscard]] constexpr bool is_special_file() const noexcept {
        return type_ == InodeType::CharSpecial || type_ == InodeType::BlockSpecial;
    }

    [[nodiscard]] constexpr bool is_pipe() const noexcept {
        return pipe_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto owner() const noexcept -> uid { return owner_; }
    [[nodiscard]] auto group() const noexcept -> gid { return group_; }

    void set_owner(uid new_owner) noexcept {
        owner_ = new_owner;
        mark_dirty();
    }

    void set_group(gid new_group) noexcept {
        group_ = new_group;
        mark_dirty();
    }

    [[nodiscard]] auto size() const noexcept -> file_pos64 {
        return size64_ != file_pos64{0} ? size64_ : file_pos64{std::to_underlying(size_)};
    }

    void set_size(file_pos64 new_size) noexcept {
        size64_ = new_size;
        size_ = file_pos{static_cast<std::int32_t>(std::to_underlying(new_size))};
        modification_time_.update();
        mark_dirty();
    }

    [[nodiscard]] auto link_count() const noexcept -> links { return link_count_; }

    void increment_links() noexcept {
        link_count_ = links{std::to_underlying(link_count_) + 1};
        mark_dirty();
    }

    void decrement_links() noexcept {
        if (std::to_underlying(link_count_) > 0) {
            link_count_ = links{std::to_underlying(link_count_) - 1};
            mark_dirty();
        }
    }

    [[nodiscard]] auto modification_time() const -> FileTime { return modification_time_; }

    void touch() {
        modification_time_.update();
        mark_dirty();
    }

    [[nodiscard]] auto zone(std::size_t index) const -> std::expected<zone_nr, InodeError> {
        if (index >= zones_.size()) {
            return std::unexpected(InodeError::InvalidArgument);
        }
        return zones_[index];
    }

    auto set_zone(std::size_t index, zone_nr zone) -> std::expected<void, InodeError> {
        if (index >= zones_.size()) {
            return std::unexpected(InodeError::InvalidArgument);
        }
        zones_[index] = zone;
        mark_dirty();
        return {};
    }

    [[nodiscard]] auto device() const noexcept -> dev_nr { return device_; }
    [[nodiscard]] auto number() const noexcept -> inode_nr { return number_; }

    [[nodiscard]] bool is_dirty() const noexcept { return dirty_.load(std::memory_order_acquire); }

    void mark_dirty() noexcept { dirty_.store(true, std::memory_order_release); }

    void mark_clean() noexcept { dirty_.store(false, std::memory_order_release); }

    [[nodiscard]] bool is_mounted() const noexcept {
        return mounted_.load(std::memory_order_acquire);
    }

    void set_mounted(bool mounted = true) noexcept {
        mounted_.store(mounted, std::memory_order_release);
    }

    [[nodiscard]] auto reference_count() const noexcept -> std::int32_t {
        return reference_count_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto check_permission(uid requesting_uid, gid requesting_gid,
                                        Permissions required) const
        -> std::expected<void, InodeError>;

    [[nodiscard]] auto extent_table() const -> const ExtentTable * { return extents_.get(); }

    auto allocate_extent_table(std::size_t initial_capacity) -> std::expected<void, InodeError>;

  private:
    void increment_references() noexcept {
        reference_count_.fetch_add(1, std::memory_order_acq_rel);
    }

    void decrement_references() noexcept {
        reference_count_.fetch_sub(1, std::memory_order_acq_rel);
    }

    void set_device_and_number(dev_nr device, inode_nr number) noexcept {
        device_ = device;
        number_ = number;
    }
};
/**
 * @class InodeTable
 * @brief Modern inode table with enhanced resource management
 */

class InodeTable {
  private:
    static constexpr std::size_t kTableSize = DefaultFsConstants::kNrInodes;

    std::array<Inode, kTableSize> inodes_{};
    std::atomic<std::size_t> inodes_in_use_{0};

    [[nodiscard]] auto find_free_slot() -> std::optional<std::size_t>;

    [[nodiscard]] auto find_inode(dev_nr device, inode_nr number) -> Inode *;

  public:
    InodeTable() = default;
    ~InodeTable() = default;

    InodeTable(const InodeTable &) = delete;
    InodeTable &operator=(const InodeTable &) = delete;
    InodeTable(InodeTable &&) = default;
    InodeTable &operator=(InodeTable &&) = default;

    [[nodiscard]] auto get_inode(dev_nr device, inode_nr number)
        -> std::expected<InodeHandle, InodeError>;

    [[nodiscard]] auto allocate_inode(dev_nr device, InodeType type, uid owner, gid group)
        -> std::expected<InodeHandle, InodeError>;

    void release_inode(Inode *inode) noexcept;

    [[nodiscard]] auto flush_dirty_inodes() -> std::size_t;

    [[nodiscard]] auto inodes_in_use() const noexcept -> std::size_t {
        return inodes_in_use_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto available_inodes() const noexcept -> std::size_t {
        return kTableSize - inodes_in_use();
    }

    [[nodiscard]] auto begin() noexcept -> Inode * { return inodes_.data(); }
    [[nodiscard]] auto end() noexcept -> Inode * { return inodes_.data() + kTableSize; }
    [[nodiscard]] auto begin() const noexcept -> const Inode * { return inodes_.data(); }
    [[nodiscard]] auto end() const noexcept -> const Inode * { return inodes_.data() + kTableSize; }
};

extern InodeTable g_inode_table;

[[nodiscard]] auto get_inode(dev_nr device, inode_nr number)
    -> std::expected<InodeHandle, InodeError>;

[[nodiscard]] auto allocate_inode(dev_nr device, InodeType type, uid owner, gid group)
    -> std::expected<InodeHandle, InodeError>;

} // namespace minix::fs
