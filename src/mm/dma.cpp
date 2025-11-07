/**
 * @file dma.cpp
 * @brief DMA buffer management implementation
 * 
 * Provides physical memory allocation for DMA operations with cache coherency
 * and scatter-gather support. Based on Linux DMA API and BSD busdma.
 */

#include <xinim/mm/dma.hpp>
#include <cstring>
#include <algorithm>

namespace xinim::mm {

// Physical memory allocator (simplified for now - will integrate with PMM)
static void* allocate_physical_pages(size_t num_pages, uint64_t& phys_addr) {
    // TODO: Integrate with actual PMM
    // For now, use aligned allocation and assume identity mapping
    void* virt = aligned_alloc(4096, num_pages * 4096);
    if (virt) {
        std::memset(virt, 0, num_pages * 4096);
        // TODO: Get real physical address from page tables
        phys_addr = reinterpret_cast<uint64_t>(virt);
    }
    return virt;
}

static void free_physical_pages(void* virt, size_t num_pages) {
    // TODO: Integrate with actual PMM
    free(virt);
}

// DMABuffer implementation

DMABuffer::DMABuffer(size_t size, DMAConstraints constraints)
    : size_(size)
    , constraints_(constraints)
    , virt_addr_(nullptr)
    , phys_addr_(0)
{
    allocate();
}

DMABuffer::~DMABuffer() {
    if (virt_addr_) {
        size_t num_pages = (size_ + 4095) / 4096;
        free_physical_pages(virt_addr_, num_pages);
    }
}

DMABuffer::DMABuffer(DMABuffer&& other) noexcept
    : size_(other.size_)
    , constraints_(other.constraints_)
    , virt_addr_(other.virt_addr_)
    , phys_addr_(other.phys_addr_)
{
    other.size_ = 0;
    other.virt_addr_ = nullptr;
    other.phys_addr_ = 0;
}

DMABuffer& DMABuffer::operator=(DMABuffer&& other) noexcept {
    if (this != &other) {
        if (virt_addr_) {
            size_t num_pages = (size_ + 4095) / 4096;
            free_physical_pages(virt_addr_, num_pages);
        }
        
        size_ = other.size_;
        constraints_ = other.constraints_;
        virt_addr_ = other.virt_addr_;
        phys_addr_ = other.phys_addr_;
        
        other.size_ = 0;
        other.virt_addr_ = nullptr;
        other.phys_addr_ = 0;
    }
    return *this;
}

void DMABuffer::allocate() {
    size_t num_pages = (size_ + 4095) / 4096;
    virt_addr_ = allocate_physical_pages(num_pages, phys_addr_);
    
    if (!virt_addr_) {
        throw std::bad_alloc();
    }
    
    // Validate constraints
    if (constraints_.max_address && phys_addr_ + size_ > constraints_.max_address) {
        free_physical_pages(virt_addr_, num_pages);
        virt_addr_ = nullptr;
        phys_addr_ = 0;
        throw std::bad_alloc();
    }
    
    if (constraints_.alignment && (phys_addr_ % constraints_.alignment) != 0) {
        free_physical_pages(virt_addr_, num_pages);
        virt_addr_ = nullptr;
        phys_addr_ = 0;
        throw std::bad_alloc();
    }
}

void DMABuffer::sync(SyncDirection direction) {
    if (!virt_addr_) return;
    
    // TODO: Implement cache operations based on architecture
    // For x86_64 with cache coherency, this is often a no-op
    // But we may need sfence/mfence for ordering
    
    switch (direction) {
        case SyncDirection::ToDevice:
            // Flush CPU cache to ensure device sees latest data
            __asm__ volatile("sfence" ::: "memory");
            break;
            
        case SyncDirection::FromDevice:
            // Invalidate CPU cache to ensure CPU sees device's data
            __asm__ volatile("mfence" ::: "memory");
            break;
            
        case SyncDirection::Bidirectional:
            __asm__ volatile("mfence" ::: "memory");
            break;
    }
}

// DMAPool implementation

DMAPool::DMAPool(size_t block_size, size_t num_blocks, DMAConstraints constraints)
    : block_size_((block_size + 63) & ~63)  // Align to 64 bytes
    , num_blocks_(num_blocks)
    , constraints_(constraints)
{
    // Allocate one large buffer for the pool
    size_t total_size = block_size_ * num_blocks_;
    pool_buffer_ = std::make_unique<DMABuffer>(total_size, constraints);
    
    // Initialize free list
    free_blocks_.reserve(num_blocks_);
    for (size_t i = 0; i < num_blocks_; ++i) {
        free_blocks_.push_back(i);
    }
}

void* DMAPool::allocate(uint64_t& phys_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (free_blocks_.empty()) {
        return nullptr;
    }
    
    size_t block_idx = free_blocks_.back();
    free_blocks_.pop_back();
    
    uint8_t* virt_base = static_cast<uint8_t*>(pool_buffer_->virtual_address());
    uint64_t phys_base = pool_buffer_->physical_address();
    
    phys_addr = phys_base + (block_idx * block_size_);
    return virt_base + (block_idx * block_size_);
}

void DMAPool::free(void* ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint8_t* virt_base = static_cast<uint8_t*>(pool_buffer_->virtual_address());
    size_t offset = static_cast<uint8_t*>(ptr) - virt_base;
    size_t block_idx = offset / block_size_;
    
    if (block_idx < num_blocks_) {
        free_blocks_.push_back(block_idx);
    }
}

// SGList implementation

bool SGList::add_entry(uint64_t phys_addr, size_t length) {
    if (entries.size() >= max_entries) {
        return false;
    }
    
    // Try to coalesce with previous entry
    if (!entries.empty()) {
        auto& last = entries.back();
        if (last.phys_addr + last.length == phys_addr) {
            last.length += length;
            return true;
        }
    }
    
    entries.push_back({phys_addr, length});
    return true;
}

size_t SGList::total_length() const {
    size_t total = 0;
    for (const auto& entry : entries) {
        total += entry.length;
    }
    return total;
}

} // namespace xinim::mm
