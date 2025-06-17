#pragma once

/**
 * @file buffer.hpp
 * @brief C++23 modernized buffer cache with enhanced memory safety
 */

#include "const.hpp"
#include <array>
#include <atomic>
#include <concepts>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

namespace minix::fs {

class BufferManager;
class DeviceManager;

template <typename T>
concept BufferDataType = requires {
    sizeof(T) <= DefaultFsConstants::kBlockSize;
    std::is_trivially_copyable_v<T>;
    std::is_standard_layout_v<T>;
};

enum class BufferError {
    NoFreeBuffers,
    InvalidDevice,
    DiskError,
    InvalidBlockNumber,
    BufferInUse,
    CorruptedData
};

/**
 * @class BufferData
 * @brief Type-safe buffer data union
 */
class BufferData {
  private:
    alignas(std::max_align_t) std::array<std::byte, DefaultFsConstants::kBlockSize> raw_data_{};

  public:
    template <BufferDataType T> [[nodiscard]] auto as() -> std::span<T> {
        static_assert(DefaultFsConstants::kBlockSize % sizeof(T) == 0,
                      "Buffer size must be divisible by data type size");
        return std::span<T>{reinterpret_cast<T *>(raw_data_.data()),
                            DefaultFsConstants::kBlockSize / sizeof(T)};
    }
    template <BufferDataType T> [[nodiscard]] auto as() const -> std::span<const T> {
        static_assert(DefaultFsConstants::kBlockSize % sizeof(T) == 0,
                      "Buffer size must be divisible by data type size");
        return std::span<const T>{reinterpret_cast<const T *>(raw_data_.data()),
                                  DefaultFsConstants::kBlockSize / sizeof(T)};
    }

    [[nodiscard]] auto bytes() -> std::span<std::byte> { return std::span{raw_data_}; }

    [[nodiscard]] auto bytes() const -> std::span<const std::byte> { return std::span{raw_data_}; }

    void zero() noexcept { std::fill(raw_data_.begin(), raw_data_.end(), std::byte{0}); }
};

/**
 * @class BufferHandle
 * @brief RAII buffer handle with automatic lifetime management
 */
class BufferHandle {
  private:
    class Buffer *buffer_;
    bool owned_;

  public:
    explicit BufferHandle(Buffer *buf, bool owned = true) : buffer_{buf}, owned_{owned} {}
    ~BufferHandle();

    BufferHandle(const BufferHandle &) = delete;
    BufferHandle &operator=(const BufferHandle &) = delete;

    BufferHandle(BufferHandle &&other) noexcept
        : buffer_{std::exchange(other.buffer_, nullptr)},
          owned_{std::exchange(other.owned_, false)} {}

    BufferHandle &operator=(BufferHandle &&other) noexcept {
        if (this != &other) {
            if (owned_ && buffer_) {
                release();
            }
            buffer_ = std::exchange(other.buffer_, nullptr);
            owned_ = std::exchange(other.owned_, false);
        }
        return *this;
    }

    [[nodiscard]] auto get() -> Buffer * { return buffer_; }
    [[nodiscard]] auto get() const -> const Buffer * { return buffer_; }

    [[nodiscard]] auto operator->() -> Buffer * { return buffer_; }
    [[nodiscard]] auto operator->() const -> const Buffer * { return buffer_; }

    [[nodiscard]] auto operator*() -> Buffer & { return *buffer_; }
    [[nodiscard]] auto operator*() const -> const Buffer & { return *buffer_; }

    [[nodiscard]] bool valid() const noexcept { return buffer_ != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    void release();
};

/**
 * @class Buffer
 * @brief Modern buffer with enhanced type safety and atomic operations
 */
class Buffer {
  private:
    friend class BufferManager;
    friend class BufferHandle;

    BufferData data_{};
    std::atomic<std::uint32_t> reference_count_{0};
    std::atomic<BlockType> block_type_{BlockType::FullDataBlock};
    std::atomic<bool> dirty_{false};

    Buffer *next_{nullptr};
    Buffer *prev_{nullptr};
    Buffer *hash_next_{nullptr};

    block_nr block_number_{kNoBlock};
    dev_nr device_{kNoDev};

  public:
    template <BufferDataType T> [[nodiscard]] auto data_as() -> std::span<T> {
        return data_.template as<T>();
    }

    template <BufferDataType T> [[nodiscard]] auto data_as() const -> std::span<const T> {
        return data_.template as<T>();
    }

    [[nodiscard]] auto raw_data() -> std::span<std::byte> { return data_.bytes(); }

