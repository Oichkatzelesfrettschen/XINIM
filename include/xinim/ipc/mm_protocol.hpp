/**
 * @file mm_protocol.hpp
 * @brief Memory Manager IPC Protocol Structures
 *
 * This header defines the request and response structures used for
 * communication between the kernel syscall dispatcher and the Memory Manager.
 *
 * @ingroup ipc
 */

#ifndef XINIM_IPC_MM_PROTOCOL_HPP
#define XINIM_IPC_MM_PROTOCOL_HPP

#include <cstdint>
#include "message_types.h"

namespace xinim::ipc {

/* ========================================================================
 * Heap Management Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to set program break (heap boundary)
 * Message type: MM_BRK
 */
struct MmBrkRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t new_brk;        /**< New heap break address (0 = query current) */

    MmBrkRequest() : caller_pid(0), new_brk(0) {}
    MmBrkRequest(int32_t pid, uint64_t brk) : caller_pid(pid), new_brk(brk) {}
} __attribute__((packed));

/**
 * @brief Response to brk request
 * Message type: MM_REPLY
 */
struct MmBrkResponse {
    uint64_t current_brk;    /**< Current heap break address */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    MmBrkResponse() : current_brk(0), result(-1), error(IPC_ENOSYS) {}
    MmBrkResponse(uint64_t brk, int32_t r, int32_t e)
        : current_brk(brk), result(r), error(e) {}
} __attribute__((packed));

/* ========================================================================
 * Memory Mapping Protocol Structures
 * ======================================================================== */

/**
 * @brief Memory mapping protection flags
 */
enum class MmapProt : uint32_t {
    PROT_NONE  = 0x0,        /**< Page cannot be accessed */
    PROT_READ  = 0x1,        /**< Page can be read */
    PROT_WRITE = 0x2,        /**< Page can be written */
    PROT_EXEC  = 0x4,        /**< Page can be executed */
};

/**
 * @brief Memory mapping flags
 */
enum class MmapFlags : uint32_t {
    MAP_SHARED    = 0x01,    /**< Share changes */
    MAP_PRIVATE   = 0x02,    /**< Changes are private */
    MAP_FIXED     = 0x10,    /**< Map at exact address */
    MAP_ANONYMOUS = 0x20,    /**< Don't use a file */
    MAP_GROWSDOWN = 0x0100,  /**< Stack-like segment */
    MAP_LOCKED    = 0x2000,  /**< Lock pages in memory */
    MAP_NORESERVE = 0x4000,  /**< Don't reserve swap space */
    MAP_POPULATE  = 0x8000,  /**< Populate page tables */
};

/**
 * @brief Request to map memory region
 * Message type: MM_MMAP
 */
struct MmMmapRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Preferred address (0 = kernel chooses) */
    uint64_t length;         /**< Length of mapping */
    uint32_t prot;           /**< Protection flags (MmapProt) */
    uint32_t flags;          /**< Mapping flags (MmapFlags) */
    int32_t fd;              /**< File descriptor (-1 for anonymous) */
    uint64_t offset;         /**< File offset */

    MmMmapRequest() : caller_pid(0), addr(0), length(0), prot(0),
                      flags(0), fd(-1), offset(0) {}
} __attribute__((packed));

/**
 * @brief Response to mmap request
 * Message type: MM_REPLY
 */
struct MmMmapResponse {
    uint64_t mapped_addr;    /**< Mapped address (on success) */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    MmMmapResponse() : mapped_addr(0), result(-1), error(IPC_ENOSYS) {}
    MmMmapResponse(uint64_t addr, int32_t r, int32_t e)
        : mapped_addr(addr), result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to unmap memory region
 * Message type: MM_MUNMAP
 */
struct MmMunmapRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address of mapping */
    uint64_t length;         /**< Length of mapping */

    MmMunmapRequest() : caller_pid(0), addr(0), length(0) {}
    MmMunmapRequest(int32_t pid, uint64_t a, uint64_t len)
        : caller_pid(pid), addr(a), length(len) {}
} __attribute__((packed));

/**
 * @brief Response to munmap request
 * Message type: MM_REPLY
 */
