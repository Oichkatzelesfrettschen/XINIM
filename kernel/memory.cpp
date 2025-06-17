/* This file contains the drivers for four special files:
 *     /dev/null	- null device (data sink)
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/ram		- RAM disk
 * It accepts three messages, for reading, for writing, and for
 * control. All use message format m2 and with these parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_IOCTL | device  |         |  blocks | ram org |         |
 * ----------------------------------------------------------------
 *
 *
 * The file contains one entry point:
 *
 *   mem_task:	main entry when system is brought up
 *
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "const.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <array>   // for std::array
#include <cstddef> // For std::size_t
#include <cstdint> // For uint64_t
#include <utility>

/// Number of RAM-type devices managed by the driver
constexpr std::size_t NR_RAMS = 4;

/// Message exchange buffer
static message mess;

/// Origin of each RAM disk (phys_bytes -> uint64_t)
static std::array<uint64_t, NR_RAMS> ram_origin{};
/// Limit of RAM disk per minor device (phys_bytes -> uint64_t)
static std::array<uint64_t, NR_RAMS> ram_limit{};

/**
 * @class MessageReply
 * @brief RAII helper to send a reply message when leaving scope.
 */
class MessageReply {
  public:
    MessageReply(int caller, int proc) noexcept : caller_{caller}, proc_{proc} {}
    ~MessageReply() noexcept {
        mess.m_type = TASK_REPLY;
        rep_proc_nr(mess) = proc_;
        rep_status(mess) = result;
        send(caller_, &mess);
    }

    int result{OK};

  private:
    int caller_;
    int proc_;
};

/**
 * @brief Entry point for the memory driver task.
 *
 * Waits for driver messages and services /dev/null, /dev/mem,
 * /dev/kmem and /dev/ram requests.
 */
PUBLIC void mem_task() noexcept {

    int r, caller, proc_nr;
    extern unsigned int sizes[8]; // Explicitly unsigned int
    extern uint64_t
    get_base() noexcept; // Assuming get_base returns phys_clicks -> uint64_t and is noexcept

    /* Initialize this task. */
    // get_base() returns uint64_t (phys_clicks). CLICK_SHIFT is int.
    ram_origin[KMEM_DEV] = get_base() << CLICK_SHIFT;
    // sizes[] are unsigned int. ram_limit is uint64_t[].
    ram_limit[KMEM_DEV] = (static_cast<uint64_t>(sizes[0]) + static_cast<uint64_t>(sizes[1]))
                          << CLICK_SHIFT;
    ram_limit[MEM_DEV] = MEM_BYTES; // MEM_BYTES is uint64_t const

    /* Here is the main loop of the memory task.  It waits for a message, carries
     * it out, and sends a reply.
     */
    while (TRUE) {
        /* First wait for a request to read or write. */
        receive(ANY, &mess);
        if (mess.m_source < 0)
            panic("mem task got message from ", mess.m_source);
        caller = mess.m_source;
        proc_nr = proc_nr(mess);
        MessageReply reply{caller, proc_nr};

        /* Now carry out the work.  It depends on the opcode. */
        switch (mess.m_type) {
        case DISK_READ:
            r = do_mem(&mess);
            break;
        case DISK_WRITE:
            r = do_mem(&mess);
            break;
        case DISK_IOCTL:
            r = do_setup(&mess);
            break;
        default:
            r = ErrorCode::EINVAL;
            break;
        }

        reply.result = r;
    }
}
/**
 * @brief Handle a read or write request for the memory devices.
 *
 * @param m_ptr Message describing the operation.
 * @return Number of bytes transferred or a negative ErrorCode.
 */
[[nodiscard]] static int do_mem(message *m_ptr) noexcept {
    /* Read or write /dev/null, /dev/mem, /dev/kmem, or /dev/ram. */

    int device;
    std::size_t count;            // Was int, for byte counts
    uint64_t mem_phys, user_phys; // phys_bytes -> uint64_t
    struct proc *rp;
    // extern phys_clicks get_base(); // get_base is uint64_t
    // extern phys_bytes umap(); // umap returns uint64_t

    /* Get minor device number and check for /dev/null. */
    device = device(*m_ptr);
    if (device < 0 || device >= NR_RAMS)
        return (ErrorCode::ENXIO); /* bad minor device */
    if (device == NULL_DEV)        // NULL_DEV is int
        return (m_ptr->m_type == DISK_READ ? EOF : static_cast<int>(count(*m_ptr)));

    /* Set up 'mem_phys' for /dev/mem, /dev/kmem, or /dev/ram. */
    // m_ptr->POSITION (m2_l1) is int64_t. ram_origin is uint64_t[].
    if (position(*m_ptr) < 0)
        return (ErrorCode::ENXIO);
    mem_phys = ram_origin[device] + static_cast<uint64_t>(position(*m_ptr));
    if (mem_phys >= ram_limit[device]) // ram_limit is uint64_t[]
        return (EOF);

    count = static_cast<std::size_t>(count(*m_ptr));
    if (mem_phys + count > ram_limit[device]) { // count is std::size_t
        count = static_cast<std::size_t>(ram_limit[device] - mem_phys);
    }

    /* Determine address where data is to go or to come from. */
    rp = proc_addr(proc_nr(*m_ptr));
    // umap takes (proc*, int, std::size_t, std::size_t) returns uint64_t
    // address macro gets m2_p1 (char*)
    user_phys = umap(rp, D, reinterpret_cast<std::size_t>(address(*m_ptr)), count);
    if (user_phys == 0)
        return (ErrorCode::E_BAD_ADDR);

    /* Copy the data. */
    // phys_copy (klib88 version) takes (uintptr_t dst, uintptr_t src, size_t n)
    // phys_copy (klib64 version) takes (void* dst, const void* src, size_t n)
    // Assuming a common phys_copy that can take uintptr_t for physical addresses.
    if (m_ptr->m_type == DISK_READ) {
        phys_copy(static_cast<uintptr_t>(user_phys), static_cast<uintptr_t>(mem_phys), count);
    } else {
        phys_copy(static_cast<uintptr_t>(mem_phys), static_cast<uintptr_t>(user_phys), count);
    }
    return static_cast<int>(count); // Return int count
}
/**
 * @brief Configure parameters for a RAM disk.
 *
 * @param m_ptr Message describing the disk configuration.
 * @return OK on success or a negative ErrorCode.
 */
[[nodiscard]] static int do_setup(message *m_ptr) noexcept {
    int device;

    device = device(*m_ptr);
    if (device < 0 || device >= NR_RAMS)
        return (ErrorCode::ENXIO); /* bad minor device */
    // ram_origin is uint64_t[]. POSITION (m2_l1) is int64_t.
    ram_origin[device] = static_cast<uint64_t>(position(*m_ptr));
    // ram_limit is uint64_t[]. COUNT (m2_i1) is int. BLOCK_SIZE is int.
    ram_limit[device] = static_cast<uint64_t>(position(*m_ptr)) +
                        static_cast<uint64_t>(static_cast<int64_t>(count(*m_ptr)) * BLOCK_SIZE);
    return (OK);
}
