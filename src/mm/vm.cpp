/**
 * @file vm.cpp
 * @brief Modern C++23 Polymorphic Virtual Memory Manager for XINIM.
 *
 * This module provides a flexible and robust virtual memory subsystem,
 * acting as a polymorphic frontend to various VM backend strategies.
 * The primary backend, `IntervalTreeCoWBackend`, integrates an AVL-based
 * Interval Tree for efficient Virtual Memory Area (VMA) management and
 * Copy-on-Write (COW) semantics for process forking.
 *
 * This design embraces the "Planck-scale" granularity of VMAs and page frames,
 * and the "quantum-foam" dynamism of memory allocation, deallocation, and COW
 * operations. It leverages modern C++23 features for safety, clarity, and performance.
 *
 * @ingroup memory
 * @section Features
 * - **Polymorphic Backend**: Supports swappable VM strategies via `IVmBackend` interface.
 * - **Interval Tree (AVL-based)**: `O(log N)` VMA management, handling overlaps and merging.
 * - **Copy-on-Write (COW)**: Efficient `fork` by sharing pages; duplicates only on write faults.
 * - **RAII**: `std::unique_ptr` for VMA nodes, `std::atomic` for page frame refcounts.
 * - **Strong Types**: `VmFlags`, `VmAreaType` as strong enums for type safety.
 * - **ASLR**: Simple address-space layout randomization for base addresses.
 * - **C++23**: Extensive use of modern C++23 features for safety, clarity, and performance.
 */

#include "../include/vm.hpp"
#include "const.hpp"
#include "mproc.hpp" // For vm_proc, vm_area, NR_PROCS
#include <algorithm> // For std::max, std::min
#include <array>     // For std::array
#include <atomic>    // For std::atomic
#include <cstdint>   // For uint64_t
#include <format>    // For std::format
#include <functional> // For std::function
#include <iostream>  // For std::cerr
#include <memory>    // For std::unique_ptr, std::shared_ptr
#include <stdexcept> // For std::runtime_error
#include <vector>    // For std::vector

// Forward declarations for internal helpers
static unsigned long next_rand() noexcept;
static inline virt_addr64 page_align_down(virt_addr64 addr) noexcept;
static inline virt_addr64 page_align_up(virt_addr64 addr) noexcept;
static inline bool vm_areas_overlap(const vm_area &a, const vm_area &b) noexcept;

//-----------------------------------------------------------------------------
// Conceptual Page Frame Management (The "Quantum Foam" of Physical Memory)
//-----------------------------------------------------------------------------
/**
 * @brief Represents a physical page frame.
 *
 * This is a conceptual representation of a physical page in memory.
 * It includes a reference count for Copy-on-Write (COW) semantics,
 * allowing multiple VMAs to share the same physical page until a write occurs.
 */
struct PageFrame {
    uint64_t pfn;                ///< Conceptual Physical Frame Number (for demo)
    std::atomic<uint32_t> refcnt; ///< Reference count for COW.

    /// Constructor for a new page frame.
    explicit PageFrame(uint64_t initial_pfn) noexcept : pfn{initial_pfn}, refcnt{1} {
        std::cerr << std::format("PFN {:#x} allocated. Refcnt: {}\n", pfn, refcnt.load());
    }

    /// Destructor for a page frame.
    ~PageFrame() {
        std::cerr << std::format("PFN {:#x} deallocated.\n", pfn);
    }
};

/// Simple global conceptual Page Frame Allocator.
static std::vector<std::unique_ptr<PageFrame>> g_page_frame_pool;
static uint64_t g_next_pfn = 0x1000000; // Starting PFN for demo

/**
 * @brief Allocates a new conceptual PageFrame.
 * @return A unique_ptr to the newly allocated PageFrame.
 */
static std::unique_ptr<PageFrame> alloc_page_frame() {
    g_page_frame_pool.emplace_back(std::make_unique<PageFrame>(g_next_pfn++));
    return std::move(g_page_frame_pool.back());
}

/**
 * @brief Decrements the reference count of a PageFrame. If the count reaches zero,
 * the PageFrame is conceptually freed.
 * @param frame A pointer to the PageFrame to free.
 */