struct MmMunmapResponse {
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    MmMunmapResponse() : result(-1), error(IPC_ENOSYS) {}
    MmMunmapResponse(int32_t r, int32_t e) : result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to change memory protection
 * Message type: MM_MPROTECT
 */
struct MmMprotectRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address */
    uint64_t length;         /**< Length of region */
    uint32_t prot;           /**< New protection flags (MmapProt) */

    MmMprotectRequest() : caller_pid(0), addr(0), length(0), prot(0) {}
    MmMprotectRequest(int32_t pid, uint64_t a, uint64_t len, uint32_t p)
        : caller_pid(pid), addr(a), length(len), prot(p) {}
} __attribute__((packed));

/**
 * @brief Response to mprotect request
 * Message type: MM_REPLY
 */
struct MmMprotectResponse {
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    MmMprotectResponse() : result(-1), error(IPC_ENOSYS) {}
    MmMprotectResponse(int32_t r, int32_t e) : result(r), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to sync memory mapping to file
 * Message type: MM_MSYNC
 */
struct MmMsyncRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address */
    uint64_t length;         /**< Length of region */
    uint32_t flags;          /**< Sync flags (MS_ASYNC, MS_SYNC, MS_INVALIDATE) */

    MmMsyncRequest() : caller_pid(0), addr(0), length(0), flags(0) {}
} __attribute__((packed));

/**
 * @brief Request to lock memory pages
 * Message type: MM_MLOCK
 */
struct MmMlockRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address */
    uint64_t length;         /**< Length of region */

    MmMlockRequest() : caller_pid(0), addr(0), length(0) {}
} __attribute__((packed));

/**
 * @brief Request to unlock memory pages
 * Message type: MM_MUNLOCK
 */
struct MmMunlockRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address */
    uint64_t length;         /**< Length of region */

    MmMunlockRequest() : caller_pid(0), addr(0), length(0) {}
} __attribute__((packed));

/**
 * @brief Memory advice flags
 */
enum class MadviseAdvice : uint32_t {
    MADV_NORMAL     = 0,     /**< No special treatment */
    MADV_RANDOM     = 1,     /**< Random page references */
    MADV_SEQUENTIAL = 2,     /**< Sequential page references */
    MADV_WILLNEED   = 3,     /**< Will need these pages */
    MADV_DONTNEED   = 4,     /**< Don't need these pages */
    MADV_FREE       = 8,     /**< Free pages (but keep mapping) */
    MADV_REMOVE     = 9,     /**< Remove pages from backing store */
    MADV_DONTFORK   = 10,    /**< Don't inherit across fork */
    MADV_DOFORK     = 11,    /**< Do inherit across fork */
    MADV_MERGEABLE  = 12,    /**< KSM may merge identical pages */
    MADV_UNMERGEABLE = 13,   /**< Never merge with KSM */
    MADV_HUGEPAGE   = 14,    /**< Use transparent huge pages */
    MADV_NOHUGEPAGE = 15,    /**< Don't use transparent huge pages */
};

/**
 * @brief Request to give memory advice
 * Message type: MM_MADVISE
 */
struct MmMadviseRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t addr;           /**< Start address */
    uint64_t length;         /**< Length of region */
    uint32_t advice;         /**< Advice (MadviseAdvice) */

    MmMadviseRequest() : caller_pid(0), addr(0), length(0), advice(0) {}
} __attribute__((packed));

/* ========================================================================
 * Shared Memory Protocol Structures
 * ======================================================================== */

/**
 * @brief Shared memory flags
 */
enum class ShmFlags : uint32_t {
    IPC_CREAT  = 0x0200,     /**< Create if key doesn't exist */
    IPC_EXCL   = 0x0400,     /**< Fail if key exists */
    SHM_RDONLY = 0x1000,     /**< Attach read-only */
    SHM_RND    = 0x2000,     /**< Round attach address to SHMLBA */
};

/**
 * @brief Request to get shared memory segment
 * Message type: MM_SHMGET
 */
struct MmShmgetRequest {
    int32_t caller_pid;      /**< PID of calling process */
    int32_t key;             /**< Shared memory key */
    uint64_t size;           /**< Size of segment */
    uint32_t flags;          /**< Creation flags */

    MmShmgetRequest() : caller_pid(0), key(0), size(0), flags(0) {}
} __attribute__((packed));

