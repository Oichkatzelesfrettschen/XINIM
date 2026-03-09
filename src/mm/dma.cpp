/**
 * @file dma.cpp
 * @brief Compatibility DMA helpers layered on top of the active allocator.
 */

#include <xinim/mm/dma.hpp>

#include <algorithm>
#include <utility>

namespace xinim::mm {

bool SGList::add_entry(uint64_t phys_addr, std::size_t length) {
    if (length == 0) {
        return false;
    }

    if (!entries_.empty()) {
        auto& last = entries_.back();
        if (last.phys_addr + last.length == phys_addr) {
            last.length += length;
            return true;
        }
    }

    entries_.push_back({phys_addr, length});
    return true;
}

std::size_t SGList::total_length() const noexcept {
    std::size_t total = 0;
    for (const auto& entry : entries_) {
        total += entry.length;
    }
    return total;
}

DMAPool::DMAPool(std::size_t block_size, std::size_t num_blocks, DMAFlags flags)
    : block_size_(std::max<std::size_t>(block_size, 64))
    , num_blocks_(num_blocks)
    , flags_(flags | DMAFlags::CONTIGUOUS)
    , pool_buffer_(DMAAllocator::allocate_aligned(block_size_ * num_blocks_, 64, flags_ | DMAFlags::ZERO)) {
    free_blocks_.reserve(num_blocks_);
    for (std::size_t index = 0; index < num_blocks_; ++index) {
        free_blocks_.push_back(num_blocks_ - index - 1);
    }
}

DMAPool::~DMAPool() {
    if (pool_buffer_.is_valid()) {
        DMAAllocator::free(pool_buffer_);
    }
}

void* DMAPool::allocate(uint64_t& phys_addr) {
    if (!pool_buffer_.is_valid() || free_blocks_.empty()) {
        phys_addr = 0;
        return nullptr;
    }

    const std::size_t block_index = free_blocks_.back();
    free_blocks_.pop_back();
    ++allocated_blocks_;

    auto* virt_base = static_cast<unsigned char*>(pool_buffer_.virtual_addr);
    phys_addr = pool_buffer_.physical_addr + (block_index * block_size_);
    return virt_base + (block_index * block_size_);
}

void DMAPool::free(void* ptr) {
    if (!pool_buffer_.is_valid() || ptr == nullptr) {
        return;
    }

    auto* virt_base = static_cast<unsigned char*>(pool_buffer_.virtual_addr);
    auto* current = static_cast<unsigned char*>(ptr);
    if (current < virt_base || current >= virt_base + pool_buffer_.size) {
        return;
    }

    const std::size_t offset = static_cast<std::size_t>(current - virt_base);
    const std::size_t block_index = offset / block_size_;
    if (block_index >= num_blocks_) {
        return;
    }

    free_blocks_.push_back(block_index);
    if (allocated_blocks_ > 0) {
        --allocated_blocks_;
    }
}

DMAMapping::DMAMapping(const DMABuffer& buffer, bool to_device) noexcept
    : buffer_(buffer)
    , to_device_(to_device)
    , active_(buffer.is_valid()) {
    if (!active_) {
        return;
    }

    if (to_device_) {
        DMAAllocator::sync_for_device(buffer_);
    } else {
        DMAAllocator::sync_for_cpu(buffer_);
    }
}

DMAMapping::~DMAMapping() {
    if (!active_) {
        return;
    }

    if (to_device_) {
        DMAAllocator::sync_for_cpu(buffer_);
    } else {
        DMAAllocator::sync_for_device(buffer_);
    }
}

DMAMapping::DMAMapping(DMAMapping&& other) noexcept
    : buffer_(other.buffer_)
    , to_device_(other.to_device_)
    , active_(other.active_) {
    other.buffer_ = DMABuffer{};
    other.active_ = false;
}

DMAMapping& DMAMapping::operator=(DMAMapping&& other) noexcept {
    if (this != &other) {
        if (active_) {
            if (to_device_) {
                DMAAllocator::sync_for_cpu(buffer_);
            } else {
                DMAAllocator::sync_for_device(buffer_);
            }
        }

        buffer_ = other.buffer_;
        to_device_ = other.to_device_;
        active_ = other.active_;

        other.buffer_ = DMABuffer{};
        other.active_ = false;
    }
    return *this;
}

} // namespace xinim::mm
