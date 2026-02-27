/* This file contains the drivers for four special files:
 *     /dev/null	- null device (data sink)
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/ram		- RAM disk
 */

#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "proc.hpp"
#include "type.hpp"
#include "panic.hpp"
#include <array>   
#include <cstddef> 
#include <cstdint> 
#include <cstdio>  
#include <memory>  
#include <span>    
#include <utility>

constexpr std::size_t NR_RAMS = 4;
static std::unique_ptr<message> mess = std::make_unique<message>();
static std::array<uint64_t, NR_RAMS> ram_origin{};
static std::array<uint64_t, NR_RAMS> ram_limit{};

class MessageReply {
  public:
    MessageReply(int caller, int proc_id) noexcept : caller_{caller}, proc_{proc_id} {}
    ~MessageReply() noexcept {
        mess->m_type = TASK_REPLY;
        rep_proc_nr(*mess) = proc_;
        rep_status(*mess) = result;
        ipc_send(caller_, mess.get());
    }
    int result{OK};
  private:
    int caller_;
    int proc_;
};

[[nodiscard]] static int do_mem(message *m_ptr) noexcept;
[[nodiscard]] static int do_setup(message *m_ptr) noexcept;

extern "C" void mem_task() noexcept {
    int r, caller, proc_nr_local;
    extern unsigned int sizes[8];
    extern uint64_t get_base() noexcept;

    ram_origin[KMEM_DEV] = get_base() << CLICK_SHIFT;
    ram_limit[KMEM_DEV] = (static_cast<uint64_t>(sizes[0]) + static_cast<uint64_t>(sizes[1])) << CLICK_SHIFT;
    ram_limit[MEM_DEV] = MEM_BYTES; 

    while (TRUE) {
        ipc_receive(ANY, mess.get());
        if (mess->m_source < 0) kpanic("mem task got bad message source");
        caller = mess->m_source;
        proc_nr_local = proc_nr(*mess);
        MessageReply reply{caller, proc_nr_local};

        switch (mess->m_type) {
        case DISK_READ:  r = do_mem(mess.get()); break;
        case DISK_WRITE: r = do_mem(mess.get()); break;
        case DISK_IOCTL: r = do_setup(mess.get()); break;
        default:         r = static_cast<int>(ErrorCode::EINVAL); break;
        }
        reply.result = r;
    }
}

[[nodiscard]] static int do_mem(message *m_ptr) noexcept {
    int minor;
    std::size_t byte_count;       
    uint64_t mem_phys, user_phys; 
    struct proc *rp;

    minor = device(*m_ptr);
    if (minor < 0 || static_cast<std::size_t>(minor) >= NR_RAMS)
        return static_cast<int>(ErrorCode::ENXIO);
    const auto minor_index = static_cast<std::size_t>(minor);
    if (minor == NULL_DEV)
        return (m_ptr->m_type == DISK_READ ? EOF : static_cast<int>(count(*m_ptr)));

    if (position(*m_ptr) < 0) return static_cast<int>(ErrorCode::ENXIO);
    mem_phys = ram_origin[minor_index] + static_cast<uint64_t>(position(*m_ptr));
    if (mem_phys >= ram_limit[minor_index]) return (EOF);

    byte_count = static_cast<std::size_t>(count(*m_ptr));
    if (mem_phys + byte_count > ram_limit[minor_index]) {
        byte_count = static_cast<std::size_t>(ram_limit[minor_index] - mem_phys);
    }

    rp = proc_addr(proc_nr(*m_ptr));
    user_phys = umap(rp, D, reinterpret_cast<std::size_t>(address(*m_ptr)), byte_count);
    if (user_phys == 0) return static_cast<int>(ErrorCode::E_BAD_ADDR);

    if (m_ptr->m_type == DISK_READ) {
        phys_copy(reinterpret_cast<void*>(static_cast<uintptr_t>(user_phys)), 
                  reinterpret_cast<const void*>(static_cast<uintptr_t>(mem_phys)), 
                  byte_count);
    } else {
        phys_copy(reinterpret_cast<void*>(static_cast<uintptr_t>(mem_phys)), 
                  reinterpret_cast<const void*>(static_cast<uintptr_t>(user_phys)), 
                  byte_count);
    }
    return static_cast<int>(byte_count);
}

[[nodiscard]] static int do_setup(message *m_ptr) noexcept {
    int minor;
    minor = device(*m_ptr);
    if (minor < 0 || static_cast<std::size_t>(minor) >= NR_RAMS)
        return static_cast<int>(ErrorCode::ENXIO);
    const auto minor_index = static_cast<std::size_t>(minor);
    ram_origin[minor_index] = static_cast<uint64_t>(position(*m_ptr));
    ram_limit[minor_index] = static_cast<uint64_t>(position(*m_ptr)) +
                       static_cast<uint64_t>(static_cast<int64_t>(count(*m_ptr)) * BLOCK_SIZE);
    return (OK);
}
