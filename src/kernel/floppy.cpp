/* This file contains the floppy disk driver.
 */

#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/callnr.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp"
#include "type.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>

struct floppy {
    int fl_procnr;
    int fl_drive;
    int fl_cylinder;
    int fl_sector;
    int fl_head;
    uint64_t fl_low;
    uint64_t fl_size;
    std::size_t fl_count;
    std::size_t fl_address;
};

static struct floppy floppy[4];
static message f_mess;

// Forward declarations
static int do_rdwt(message *m_ptr) noexcept;

extern "C" void floppy_task() noexcept {
    int r, caller, p_nr;
    while (TRUE) {
        ipc_receive(ANY, &f_mess);
        caller = f_mess.m_source;
        p_nr = proc_nr(f_mess);
        switch (f_mess.m_type) {
        case DISK_READ:
        case DISK_WRITE:
            r = do_rdwt(&f_mess);
            break;
        default:
            r = static_cast<int>(ErrorCode::EINVAL);
            break;
        }
        f_mess.m_type = TASK_REPLY;
        rep_proc_nr(f_mess) = p_nr;
        rep_status(f_mess) = r;
        ipc_send(caller, &f_mess);
    }
}

static int do_rdwt(message *m_ptr) noexcept {
    struct floppy *fp;
    uint64_t user_phys;
    std::size_t vir, ct;

    int dev = device(*m_ptr);
    if (dev < 0 || dev >= 4) return static_cast<int>(ErrorCode::EIO);
    fp = &floppy[dev];
    fp->fl_procnr = proc_nr(*m_ptr);
    vir = reinterpret_cast<std::size_t>(address(*m_ptr));
    ct = static_cast<std::size_t>(count(*m_ptr));
    
    user_phys = umap(proc_addr(fp->fl_procnr), D, vir, ct);
    (void)user_phys; 

    return OK;
}

// Dummy use to satisfy -Werror
void floppy_dummy_use() {
    (void)floppy;
}
