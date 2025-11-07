/**
 * @file dma.hpp
 * @brief DMA (Direct Memory Access) Management for Device Drivers
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace xinim::mm {

/**
 * @brief DMA Buffer Allocation Flags
 */
enum class DMAFlags : uint32_t {
    NONE          = 0,
    CONTIGUOUS    = (1 << 0),  // Physically contiguous memory
    CACHE_COHERENT = (1 << 1), // Cache-coherent (uncached/write-through)
    ZERO          = (1 << 2),  // Zero-initialize memory
    BELOW_4GB     = (1 << 3),  // For 32-bit DMA devices
    BELOW_16MB    = (1 << 4),  // For ISA/legacy DMA
};

inline DMAFlags operator|(DMAFlags a, DMAFlags b) {
    return static_cast<DMAFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DMAFlags operator&(DMAFlags a, DMAFlags b) {
    return static_cast<DMAFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool operator!(DMAFlags a) {
    return static_cast<uint32_t>(a) == 0;
}

/**
 * @brief DMA Buffer Handle
 * 
 * RAII wrapper for DMA-capable memory buffers
 */
class DMABuffer {
public:
    DMABuffer() = default;
    ~DMABuffer();

    // No copy, movable
    DMABuffer(const DMABuffer&) = delete;
    DMABuffer& operator=(const DMABuffer&) = delete;
    DMABuffer(DMABuffer&& other) noexcept;
    DMABuffer& operator=(DMABuffer&& other) noexcept;

    // Accessors
    void* virtual_addr() const { return virt_addr_; }
    uint64_t physical_addr() const { return phys_addr_; }
    size_t size() const { return size_; }
    bool is_valid() const { return virt_addr_ != nullptr; }

    // Typed access
    template<typename T>
    T* as() { return static_cast<T*>(virt_addr_); }
    
    template<typename T>
    const T* as() const { return static_cast<const T*>(virt_addr_); }

    // Memory operations
    void zero();
    void copy_from(const void* src, size_t len);
    void copy_to(void* dst, size_t len) const;

private:
    friend class DMAAllocator;
    
    DMABuffer(void* virt, uint64_t phys, size_t sz, DMAFlags flags)
        : virt_addr_(virt), phys_addr_(phys), size_(sz), flags_(flags) {}

    void* virt_addr_ = nullptr;
    uint64_t phys_addr_ = 0;
    size_t size_ = 0;
    DMAFlags flags_ = DMAFlags::NONE;
};

/**
 * @brief Scatter-Gather List Entry
 */
struct SGEntry {
    uint64_t phys_addr;  // Physical address
    uint32_t length;      // Length in bytes
    uint32_t reserved;    // For alignment
};

/**
 * @brief Scatter-Gather List
 * 
 * For devices supporting scatter-gather DMA
 */
class SGList {
public:
    SGList() = default;
    ~SGList() = default;

    // Add entry
    bool add(uint64_t phys_addr, uint32_t length);
    
    // Accessors
    size_t count() const { return entries_.size(); }
    const SGEntry* entries() const { return entries_.data(); }
    SGEntry* entries() { return entries_.data(); }
    
    // Total data length
    size_t total_length() const;
    
    // Clear all entries
    void clear() { entries_.clear(); }

private:
    std::vector<SGEntry> entries_;
};

/**
 * @brief DMA Pool for Efficient Small Allocations
 * 
 * Pre-allocates a large DMA region and suballocates from it
 */
class DMAPool {
public:
    DMAPool(size_t object_size, size_t alignment, size_t pool_size, DMAFlags flags);
    ~DMAPool();

    // Allocation
    DMABuffer allocate();
    void free(DMABuffer&& buffer);

    // Info
    size_t object_size() const { return object_size_; }
    size_t objects_allocated() const { return allocated_count_; }
    size_t objects_available() const { return pool_objects_ - allocated_count_; }

private:
    size_t object_size_;
    size_t alignment_;
    size_t pool_objects_;
    size_t allocated_count_;
    DMAFlags flags_;
    
    DMABuffer pool_buffer_;
    
    struct FreeNode {
        FreeNode* next;
    };
    FreeNode* free_list_;
};

/**
 * @brief DMA Memory Allocator
 * 
 * Manages DMA-capable memory for device drivers
 */
class DMAAllocator {
public:
    static DMAAllocator& instance();

    // Buffer allocation
    DMABuffer allocate(size_t size, size_t alignment = 16, 
                      DMAFlags flags = DMAFlags::CONTIGUOUS);
    
    // Pool management
    std::shared_ptr<DMAPool> create_pool(size_t object_size, size_t alignment,
                                         size_t pool_size, DMAFlags flags);

    // Address translation
    uint64_t virt_to_phys(void* virt_addr);
    void* phys_to_virt(uint64_t phys_addr);

    // Cache operations (for non-coherent DMA)
    void flush_cache(void* virt_addr, size_t size);
    void invalidate_cache(void* virt_addr, size_t size);
    void sync_for_device(void* virt_addr, size_t size);
    void sync_for_cpu(void* virt_addr, size_t size);

    // Statistics
    size_t total_allocated() const { return total_allocated_; }
    size_t total_freed() const { return total_freed_; }

private:
    DMAAllocator() = default;
    ~DMAAllocator() = default;

    // No copy
    DMAAllocator(const DMAAllocator&) = delete;
    DMAAllocator& operator=(const DMAAllocator&) = delete;

    size_t total_allocated_ = 0;
    size_t total_freed_ = 0;

    // Physical memory allocation
    void* alloc_phys_pages(size_t size, uint64_t& phys_addr, DMAFlags flags);
    void free_phys_pages(void* virt_addr, size_t size);
};

/**
 * @brief IOMMU (I/O Memory Management Unit) Support
 * 
 * For systems with IOMMU/VT-d/AMD-Vi
 * Future enhancement for security and large memory support
 */
class IOMMU {
public:
    static IOMMU& instance();

    bool is_enabled() const { return enabled_; }
    bool initialize();
    
    // IOVA (I/O Virtual Address) management
    uint64_t map_buffer(void* virt_addr, size_t size, bool writable);
    void unmap_buffer(uint64_t iova, size_t size);
    
    // Domain management (for device isolation)
    uint32_t create_domain();
    void destroy_domain(uint32_t domain_id);
    void attach_device(uint32_t domain_id, uint16_t segment, uint8_t bus, 
                      uint8_t device, uint8_t function);

private:
    IOMMU() = default;
    ~IOMMU() = default;

    bool enabled_ = false;
    // Implementation details would go here
};

/**
 * @brief DMA Mapping Helper
 * 
 * RAII wrapper for DMA mappings with automatic cleanup
 */
class DMAMapping {
public:
    DMAMapping(void* cpu_addr, size_t size, bool to_device);
    ~DMAMapping();

    // No copy, movable
    DMAMapping(const DMAMapping&) = delete;
    DMAMapping& operator=(const DMAMapping&) = delete;
    DMAMapping(DMAMapping&& other) noexcept;
    DMAMapping& operator=(DMAMapping&& other) noexcept;

    uint64_t device_addr() const { return device_addr_; }
    size_t size() const { return size_; }

private:
    void* cpu_addr_;
    uint64_t device_addr_;
    size_t size_;
    bool to_device_;
};

} // namespace xinim::mm