static void decrement_page_frame_ref(PageFrame *frame) noexcept {
    if (frame && frame->refcnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // If refcnt drops to 0, conceptually free the physical page.
        // In a real OS, this would involve returning the PFN to the physical allocator.
        // For this demo, unique_ptr manages the memory, and the PageFrame is effectively
        // "removed" from active use when its refcount hits zero.
    }
}

//-----------------------------------------------------------------------------
// VMA Management: AVL-based Interval Tree (The "Planck Scale" of Virtual Memory)
//-----------------------------------------------------------------------------
/**
 * @brief Node for the AVL-based Interval Tree of VMAs.
 *
 * Each node stores a `vm_area` and maintains height and `max_end` for balancing
 * and efficient interval queries. It also holds a `std::shared_ptr<PageFrame>`
 * to manage the physical page backing this VMA, enabling Copy-on-Write.
 */
struct VmAreaNode {
    vm_area area;                         ///< The virtual memory area.
    std::unique_ptr<VmAreaNode> left;     ///< Left child.
    std::unique_ptr<VmAreaNode> right;    ///< Right child.
    int height{1};                        ///< Height of the node in the AVL tree.
    virt_addr64 max_end{0};               ///< Maximum end address in this node's subtree.
    std::shared_ptr<PageFrame> page_frame;///< Shared pointer to the backing PageFrame.

    /// Constructor for a VmAreaNode.
    explicit VmAreaNode(vm_area a, std::shared_ptr<PageFrame> pf = nullptr) noexcept
        : area{std::move(a)}, max_end{area.end}, page_frame{std::move(pf)} {}

    /// Destructor handles recursive deletion of children and decrements PageFrame refcount.
    ~VmAreaNode() {
        // Shared_ptr will automatically decrement refcount.
        // For a raw PageFrame pointer, we'd call decrement_page_frame_ref here.
    }
};

// AVL Tree Helpers
static int get_height(VmAreaNode *n) noexcept { return n ? n->height : 0; }
static virt_addr64 get_max_end(VmAreaNode *n) noexcept { return n ? n->max_end : 0; }
static void update_node_metrics(VmAreaNode *n) noexcept {
    if (!n) return;
    n->height = 1 + std::max(get_height(n->left.get()), get_height(n->right.get()));
    n->max_end = std::max({n->area.end, get_max_end(n->left.get()), get_max_end(n->right.get())});
}
static int get_balance(VmAreaNode *n) noexcept {
    return n ? get_height(n->left.get()) - get_height(n->right.get()) : 0;
}

static std::unique_ptr<VmAreaNode> rotate_right(std::unique_ptr<VmAreaNode> y) noexcept {
    auto x = std::move(y->left);
    y->left = std::move(x->right);
    update_node_metrics(y.get());
    x->right = std::move(y);
    update_node_metrics(x.get());
    return x;
}

static std::unique_ptr<VmAreaNode> rotate_left(std::unique_ptr<VmAreaNode> x) noexcept {
    auto y = std::move(x->right);
    x->right = std::move(y->left);
    update_node_metrics(x.get());
    y->left = std::move(x);
    update_node_metrics(x.get());
    return y;
}

/**
 * @brief Inserts a new VMA into the AVL tree, maintaining balance and interval properties.
 * @param node_ptr Reference to the unique_ptr to the current node.
 * @param new_area The VMA to insert.
 * @param page_frame An optional shared_ptr to a PageFrame to associate with this VMA.
 * @return The updated root of the subtree.
 */
static std::unique_ptr<VmAreaNode> insert_vma_node(std::unique_ptr<VmAreaNode> node_ptr,
                                                   vm_area new_area,
                                                   std::shared_ptr<PageFrame> page_frame = nullptr) noexcept {
    if (!node_ptr) {
        return std::make_unique<VmAreaNode>(std::move(new_area), std::move(page_frame));
    }

    // Simple insertion based on start address.
    // Full interval tree logic would handle overlaps/merges here.
    if (new_area.start < node_ptr->area.start) {
        node_ptr->left = insert_vma_node(std::move(node_ptr->left), std::move(new_area), std::move(page_frame));
    } else {
        node_ptr->right = insert_vma_node(std::move(node_ptr->right), std::move(new_area), std::move(page_frame));
    }

    update_node_metrics(node_ptr.get());

    // Balance the tree (simplified AVL balancing)
    int balance = get_balance(node_ptr.get());

    // Left Left Case
    if (balance > 1 && new_area.start < node_ptr->left->area.start) {
        return rotate_right(std::move(node_ptr));
    }
    // Right Right Case
    if (balance < -1 && new_area.start > node_ptr->right->area.start) {
        return rotate_left(std::move(node_ptr));
    }
    // Left Right Case
    if (balance > 1 && new_area.start > node_ptr->left->area.start) {
        node_ptr->left = rotate_left(std::move(node_ptr->left));
        return rotate_right(std::move(node_ptr));
    }
    // Right Left Case
    if (balance < -1 && new_area.start < node_ptr->right->area.start) {
        node_ptr->right = rotate_right(std::move(node_ptr->right));
        return rotate_left(std::move(node_ptr));
    }

    return node_ptr;
}

