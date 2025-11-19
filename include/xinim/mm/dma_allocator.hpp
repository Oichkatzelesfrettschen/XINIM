// XINIM Operating System
// Copyright (c) 2025 XINIM Project
//
// DMA (Direct Memory Access) Allocator
// Provides contiguous physical memory allocation for device drivers
//
// This allocator is specifically designed for DMA operations which require:
// - Physically contiguous memory regions
// - Memory below 4GB (for 32-bit DMA devices)
// - Cache coherency management
// - Physical address translation

#pragma once

#include <cstdint>
#include <cstddef>

namespace xinim::mm {

// DMA allocation flags
enum class DMAFlags : uint32_t {
    NONE = 0,
    BELOW_4GB = (1 << 0),      // Allocate memory below 4GB (for 32-bit DMA)
    COHERENT = (1 << 1),       // Cache-coherent memory (uncached)
    ZERO = (1 << 2),           // Zero the allocated memory
    CONTIGUOUS = (1 << 3),     // Physically contiguous (always true for DMA)
};

inline DMAFlags operator|(DMAFlags a, DMAFlags b) {
    return static_cast<DMAFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline DMAFlags operator&(DMAFlags a, DMAFlags b) {
    return static_cast<DMAFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

inline bool operator!(DMAFlags flags) {
    return static_cast<uint32_t>(flags) == 0;
}

// DMA buffer descriptor
struct DMABuffer {
    void* virtual_addr;        // Virtual address (for CPU access)
    uint64_t physical_addr;    // Physical address (for DMA)
    size_t size;               // Buffer size in bytes
    DMAFlags flags;            // Allocation flags
    bool is_coherent;          // True if cache-coherent

    DMABuffer()
        : virtual_addr(nullptr)
        , physical_addr(0)
        , size(0)
        , flags(DMAFlags::NONE)
        , is_coherent(false) {}

    bool is_valid() const { return virtual_addr != nullptr; }
};

// DMA Allocator Interface
class DMAAllocator {
public:
    // Initialize the DMA allocator
    static bool initialize();

    // Shutdown the DMA allocator
    static void shutdown();

    // Allocate DMA-capable memory
    // Returns DMABuffer with both virtual and physical addresses
    static DMABuffer allocate(size_t size, DMAFlags flags = DMAFlags::NONE);

    // Free DMA-capable memory
    static void free(const DMABuffer& buffer);

    // Allocate aligned DMA memory
    static DMABuffer allocate_aligned(size_t size, size_t alignment,
                                      DMAFlags flags = DMAFlags::NONE);

    // Physical to virtual address translation
    static void* phys_to_virt(uint64_t phys_addr);

    // Virtual to physical address translation
    static uint64_t virt_to_phys(const void* virt_addr);

    // Cache coherency operations
    static void flush_cache(const void* virt_addr, size_t size);
    static void invalidate_cache(const void* virt_addr, size_t size);
    static void sync_for_device(const DMABuffer& buffer);
    static void sync_for_cpu(const DMABuffer& buffer);

    // Query allocator status
    static size_t get_total_allocated();
    static size_t get_available_memory();
    static bool is_address_dma_capable(uint64_t phys_addr);

private:
    DMAAllocator() = delete;
    ~DMAAllocator() = delete;
    DMAAllocator(const DMAAllocator&) = delete;
    DMAAllocator& operator=(const DMAAllocator&) = delete;
};

// RAII wrapper for DMA buffers
class ScopedDMABuffer {
public:
    explicit ScopedDMABuffer(size_t size, DMAFlags flags = DMAFlags::NONE)
        : buffer_(DMAAllocator::allocate(size, flags)) {}

    ~ScopedDMABuffer() {
        if (buffer_.is_valid()) {
            DMAAllocator::free(buffer_);
        }
    }

    // No copy
    ScopedDMABuffer(const ScopedDMABuffer&) = delete;
    ScopedDMABuffer& operator=(const ScopedDMABuffer&) = delete;

    // Move semantics
    ScopedDMABuffer(ScopedDMABuffer&& other) noexcept
        : buffer_(other.buffer_) {
        other.buffer_ = DMABuffer{};
    }

    ScopedDMABuffer& operator=(ScopedDMABuffer&& other) noexcept {
        if (this != &other) {
            if (buffer_.is_valid()) {
                DMAAllocator::free(buffer_);
            }
            buffer_ = other.buffer_;
            other.buffer_ = DMABuffer{};
        }
        return *this;
    }

    const DMABuffer& get() const { return buffer_; }
    DMABuffer& get() { return buffer_; }

    void* data() { return buffer_.virtual_addr; }
    const void* data() const { return buffer_.virtual_addr; }

    uint64_t physical_address() const { return buffer_.physical_addr; }
    size_t size() const { return buffer_.size; }
    bool is_valid() const { return buffer_.is_valid(); }

    // Release ownership
    DMABuffer release() {
        DMABuffer temp = buffer_;
        buffer_ = DMABuffer{};
        return temp;
    }

private:
    DMABuffer buffer_;
};

} // namespace xinim::mm