    [[nodiscard]] auto raw_data() const -> std::span<const std::byte> { return data_.bytes(); }

    [[nodiscard]] auto block_number() const noexcept -> block_nr { return block_number_; }

    [[nodiscard]] auto device() const noexcept -> dev_nr { return device_; }

    [[nodiscard]] bool is_dirty() const noexcept { return dirty_.load(std::memory_order_acquire); }

    void mark_dirty() noexcept { dirty_.store(true, std::memory_order_release); }

    void mark_clean() noexcept { dirty_.store(false, std::memory_order_release); }

    [[nodiscard]] auto get_block_type() const noexcept -> BlockType {
        return block_type_.load(std::memory_order_acquire);
    }

    void set_block_type(BlockType type) noexcept {
        block_type_.store(type, std::memory_order_release);
    }

    [[nodiscard]] auto reference_count() const noexcept -> std::uint32_t {
        return reference_count_.load(std::memory_order_acquire);
    }

    void zero_data() noexcept {
        data_.zero();
        mark_dirty();
    }

  private:
    void increment_references() noexcept {
        reference_count_.fetch_add(1, std::memory_order_acq_rel);
    }

    void decrement_references() noexcept {
        reference_count_.fetch_sub(1, std::memory_order_acq_rel);
    }
};

/**
 * @class BufferPool
 * @brief Type-safe buffer pool with enhanced resource management
 */
class BufferPool {
  private:
    static constexpr std::size_t kPoolSize = DefaultFsConstants::kNrBufs;
    static constexpr std::size_t kHashSize = DefaultFsConstants::kNrBufHash;

    std::array<Buffer, kPoolSize> buffers_{};
    std::array<Buffer *, kHashSize> hash_table_{};

    Buffer *lru_front_{nullptr};
    Buffer *lru_rear_{nullptr};

    std::atomic<std::size_t> buffers_in_use_{0};

    [[nodiscard]] static auto hash_block(block_nr block) noexcept -> std::size_t {
        return std::to_underlying(block) & (kHashSize - 1);
    }

    void move_to_front(Buffer *buffer) noexcept;
    void move_to_rear(Buffer *buffer) noexcept;
    void remove_from_lru(Buffer *buffer) noexcept;

    void add_to_hash(Buffer *buffer) noexcept;
    void remove_from_hash(Buffer *buffer) noexcept;
    [[nodiscard]] auto find_in_hash(dev_nr device, block_nr block) const noexcept -> Buffer *;

  public:
    BufferPool();
    ~BufferPool() = default;

    BufferPool(const BufferPool &) = delete;
    BufferPool &operator=(const BufferPool &) = delete;
    BufferPool(BufferPool &&) = default;
    BufferPool &operator=(BufferPool &&) = default;

    [[nodiscard]] auto get_buffer(dev_nr device, block_nr block, IoMode mode)
        -> std::expected<BufferHandle, BufferError>;

    void put_buffer(Buffer *buffer, BlockType type) noexcept;

    void invalidate_device(dev_nr device) noexcept;

    [[nodiscard]] auto flush_dirty_buffers() -> std::size_t;

    [[nodiscard]] auto buffers_in_use() const noexcept -> std::size_t {
        return buffers_in_use_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto available_buffers() const noexcept -> std::size_t {
        return kPoolSize - buffers_in_use();
    }

    [[nodiscard]] auto begin() noexcept -> Buffer * { return buffers_.data(); }
    [[nodiscard]] auto end() noexcept -> Buffer * { return buffers_.data() + kPoolSize; }
    [[nodiscard]] auto begin() const noexcept -> const Buffer * { return buffers_.data(); }
    [[nodiscard]] auto end() const noexcept -> const Buffer * {
        return buffers_.data() + kPoolSize;
    }
};

/**
 * @class BufferGuard
 * @brief RAII guard for automatic buffer release
 */
class BufferGuard {
  private:
    BufferHandle handle_;
    BlockType block_type_;

  public:
    BufferGuard(BufferHandle handle, BlockType type)
        : handle_{std::move(handle)}, block_type_{type} {}

    ~BufferGuard() {
        if (handle_.valid()) {
            release_buffer(std::move(handle_), block_type_);
        }
    }

    [[nodiscard]] auto get() -> Buffer * { return handle_.get(); }
    [[nodiscard]] auto operator->() -> Buffer * { return handle_.get(); }
    [[nodiscard]] auto operator*() -> Buffer & { return *handle_.get(); }

    [[nodiscard]] auto release() -> BufferHandle { return std::move(handle_); }

  private:
    static void release_buffer(BufferHandle handle, BlockType type);
};

} // namespace minix::fs