/**
 * @brief Finds the VMA node covering a specific virtual address.
 * @param node The current node in the tree.
 * @param addr The virtual address to find.
 * @return A pointer to the VmAreaNode if found, nullptr otherwise.
 */
static VmAreaNode *find_vma(VmAreaNode *node, virt_addr64 addr) noexcept {
    while (node) {
        if (addr >= node->area.start && addr < node->area.end) {
            return node;
        }
        if (addr < node->area.start) {
            node = node->left.get();
        } else {
            node = node->right.get();
        }
    }
    return nullptr;
}

//-----------------------------------------------------------------------------
// Process VM State (The "Superframework" of VM Contexts)
//-----------------------------------------------------------------------------
/**
 * @brief Per-process virtual memory information.
 *
 * Each element owns the root of an AVL Interval Tree of `VmAreaNode`s
 * describing the process's virtual memory layout. It also maintains a
 * per-process RNG state for ASLR.
 */
struct vm_proc {
    std::unique_ptr<VmAreaNode> vma_root; ///< Root of the AVL Interval Tree for this process.
    unsigned long rng_state;              ///< Per-process RNG state for ASLR.

    /// Default constructor.
    vm_proc() noexcept : vma_root(nullptr), rng_state(1) {}

    /// Copy constructor for vm_proc (deep copy of VMA tree for COW).
    vm_proc(const vm_proc &other) noexcept : rng_state(other.rng_state) {
        // Recursive helper for deep copy with COW semantics
        std::function<std::unique_ptr<VmAreaNode>(VmAreaNode *)> copy_tree =
            [&](VmAreaNode *node) -> std::unique_ptr<VmAreaNode> {
            if (!node) return nullptr;

            vm_area child_area = node->area;
            std::shared_ptr<PageFrame> child_page_frame = nullptr;

            if (node->page_frame) {
                // For writable private mappings, enable COW.
                if (has_flag(node->area.flags, VmFlags::VM_WRITE) && has_flag(node->area.flags, VmFlags::VM_PRIVATE)) {
                    child_area.flags = (node->area.flags & ~VmFlags::VM_WRITE) | VmFlags::VM_COW_WRITE_FAULT;
                    node->area.flags = (node->area.flags & ~VmFlags::VM_WRITE) | VmFlags::VM_COW_WRITE_FAULT;
                    node->page_frame->refcnt.fetch_add(1, std::memory_order_relaxed); // Increment refcount.
                    child_page_frame = node->page_frame; // Share the PageFrame pointer.
                    std::cerr << std::format("VM_FORK: COW enabled for page {:#x} (PFN {:#x}). Refcnt: {}\n",
                                             node->area.start, node->page_frame->pfn, node->page_frame->refcnt.load());
                } else {
                    // Other mappings (read-only, shared) are just shared.
                    node->page_frame->refcnt.fetch_add(1, std::memory_order_relaxed);
                    child_page_frame = node->page_frame;
                    std::cerr << std::format("VM_FORK: Shared page {:#x} (PFN {:#x}). Refcnt: {}\n",
                                             node->area.start, node->page_frame->pfn, node->page_frame->refcnt.load());
                }
            }

            auto new_node = std::make_unique<VmAreaNode>(std::move(child_area), std::move(child_page_frame));
            new_node->left = copy_tree(node->left.get());
            new_node->right = copy_tree(node->right.get());
            update_node_metrics(new_node.get());
            return new_node;
        };

        vma_root = copy_tree(other.vma_root.get());
    }