/**
 * @brief Response to shmget request
 * Message type: MM_REPLY
 */
struct MmShmgetResponse {
    int32_t shmid;           /**< Shared memory ID (>= 0 on success, -1 on error) */
    int32_t error;           /**< Error code */

    MmShmgetResponse() : shmid(-1), error(IPC_ENOSYS) {}
    MmShmgetResponse(int32_t id, int32_t e) : shmid(id), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to attach shared memory
 * Message type: MM_SHMAT
 */
struct MmShmatRequest {
    int32_t caller_pid;      /**< PID of calling process */
    int32_t shmid;           /**< Shared memory ID */
    uint64_t shmaddr;        /**< Preferred address (0 = kernel chooses) */
    uint32_t flags;          /**< Attach flags */

    MmShmatRequest() : caller_pid(0), shmid(-1), shmaddr(0), flags(0) {}
} __attribute__((packed));

/**
 * @brief Response to shmat request
 * Message type: MM_REPLY
 */
struct MmShmatResponse {
    uint64_t attached_addr;  /**< Attached address */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    MmShmatResponse() : attached_addr(0), result(-1), error(IPC_ENOSYS) {}
} __attribute__((packed));

/**
 * @brief Request to detach shared memory
 * Message type: MM_SHMDT
 */
struct MmShmdtRequest {
    int32_t caller_pid;      /**< PID of calling process */
    uint64_t shmaddr;        /**< Attached address */

    MmShmdtRequest() : caller_pid(0), shmaddr(0) {}
} __attribute__((packed));

/* ========================================================================
 * Memory Information Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to get page size
 * Message type: MM_GETPAGESIZE
 */
struct MmGetpagesizeRequest {
    int32_t caller_pid;      /**< PID of calling process */

    MmGetpagesizeRequest() : caller_pid(0) {}
} __attribute__((packed));

/**
 * @brief Response to getpagesize request
 * Message type: MM_REPLY
 */
struct MmGetpagesizeResponse {
    uint64_t page_size;      /**< Page size in bytes */
    int32_t result;          /**< 0 on success */
    int32_t error;           /**< Error code */

    MmGetpagesizeResponse() : page_size(4096), result(0), error(IPC_SUCCESS) {}
} __attribute__((packed));

/* ========================================================================
 * Generic Response Structure
 * ======================================================================== */

/**
 * @brief Generic memory manager response for simple operations
 * Message type: MM_REPLY
 */
struct MmGenericResponse {
    int32_t result;          /**< Operation result */
    int32_t error;           /**< Error code */
    int64_t extra_data;      /**< Optional extra return value */

    MmGenericResponse() : result(-1), error(IPC_ENOSYS), extra_data(0) {}
    MmGenericResponse(int32_t r, int32_t e, int64_t x = 0)
        : result(r), error(e), extra_data(x) {}
} __attribute__((packed));

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * @brief Check if a message type is a memory manager message
 */
inline constexpr bool is_mm_message(int msg_type) noexcept {
    return msg_type >= 300 && msg_type < 400;
}

/**
 * @brief Get human-readable name for memory manager message type
 */
inline const char* mm_message_name(int msg_type) noexcept {
    switch (msg_type) {
        case MM_BRK: return "MM_BRK";
        case MM_MMAP: return "MM_MMAP";
        case MM_MUNMAP: return "MM_MUNMAP";
        case MM_MPROTECT: return "MM_MPROTECT";
        case MM_MSYNC: return "MM_MSYNC";
        case MM_MLOCK: return "MM_MLOCK";
        case MM_MUNLOCK: return "MM_MUNLOCK";
        case MM_MADVISE: return "MM_MADVISE";
        case MM_SHMGET: return "MM_SHMGET";
        case MM_SHMAT: return "MM_SHMAT";
        case MM_SHMDT: return "MM_SHMDT";
        case MM_GETPAGESIZE: return "MM_GETPAGESIZE";
        case MM_REPLY: return "MM_REPLY";
        case MM_ERROR: return "MM_ERROR";
        default: return "UNKNOWN_MM_MESSAGE";
    }
}

} // namespace xinim::ipc

#endif /* XINIM_IPC_MM_PROTOCOL_HPP */
