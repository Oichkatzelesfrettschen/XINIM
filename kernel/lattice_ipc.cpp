/*
 * @file lattice_ipc.cpp
 * @brief Reference-counted message buffer for lattice IPC.
 */

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace lattice {
/**
 * @class MessageBuffer
 * @brief Shared container for IPC message bytes.
 *
 * The buffer uses reference counting so multiple threads can access the same
 * underlying storage without copying. Lifetime is automatically managed via
 * std::shared_ptr.
 */
class MessageBuffer {
  public:
    using Byte = std::byte; ///< Convenience alias for byte type

    /**
     * @brief Construct an empty MessageBuffer.
     */
    MessageBuffer() = default;

    /**
     * @brief Construct buffer of given @p size in bytes.
     * @param size Number of bytes to allocate.
     */
    explicit MessageBuffer(std::size_t size) : data_{std::make_shared<std::vector<Byte>>(size)} {}

    /**
     * @brief Obtain mutable view of the stored bytes.
     * @return Span covering the buffer contents.
     */
    [[nodiscard]] std::span<Byte> span() noexcept {
        return data_ ? std::span<Byte>{data_->data(), data_->size()} : std::span<Byte>{};
    }

    /**
     * @brief Obtain const view of the stored bytes.
     * @return Span over immutable bytes.
     */
    [[nodiscard]] std::span<const Byte> span() const noexcept {
        return data_ ? std::span<const Byte>{data_->data(), data_->size()}
                     : std::span<const Byte>{};
    }

    /**
     * @brief Number of bytes in the buffer.
     */
    [[nodiscard]] std::size_t size() const noexcept { return data_ ? data_->size() : 0U; }

    /**
     * @brief Access underlying shared pointer for advanced sharing.
     */
    [[nodiscard]] std::shared_ptr<std::vector<Byte>> share() const noexcept { return data_; }

  private:
    std::shared_ptr<std::vector<Byte>> data_{}; ///< Shared data container
};
} // namespace lattice