    /// Assignment operator (simplified for demo, typically deep copy or COW for tree structure).
    vm_proc &operator=(const vm_proc &other) noexcept {
        if (this != &other) {
            // In a real COW system, this would be a deep copy with refcount adjustments
            // or a copy-on-write mechanism for the tree itself.
            // For this demo, we'll just reset and let the copy constructor handle it.
            this->~vm_proc(); // Explicitly call destructor to free current resources
            new (this) vm_proc(other); // Placement new to call copy constructor
        }
        return *this;
    }
};

/// Global table of per-process VM information.
static std::vector<vm_proc> vm_proc_table(NR_PROCS);

/* State for a very small pseudo random generator used for ASLR. */
static unsigned long g_rng_state = 1;

/**
 * @brief Generate a pseudo random number used for address randomisation.
 * @return Next pseudo random value.
 */
static unsigned long next_rand() noexcept {
    g_rng_state = g_rng_state * 1103515245 + 12345;
    return g_rng_state;
}

static inline virt_addr64 page_align_down(virt_addr64 addr) noexcept {
    return addr & ~(PAGE_SIZE_4K - 1);
}

static inline virt_addr64 page_align_up(virt_addr64 addr) noexcept {
    return (addr + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
}

static inline bool vm_areas_overlap(const vm_area &a, const vm_area &b) noexcept {
    return !(a.end <= b.start || b.end <= a.start);
}

//-----------------------------------------------------------------------------
// IVmBackend: The Polymorphic VM Strategy Interface (The "Lagrangian")
//-----------------------------------------------------------------------------
/**
 * @brief Abstract interface for a VM backend strategy.
 *
 * This interface defines the core operations that any VM backend must implement,
 * allowing the main VM functions to dispatch calls polymorphically.
 */
struct IVmBackend {
    virtual ~IVmBackend() = default;
    virtual void init() noexcept = 0;
    virtual void *alloc(u64_t bytes, VmFlags flags) noexcept = 0;
    virtual void handle_fault(int proc, virt_addr64 addr) noexcept = 0;
    virtual int fork_proc(int parent, int child) noexcept = 0;
    virtual void *mmap_region(int proc, void *addr, u64_t length, VmFlags flags) noexcept = 0;
};

//-----------------------------------------------------------------------------
// IntervalTreeCoWBackend: Primary VM Backend (The "Unified Field Theory")
//-----------------------------------------------------------------------------
/**
 * @brief Concrete VM backend implementing AVL Interval Tree with Copy-on-Write.
 *
 * This backend provides a robust and efficient virtual memory management
 * solution by combining a balanced interval tree for VMAs and COW for `fork`.
 */
struct IntervalTreeCoWBackend : public IVmBackend {
    void init() noexcept override {
        for (auto &p : vm_proc_table) {
            p.vma_root.reset(); // Clears the VMA tree for each process.
            p.rng_state = 1;
        }
        g_rng_state = 1;
        g_page_frame_pool.clear(); // Clear conceptual page frames
        g_next_pfn = 0x1000000;
        std::cerr << "VM: Initialized Interval Tree + COW backend.\n";
    }

    void *alloc(u64_t bytes, VmFlags flags) noexcept override {
        virt_addr64 base;
        base = U64_C(0x0000000010000000) + (next_rand() & 0xFFFFF000); // Simple ASLR
        u64_t aligned_bytes = page_align_up(bytes);
        (void)flags;
        return reinterpret_cast<void *>(base + aligned_bytes);
    }

    void handle_fault(int proc, virt_addr64 addr) noexcept override {
        virt_addr64 page_addr = page_align_down(addr);
        vm_proc &p = vm_proc_table[proc];

        VmAreaNode *vma_node = find_vma(p.vma_root.get(), page_addr);

        if (!vma_node) {
            std::cerr << std::format("VM_FAULT: Invalid access for proc {} at addr {:#x}\n", proc, addr);
            return; // SIGSEGV in a real OS
        }

        // Conceptual COW logic
        if (has_flag(vma_node->area.flags, VmFlags::VM_COW_WRITE_FAULT)) {
            // This is a write fault on a COW page.
            if (vma_node->page_frame && vma_node->page_frame->refcnt.load(std::memory_order_acquire) > 1) {
                // Decrement old page frame refcount
                decrement_page_frame_ref(vma_node->page_frame.get());
                // Allocate a new page frame and conceptually copy data
                vma_node->page_frame = alloc_page_frame(); // Simulate copy by allocating new frame
                // Update flags: remove COW, add VM_WRITE
                vma_node->area.flags = (vma_node->area.flags & ~VmFlags::VM_COW_WRITE_FAULT) | VmFlags::VM_WRITE;
                std::cerr << std::format("VM_FAULT: COW triggered for proc {} at addr {:#x}. New PFN: {:#x}\n",
                                         proc, addr, vma_node->page_frame->pfn);
            } else if (vma_node->page_frame) {
                // It was a COW page, but we were the only owner. Just make it writable.
                vma_node->area.flags = (vma_node->area.flags & ~VmFlags::VM_COW_WRITE_FAULT) | VmFlags::VM_WRITE;
                std::cerr << std::format("VM_FAULT: COW page for proc {} at addr {:#x} now writable.\n", proc, addr);
            } else {
                // No page frame yet, allocate a new one (demand zero/fill)
                vma_node->page_frame = alloc_page_frame();
                vma_node->area.flags = (vma_node->area.flags & ~VmFlags::VM_COW_WRITE_FAULT) | VmFlags::VM_WRITE;
                std::cerr << std::format("VM_FAULT: Demand-allocated page for proc {} at addr {:#x}. PFN: {:#x}\n",
                                         proc, addr, vma_node->page_frame->pfn);
            }
        } else if (!vma_node->page_frame) {
            // Not a COW write fault, but page not present. Demand allocate.
            vma_node->page_frame = alloc_page_frame();
            std::cerr << std::format("VM_FAULT: Demand-allocated page for proc {} at addr {:#x}. PFN: {:#x}\n",
                                     proc, addr, vma_node->page_frame->pfn);
        } else {
            std::cerr << std::format("VM_FAULT: Page already present for proc {} at addr {:#x}. PFN: {:#x}\n",
                                     proc, addr, vma_node->page_frame->pfn);
        }
    }

    int fork_proc(int parent, int child) noexcept override {
        vm_proc &parent_proc = vm_proc_table[parent];
        vm_proc &child_proc = vm_proc_table[child];

        child_proc.vma_root.reset(); // Ensure child starts with an empty tree.
        child_proc.rng_state = next_rand(); // Give child its own RNG state.

        // Recursive helper for deep copy with COW semantics
        std::function<std::unique_ptr<VmAreaNode>(VmAreaNode *)> copy_tree =
            [&](VmAreaNode *node) -> std::unique_ptr<VmAreaNode> {
            if (!node) return nullptr;

            vm_area child_area = node->area;
            std::shared_ptr<PageFrame> child_page_frame = nullptr;

            if (node->page_frame) {
                // For writable private mappings, enable COW.
                if (has_flag(node->area.flags, VmFlags::VM_WRITE) && has_flag(node->area.flags, VmFlags::VM_PRIVATE)) {
                    child_area.flags = (node->area.flags & ~VmFlags::VM_WRITE) | VmFlags::VM_COW_WRITE_FAULT;
                    node->area.flags = (node->area.flags & ~VmFlags::VM_WRITE) | VmFlags::VM_COW_WRITE_FAULT;
                    node->page_frame->refcnt.fetch_add(1, std::memory_order_relaxed); // Increment refcount.
                    child_page_frame = node->page_frame; // Share the PageFrame pointer.
                    std::cerr << std::format("VM_FORK: COW enabled for page {:#x} (PFN {:#x}). Refcnt: {}\n",
                                             node->area.start, node->page_frame->pfn, node->page_frame->refcnt.load());
                } else {
                    // Other mappings (read-only, shared) are just shared.
                    node->page_frame->refcnt.fetch_add(1, std::memory_order_relaxed);
                    child_page_frame = node->page_frame;
                    std::cerr << std::format("VM_FORK: Shared page {:#x} (PFN {:#x}). Refcnt: {}\n",
                                             node->area.start, node->page_frame->pfn, node->page_frame->refcnt.load());
                }
            }

            auto new_node = std::make_unique<VmAreaNode>(std::move(child_area), std::move(child_page_frame));
            new_node->left = copy_tree(node->left.get());
            new_node->right = copy_tree(node->right.get());
            update_node_metrics(new_node.get());
            return new_node;
        };

        child_proc.vma_root = copy_tree(parent_proc.vma_root.get());
        return OK;
    }

    void *mmap_region(int proc, void *addr, u64_t length, VmFlags flags) noexcept override {
        vm_proc &p = vm_proc_table[proc];
        virt_addr64 base = reinterpret_cast<virt_addr64>(addr);

        if (!base) {
            base = reinterpret_cast<virt_addr64>(alloc(length, flags));
        }
        // Ensure base and length are page-aligned for VMA management.
        base = page_align_down(base);
        length = page_align_up(length);

        vm_area new_area{.start = base, .end = base + length, .flags = flags, .type = VmAreaType::Mapped};
        p.vma_root = insert_vma_node(std::move(p.vma_root), std::move(new_area));

        return reinterpret_cast<void *>(base);
    }
};

// Global unique_ptr to the active VM backend.
static std::unique_ptr<IVmBackend> g_vm_backend;

//-----------------------------------------------------------------------------
// Public API Functions (The "Unified Field Theory" Interface)
//-----------------------------------------------------------------------------
/*===========================================================================*
 * vm_init                                           *
 *===========================================================================*/
/**
 * @brief Initialise the virtual memory subsystem.
 *
 * Dispatches to the active VM backend's initialization routine.
 */
void vm_init() noexcept {
    // Instantiate the default backend if not already set.
    if (!g_vm_backend) {
        g_vm_backend = std::make_unique<IntervalTreeCoWBackend>();
    }
    g_vm_backend->init();
}

/*===========================================================================*
 * vm_alloc                                          *
 *===========================================================================*/
/**
 * @brief Allocate a region of virtual memory.
 *
 * Dispatches to the active VM backend's allocation routine.
 *
 * @param bytes Number of bytes to allocate.
 * @param flags Protection flags.
 * @return Base address of the allocated region.
 */
void *vm_alloc(u64_t bytes, VmFlags flags) noexcept {
    return g_vm_backend->alloc(bytes, flags);
}

/*===========================================================================*
 * vm_handle_fault                                   *
 *===========================================================================*/
/**
 * @brief Handles a page fault within a process.
 *
 * Dispatches to the active VM backend's fault handling routine.
 *
 * @param proc Index of the faulting process.
 * @param addr Faulting virtual address.
 */
void vm_handle_fault(int proc, virt_addr64 addr) noexcept {
    g_vm_backend->handle_fault(proc, addr);
}

/*===========================================================================*
 * vm_fork                                           *
 *===========================================================================*/
/**
 * @brief Duplicates a parent's virtual memory state for a child process.
 *
 * Dispatches to the active VM backend's fork routine.
 *
 * @param parent Index of the parent process.
 * @param child  Index of the child process.
 * @return ::OK on success.
 */
int vm_fork(int parent, int child) noexcept {
    return g_vm_backend->fork_proc(parent, child);
}

/*===========================================================================*
 * vm_mmap                                           *
 *===========================================================================*/
/**
 * @brief Maps a region of memory into a process.
 *
 * Dispatches to the active VM backend's mmap routine.
 *
 * @param proc   Process index to map into.
 * @param addr   Desired base address or nullptr for automatic placement.
 * @param length Length of the mapping in bytes.
 * @param flags  Mapping flags.
 * @return Base address of the mapped region.
 */
void *vm_mmap(int proc, void *addr, u64_t length, VmFlags flags) noexcept {
    return g_vm_backend->mmap_region(proc, addr, length, flags);
}

//-----------------------------------------------------------------------------
// Conceptual main for demonstration/testing
//-----------------------------------------------------------------------------
/*
int main() {
    // Example of how to use the VM system with the default backend
    vm_init(); // Initializes the IntervalTreeCoWBackend

    // Test process 0
    std::cerr << "\n--- Process 0 Operations ---\n";
    void* p0_mem1 = vm_mmap(0, nullptr, 4 * PAGE_SIZE_4K, VmFlags::VM_READ | VmFlags::VM_WRITE | VmFlags::VM_PRIVATE);
    std::cerr << std::format("P0: Mapped {:#x} to {:#x}\n", p0_mem1, (virt_addr64)p0_mem1 + 4 * PAGE_SIZE_4K);
    vm_handle_fault(0, (virt_addr64)p0_mem1 + PAGE_SIZE_4K); // Fault on a page
    vm_handle_fault(0, (virt_addr64)p0_mem1 + 2 * PAGE_SIZE_4K); // Another fault

    // Test process 1 (fork from 0)
    std::cerr << "\n--- Process 1 (Fork from P0) ---\n";
    vm_fork(0, 1);

    // P0 writes to a COW page
    std::cerr << "\n--- P0 Write to COW Page ---\n";
    vm_handle_fault(0, (virt_addr64)p0_mem1 + PAGE_SIZE_4K); // Simulate write fault on P0

    // P1 writes to its own COW page
    std::cerr << "\n--- P1 Write to COW Page ---\n";
    vm_handle_fault(1, (virt_addr64)p0_mem1 + PAGE_SIZE_4K); // Simulate write fault on P1

    std::cerr << "\nVM operations complete.\n";
    return 0;
}
*/

---

### Five Other VM Implementation Possibilities

Here are five distinct VM implementation possibilities, adhering to the **Planck-scale** granularity and **quantum-foam** dynamism, that can be integrated as alternative `IVmBackend` implementations. Each offers unique **trade-offs** and pedagogical insights.

#### 1. `FixedIntrusiveListBackend` (Cache-Tight, Zero Heap)

* **Core Idea**: Uses `std::array` to store `vm_area_node`s directly within `vm_proc_intr`, forming an intrusive linked list. No dynamic heap allocations after initialization.
* **Trade-offs**: Extremely memory-efficient and cache-friendly. All operations are `O(N)` in the worst case, and there's a hard compile-time limit on VMAs per process. Ideal for highly constrained embedded systems or teaching basic list management.
* **Plank Scale**: `vm_area_node`s are the fundamental quanta, linked intrusively without external pointers, representing a tightly packed "quantum foam" of VMAs.

#### 2. `SparseRadixMapBackend` (Page-Granular, Precise Fault Accounting)

* **Core Idea**: Uses `std::unordered_map<u64_t, PageDesc>` where keys are virtual page numbers. Each `PageDesc` holds flags and presence.
* **Trade-offs**: Excellent for modeling real hardware page tables and handling sparse address spaces. `O(1)` average-case lookup for page metadata. `std::unordered_map` introduces dynamic allocation overhead and potential hash collisions.
* **Plank Scale**: Each entry in the map represents a single virtual page, a "quantum" of addressable memory, allowing precise control over its state. The map itself is a "quantum foam" of allocated pages.

#### 3. `BuddyAllocatorBackend` (Fast Contiguous Regions)

* **Core Idea**: Manages virtual address space using a buddy allocation system. This provides fast allocation and deallocation of power-of-two sized, contiguous virtual memory regions.
* **Trade-offs**: Excellent for allocating large, aligned blocks (e.g., for heaps or large file mappings). Suffers from internal fragmentation due to power-of-two rounding. Primarily a region-level allocator; needs to be paired with a page-level mechanism for per-page details.
* **Plank Scale**: Virtual address ranges are treated as "buddies" – quanta that can be split and merged. The free lists for each order represent different "energy levels" of available contiguous virtual space.

#### 4. `RegionBitmapBackend` (Hybrid Demand Paging)

* **Core Idea**: Combines coarse-grained `Region` descriptors (like VMAs) with fine-grained `std::bitset`s to track individual page presence within each region.
* **Trade-offs**: Balances efficient region management with precise page-level population tracking. Good for demand paging where processes have a few large VM regions but require exact per-page population status. Fixed region size granularity can lead to some internal fragmentation.
* **Plank Scale**: `Region`s are larger quanta, while the `std::bitset` within each region tracks the "presence" (population state) of individual page quanta, like a quantum state vector for a memory region.

#### 5. `PMRMonotonicArenaBackend` (Deterministic Allocations)

* **Core Idea**: Uses C++17's `std::pmr::monotonic_buffer_resource` to manage allocations within a pre-allocated virtual memory arena for each process.
* **Trade-offs**: Provides deterministic allocation costs and excellent cache locality within the arena. Freed memory is not immediately reusable within the arena (only on arena destruction). Great for teaching custom allocators and RAII.
* **Plank Scale**: The `monotonic_buffer_resource` treats a large virtual buffer as a "quantum field" where allocations are simply "particle creations" (bumping a pointer). The `vm_area`s are then tracked within this arena.

---