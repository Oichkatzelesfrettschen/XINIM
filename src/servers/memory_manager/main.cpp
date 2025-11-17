/**
 * @file main.cpp
 * @brief Memory Manager - Userspace memory allocation server for XINIM
 *
 * The Memory Manager is a userspace component that handles dynamic memory
 * allocation for processes: heap (brk), memory mapping (mmap), and shared
 * memory segments.
 *
 * Architecture:
 * 1. Maintains per-process heap boundaries (brk)
 * 2. Manages virtual memory regions (mmap)
 * 3. Allocates physical pages from kernel
 * 4. Handles shared memory segments (System V IPC)
 * 5. Enforces memory limits and protections
 *
 * @ingroup servers
 */

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "../../../include/xinim/ipc/message_types.h"
#include "../../../include/xinim/ipc/mm_protocol.hpp"
#include "../../../include/sys/type.hpp"

/* Lattice IPC functions */
namespace lattice {
    extern "C" int lattice_connect(int src, int dst, int node_id);
    extern "C" int lattice_send(int src, int dst, const message* msg, int flags);
    extern "C" int lattice_recv(int pid, message* msg, int flags);
}

namespace xinim::mem_mgr {

using namespace xinim::ipc;

/* ========================================================================
 * Memory Constants
 * ======================================================================== */

constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t HEAP_BASE = 0x0000'0000'1000'0000ULL;  // 256 MB
constexpr uint64_t HEAP_MAX  = 0x0000'0000'4000'0000ULL;  // 1 GB
constexpr uint64_t MMAP_BASE = 0x0000'7000'0000'0000ULL;  // 112 TB (high address)

/* ========================================================================
 * Memory Region
 * ======================================================================== */

/**
 * @brief Memory region descriptor
 */
struct MemoryRegion {
    uint64_t start;         /**< Start address */
    uint64_t length;        /**< Length in bytes */
    uint32_t prot;          /**< Protection flags (PROT_READ, etc.) */
    uint32_t flags;         /**< Mapping flags (MAP_PRIVATE, etc.) */
    int32_t fd;             /**< File descriptor (-1 for anonymous) */
    uint64_t offset;        /**< File offset */

    MemoryRegion()
        : start(0), length(0), prot(0), flags(0), fd(-1), offset(0) {}

    MemoryRegion(uint64_t s, uint64_t len, uint32_t p, uint32_t f, int32_t fd_val, uint64_t off)
        : start(s), length(len), prot(p), flags(f), fd(fd_val), offset(off) {}

    uint64_t end() const { return start + length; }

    bool overlaps(uint64_t addr, uint64_t len) const {
        return (addr < end()) && ((addr + len) > start);
    }
};

/* ========================================================================
 * Process Memory State
 * ======================================================================== */

/**
 * @brief Per-process memory state
 */
struct ProcessMemory {
    uint64_t heap_start;            /**< Heap base address */
    uint64_t heap_brk;              /**< Current heap break */
    uint64_t heap_max;              /**< Maximum heap address */
    uint64_t mmap_hint;             /**< Next mmap address hint */
    std::vector<MemoryRegion> regions;  /**< Mapped regions */

    ProcessMemory()
        : heap_start(HEAP_BASE),
          heap_brk(HEAP_BASE),
          heap_max(HEAP_MAX),
          mmap_hint(MMAP_BASE) {}

    /**
     * @brief Find region containing address
     */
    MemoryRegion* find_region(uint64_t addr) {
        for (auto& r : regions) {
            if (addr >= r.start && addr < r.end()) {
                return &r;
            }
        }
        return nullptr;
    }

