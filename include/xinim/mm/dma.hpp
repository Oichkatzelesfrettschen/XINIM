/**
 * @file dma.hpp
 * @brief Compatibility DMA helpers layered on top of the active DMA allocator.
 */

#pragma once

#include <xinim/mm/dma_allocator.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xinim::mm {

struct SGEntry {
    uint64_t phys_addr;
    std::size_t length;
};

class SGList {
public:
    bool add_entry(uint64_t phys_addr, std::size_t length);
    bool add(uint64_t phys_addr, uint32_t length) {
        return add_entry(phys_addr, static_cast<std::size_t>(length));
    }

    [[nodiscard]] std::size_t count() const noexcept { return entries_.size(); }
    [[nodiscard]] const SGEntry* entries() const noexcept { return entries_.data(); }
    [[nodiscard]] SGEntry* entries() noexcept { return entries_.data(); }
    [[nodiscard]] std::size_t total_length() const noexcept;
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<SGEntry> entries_{};
};

class DMAPool {
public:
    DMAPool(std::size_t block_size, std::size_t num_blocks, DMAFlags flags = DMAFlags::CONTIGUOUS);
    ~DMAPool();

    [[nodiscard]] void* allocate(uint64_t& phys_addr);
    void free(void* ptr);

    [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return free_blocks_.size() + allocated_blocks_; }
    [[nodiscard]] std::size_t allocated() const noexcept { return allocated_blocks_; }

private:
    std::size_t block_size_{0};
    std::size_t num_blocks_{0};
    DMAFlags flags_{DMAFlags::NONE};
    DMABuffer pool_buffer_{};
    std::vector<std::size_t> free_blocks_{};
    std::size_t allocated_blocks_{0};
};

class DMAMapping {
public:
    DMAMapping(const DMABuffer& buffer, bool to_device) noexcept;
    ~DMAMapping();

    DMAMapping(const DMAMapping&) = delete;
    DMAMapping& operator=(const DMAMapping&) = delete;
    DMAMapping(DMAMapping&& other) noexcept;
    DMAMapping& operator=(DMAMapping&& other) noexcept;

    [[nodiscard]] uint64_t device_addr() const noexcept { return buffer_.physical_addr; }
    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size; }

private:
    DMABuffer buffer_{};
    bool to_device_{false};
    bool active_{false};
};

} // namespace xinim::mm