    /**
     * @brief Check if address range is free
     */
    bool is_free(uint64_t addr, uint64_t length) {
        for (const auto& r : regions) {
            if (r.overlaps(addr, length)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Add memory region
     */
    void add_region(const MemoryRegion& region) {
        regions.push_back(region);
        // Sort by start address
        std::sort(regions.begin(), regions.end(),
                  [](const MemoryRegion& a, const MemoryRegion& b) {
                      return a.start < b.start;
                  });
    }

    /**
     * @brief Remove memory region
     */
    bool remove_region(uint64_t addr) {
        for (auto it = regions.begin(); it != regions.end(); ++it) {
            if (it->start == addr) {
                regions.erase(it);
                return true;
            }
        }
        return false;
    }
};

/* ========================================================================
 * Shared Memory Segment
 * ======================================================================== */

/**
 * @brief Shared memory segment (System V IPC)
 */
struct ShmSegment {
    int32_t shmid;              /**< Segment ID */
    int32_t key;                /**< IPC key */
    uint64_t size;              /**< Segment size */
    uint32_t flags;             /**< Creation flags */
    uint32_t uid;               /**< Owner UID */
    uint32_t gid;               /**< Owner GID */
    uint32_t mode;              /**< Permissions */
    int32_t attach_count;       /**< Number of attached processes */
    uint64_t physical_addr;     /**< Physical memory address (allocated by kernel) */

    ShmSegment()
        : shmid(0), key(0), size(0), flags(0),
          uid(0), gid(0), mode(0),
          attach_count(0), physical_addr(0) {}
};

/* ========================================================================
 * Memory Manager State
 * ======================================================================== */

/**
 * @brief Global memory manager state
 */
struct MemMgrState {
    std::unordered_map<int32_t, ProcessMemory> process_mem;  /**< Per-process memory */
    std::unordered_map<int32_t, ShmSegment> shm_segments;     /**< Shared memory segments */
    int32_t next_shmid;                                       /**< Next shm ID */

    MemMgrState() : next_shmid(1) {}

    ProcessMemory* get_process_mem(int32_t pid) {
        auto it = process_mem.find(pid);
        if (it == process_mem.end()) {
            process_mem[pid] = ProcessMemory();
            return &process_mem[pid];
        }
        return &it->second;
    }
};

static MemMgrState g_state;

/* ========================================================================
 * Memory Operations
 * ======================================================================== */

/**
 * @brief Handle MM_BRK request
 */
static void handle_brk(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmBrkRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmBrkResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    auto* mem = g_state.get_process_mem(req->caller_pid);

    // If new_brk is 0, just return current brk
    if (req->new_brk == 0) {
        resp->current_brk = mem->heap_brk;
        resp->result = 0;
        resp->error = IPC_SUCCESS;
        return;
    }

    // Check if new_brk is within valid range
    if (req->new_brk < mem->heap_start || req->new_brk > mem->heap_max) {
        resp->current_brk = mem->heap_brk;
        resp->result = -1;
        resp->error = IPC_ENOMEM;
        return;
    }

    // TODO: Kernel needs to allocate/deallocate pages
    // For now, just update the brk value
    mem->heap_brk = req->new_brk;

    resp->current_brk = mem->heap_brk;
    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_MMAP request
 */
static void handle_mmap(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmMmapRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmMmapResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    auto* mem = g_state.get_process_mem(req->caller_pid);

    // Round length up to page size
    uint64_t length = (req->length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint64_t addr;

    if (req->flags & 0x10) {  // MAP_FIXED
        // User requested specific address
        addr = req->addr;
        if (addr == 0 || (addr % PAGE_SIZE) != 0) {
            resp->mapped_addr = 0;
            resp->result = -1;
            resp->error = IPC_EINVAL;
            return;
        }
        if (!mem->is_free(addr, length)) {
            resp->mapped_addr = 0;
            resp->result = -1;
            resp->error = IPC_ENOMEM;
            return;
        }
    } else {
        // Kernel chooses address
        if (req->addr != 0 && (req->addr % PAGE_SIZE) == 0 &&
            mem->is_free(req->addr, length)) {
            // Use hint if valid and free
            addr = req->addr;
        } else {
            // Find free region starting from mmap_hint
            addr = mem->mmap_hint;
            while (!mem->is_free(addr, length)) {
                addr += PAGE_SIZE;
                if (addr + length >= 0x0000'8000'0000'0000ULL) {  // Exceeded userspace
                    resp->mapped_addr = 0;
                    resp->result = -1;
                    resp->error = IPC_ENOMEM;
                    return;
                }
            }
        }
    }

    // Create memory region
    MemoryRegion region(addr, length, req->prot, req->flags, req->fd, req->offset);
    mem->add_region(region);

    // Update mmap hint for next allocation
    mem->mmap_hint = addr + length;

    // TODO: Kernel needs to:
    // 1. Allocate physical pages
    // 2. Map pages into process page table
    // 3. Set page permissions (PROT_READ, PROT_WRITE, PROT_EXEC)
    // 4. If MAP_ANONYMOUS, zero pages
    // 5. If file-backed, set up page fault handler to load from file

    resp->mapped_addr = addr;
    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_MUNMAP request
 */
static void handle_munmap(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmMunmapRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmMunmapResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    auto* mem = g_state.get_process_mem(req->caller_pid);

    // Check alignment
    if ((req->addr % PAGE_SIZE) != 0) {
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    // Find and remove region
    auto* region = mem->find_region(req->addr);
    if (!region || region->start != req->addr) {
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    mem->remove_region(req->addr);

    // TODO: Kernel needs to:
    // 1. Unmap pages from process page table
    // 2. Free physical pages (if not shared)
    // 3. Flush TLB

    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_MPROTECT request
 */
static void handle_mprotect(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmMprotectRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmMprotectResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    auto* mem = g_state.get_process_mem(req->caller_pid);

    // Check alignment
    if ((req->addr % PAGE_SIZE) != 0) {
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    // Find region
    auto* region = mem->find_region(req->addr);
    if (!region) {
        resp->result = -1;
        resp->error = IPC_ENOMEM;
        return;
    }

    // Update protection
    region->prot = req->prot;

    // TODO: Kernel needs to:
    // 1. Update page table entries with new protection
    // 2. Flush TLB

    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_SHMGET request
 */
static void handle_shmget(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmShmgetRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmShmgetResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    // Check if segment with this key already exists
    for (const auto& [shmid, seg] : g_state.shm_segments) {
        if (seg.key == req->key) {
            if (req->flags & 0x0400) {  // IPC_EXCL
                resp->shmid = -1;
                resp->error = IPC_EEXIST;
                return;
            }
            resp->shmid = shmid;
            resp->error = IPC_SUCCESS;
            return;
        }
    }

    // Create new segment
    if (!(req->flags & 0x0200)) {  // IPC_CREAT not set
        resp->shmid = -1;
        resp->error = IPC_ENOENT;
        return;
    }

    ShmSegment seg;
    seg.shmid = g_state.next_shmid++;
    seg.key = req->key;
    seg.size = (req->size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Round up
    seg.flags = req->flags;
    seg.uid = 0;  // TODO: Get from process table
    seg.gid = 0;
    seg.mode = req->flags & 0777;

    // TODO: Kernel needs to allocate physical memory for segment
    seg.physical_addr = 0;  // Placeholder

    g_state.shm_segments[seg.shmid] = seg;

    resp->shmid = seg.shmid;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_SHMAT request
 */
static void handle_shmat(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmShmatRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmShmatResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    // Find shared memory segment
    auto it = g_state.shm_segments.find(req->shmid);
    if (it == g_state.shm_segments.end()) {
        resp->attached_addr = 0;
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    ShmSegment& seg = it->second;
    auto* mem = g_state.get_process_mem(req->caller_pid);

    // Determine attach address
    uint64_t addr;
    if (req->shmaddr != 0) {
        addr = req->shmaddr;
        if (req->flags & 0x2000) {  // SHM_RND
            addr &= ~(PAGE_SIZE - 1);  // Round down
        }
    } else {
        // Choose address
        addr = mem->mmap_hint;
        while (!mem->is_free(addr, seg.size)) {
            addr += PAGE_SIZE;
        }
    }

    // Check if address is free
    if (!mem->is_free(addr, seg.size)) {
        resp->attached_addr = 0;
        resp->result = -1;
        resp->error = IPC_ENOMEM;
        return;
    }

    // Create memory region for shared segment
    uint32_t prot = 0x1 | 0x2;  // PROT_READ | PROT_WRITE
    if (req->flags & 0x1000) {  // SHM_RDONLY
        prot = 0x1;  // PROT_READ only
    }

    MemoryRegion region(addr, seg.size, prot, 0x01, -1, 0);  // MAP_SHARED
    mem->add_region(region);

    seg.attach_count++;

    // TODO: Kernel needs to:
    // 1. Map shared memory physical pages into process page table
    // 2. Set permissions (read-only or read-write)

    resp->attached_addr = addr;
    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_SHMDT request
 */
static void handle_shmdt(const message& request, message& response) {
    const auto* req = reinterpret_cast<const MmShmdtRequest*>(&request.m_u);
    auto* resp = reinterpret_cast<MmGenericResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    auto* mem = g_state.get_process_mem(req->caller_pid);

    // Find region
    auto* region = mem->find_region(req->shmaddr);
    if (!region || region->start != req->shmaddr) {
        resp->result = -1;
        resp->error = IPC_EINVAL;
        return;
    }

    // Remove region
    mem->remove_region(req->shmaddr);

    // Decrement attach count
    // TODO: Find corresponding shm segment and decrement attach_count

    // TODO: Kernel needs to unmap shared memory pages

    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/**
 * @brief Handle MM_GETPAGESIZE request
 */
static void handle_getpagesize(const message& request, message& response) {
    auto* resp = reinterpret_cast<MmGetpagesizeResponse*>(&response.m_u);

    response.m_source = MEM_MGR_PID;
    response.m_type = MM_REPLY;

    resp->page_size = PAGE_SIZE;
    resp->result = 0;
    resp->error = IPC_SUCCESS;
}

/* ========================================================================
 * Message Dispatcher
 * ======================================================================== */

/**
 * @brief Dispatch incoming IPC message
 */
static void dispatch_message(const message& request, message& response) {
    switch (request.m_type) {
        case MM_BRK:
            handle_brk(request, response);
            break;
        case MM_MMAP:
            handle_mmap(request, response);
            break;
        case MM_MUNMAP:
            handle_munmap(request, response);
            break;
        case MM_MPROTECT:
            handle_mprotect(request, response);
            break;
        case MM_SHMGET:
            handle_shmget(request, response);
            break;
        case MM_SHMAT:
            handle_shmat(request, response);
            break;
        case MM_SHMDT:
            handle_shmdt(request, response);
            break;
        case MM_GETPAGESIZE:
            handle_getpagesize(request, response);
            break;
        default:
            response.m_source = MEM_MGR_PID;
            response.m_type = MM_ERROR;
            auto* resp = reinterpret_cast<MmGenericResponse*>(&response.m_u);
            resp->result = -1;
            resp->error = IPC_ENOSYS;
            break;
    }
}

/* ========================================================================
 * Server Initialization and Main Loop
 * ======================================================================== */

/**
 * @brief Initialize memory manager
 */
static bool initialize() {
    // Nothing to initialize yet
    return true;
}

/**
 * @brief Memory manager main loop
 */
static void server_loop() {
    message request, response;

    while (true) {
        // Receive IPC message from kernel
        int result = lattice::lattice_recv(MEM_MGR_PID, &request, 0);
        if (result < 0) {
            continue;
        }

        // Dispatch request
        std::memset(&response, 0, sizeof(response));
        dispatch_message(request, response);

        // Send response back to caller
        lattice::lattice_send(MEM_MGR_PID, request.m_source, &response, 0);
    }
}

} // namespace xinim::mem_mgr

/* ========================================================================
 * Entry Point
 * ======================================================================== */

/**
 * @brief Memory Manager entry point
 */
int main() {
    using namespace xinim::mem_mgr;

    if (!initialize()) {
        return 1;
    }

    server_loop();

    return 0;
}
