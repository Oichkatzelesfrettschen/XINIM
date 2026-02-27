/* This task handles the interface between file system and kernel as well as
 * between memory manager and kernel.
 */

#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/signal.hpp"
#include "sys/type.hpp" 
#include "const.hpp"
#include "glo.hpp"
#include "proc.hpp" 
#include "type.hpp"
#include "lib.hpp"
#include "panic.hpp" 
#include <algorithm>
#include <array>
#include <cstddef> 
#include <cstdint>

#define COPY_UNIT 65534L 

// Forward declarations for system call handlers.
static int do_fork(message *m_ptr) noexcept;
static int do_newmap(message *m_ptr) noexcept;
static int do_exec(message *m_ptr) noexcept;
static int do_xit(message *m_ptr) noexcept;
static int do_getsp(message *m_ptr) noexcept;
static int do_times(message *m_ptr) noexcept;
static int do_abort(message *m_ptr) noexcept;
static int do_sig(message *m_ptr) noexcept;
static int do_copy(message *m_ptr) noexcept;

extern "C" {
    void phys_copy(void *dst, const void *src, size_t n) noexcept;
    void build_sig(struct sig_info *dst, struct proc *rp, int sig) noexcept;
    void set_name(int proc_nr, char *ptr) noexcept;
    void ready(struct proc *rp) noexcept;
    void unready(struct proc *rp) noexcept;
    void pick_proc() noexcept;
    int mini_send(int caller, int dest, message *m_ptr) noexcept;
    int mini_rec(int caller, int src, message *m_ptr) noexcept;
    void sys_task() noexcept;
    void cause_sig(int proc_nr, int sig_nr) noexcept;
    void inform(int proc_nr) noexcept;
    uint64_t umap(struct proc *rp, int seg, std::size_t vir_addr, std::size_t bytes) noexcept;
}

/**
 * @brief Enumerates supported system call identifiers.
 */
enum class SysCall : int {
    Fork = SYS_FORK,     
    NewMap = SYS_NEWMAP, 
    Exec = SYS_EXEC,     
    Xit = SYS_XIT,       
    GetSp = SYS_GETSP,   
    Times = SYS_TIMES,   
    Abort = SYS_ABORT,   
    Sig = SYS_SIG,       
    Copy = SYS_COPY      
};

using SysHandler = int (*)(message *) noexcept;

constinit std::array<std::pair<SysCall, SysHandler>, 9> kSysDispatch{
    std::pair{SysCall::Fork, do_fork},   std::pair{SysCall::NewMap, do_newmap},
    std::pair{SysCall::Exec, do_exec},   std::pair{SysCall::Xit, do_xit},
    std::pair{SysCall::GetSp, do_getsp}, std::pair{SysCall::Times, do_times},
    std::pair{SysCall::Abort, do_abort}, std::pair{SysCall::Sig, do_sig},
    std::pair{SysCall::Copy, do_copy},
};

PRIVATE message m;
PRIVATE char sig_stuff[SIG_PUSH_BYTES];

/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
extern "C" void sys_task() noexcept {
    int r{};
    while (true) {
        ipc_receive(SYSTASK, &m);
        const auto type = static_cast<SysCall>(m.m_type);
        if (const auto it = std::ranges::find(kSysDispatch, type, &std::pair<SysCall, SysHandler>::first);
            it != kSysDispatch.end()) {
            r = it->second(&m);
        } else {
            r = static_cast<int>(ErrorCode::E_BAD_FCN);
        }
        m.m_type = r;         
        ipc_send(m.m_source, &m); 
    }
}

static int do_fork(message *m_ptr) noexcept {
    struct proc *rpc;
    char *sptr, *dptr; 
    int k1, k2, pid, bytes;
    std::uint64_t tok;

    k1 = proc1(*m_ptr);  
    k2 = proc2(*m_ptr);  
    pid = ::pid(*m_ptr); 
    tok = token(*m_ptr); 

    if (k1 < 0 || k1 >= NR_PROCS || k2 < 0 || k2 >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rpc = proc_addr(k2);

    sptr = (char *)proc_addr(k1); 
    dptr = (char *)proc_addr(k2); 
    bytes = sizeof(struct proc);  
    while (bytes--) *dptr++ = *sptr++; 

    rpc->p_flags |= NO_MAP;  
    rpc->p_pid = pid;        
    rpc->p_reg[RET_REG] = 0; 
    rpc->p_token = tok;      

    rpc->user_time = 0; 
    rpc->sys_time = 0;
    rpc->child_utime = 0;
    rpc->child_stime = 0;
    return (OK);
}

static int do_newmap(message *m_ptr) noexcept {
    struct proc *rp, *rsrc;
    uint64_t src_phys, dst_phys, pn; 
    std::size_t vmm, vsys, vn;       
    int caller, k, old_flags;                   
    struct mem_map *map_ptr;         

    caller = m_ptr->m_source;
    k = proc1(*m_ptr);
    map_ptr = reinterpret_cast<struct mem_map *>(mem_ptr(*m_ptr));
    if (k < -NR_TASKS || k >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);                     
    rsrc = proc_addr(caller);              
    vn = NR_SEGS * sizeof(struct mem_map); 
    pn = static_cast<uint64_t>(vn);        
    vmm = reinterpret_cast<std::size_t>(map_ptr);
    vsys = reinterpret_cast<std::size_t>(rp->p_map); 
    if ((src_phys = umap(rsrc, D, vmm, vn)) == 0)
        kpanic("bad call to sys_newmap (src)");
    if ((dst_phys = umap(proc_addr(SYSTASK), D, vsys, vn)) == 0)
        kpanic("bad call to sys_newmap (dst)");
    phys_copy(reinterpret_cast<void *>(static_cast<uintptr_t>(dst_phys)),
              reinterpret_cast<const void *>(static_cast<uintptr_t>(src_phys)),
              static_cast<std::size_t>(pn));

    old_flags = rp->p_flags; 
    rp->p_flags &= ~NO_MAP;
    if (old_flags != 0 && rp->p_flags == 0)
        ready(rp);
    return (OK);
}

static int do_exec(message *m_ptr) noexcept {
    struct proc *rp;
    int k;            
    uintptr_t sp_val; 
    std::uint64_t tok;

    k = proc1(*m_ptr); 
    sp_val = reinterpret_cast<uintptr_t>(stack_ptr(*m_ptr));
    tok = token(*m_ptr);
    if (k < 0 || k >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);
    rp->p_sp = static_cast<uint64_t>(sp_val); 
    rp->p_pcpsw.pc = 0;                       
    rp->p_alarm = 0;           
    rp->p_flags &= ~static_cast<int>(RECEIVING); 
    if (rp->p_flags == 0)
        ready(rp);
    rp->p_token = tok; 
    set_name(k, stack_ptr(*m_ptr));   
    return (OK);
}

static int do_xit(message *m_ptr) noexcept {
    struct proc *rp, *rc;
    struct proc *np, *xp;
    int parent, proc_nr; 

    parent = proc1(*m_ptr);  
    proc_nr = proc2(*m_ptr); 
    if (parent < 0 || parent >= NR_PROCS || proc_nr < 0 || proc_nr >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(parent);
    rc = proc_addr(proc_nr);
    rp->child_utime += rc->user_time + rc->child_utime; 
    rp->child_stime += rc->sys_time + rc->child_stime;
    unready(rc);
    rc->p_alarm = 0;              
    set_name(proc_nr, (char *)nullptr); 

    if (rc->p_flags & static_cast<int>(SENDING)) {
        for (rp = &proc[0]; rp < &proc[NR_TASKS + NR_PROCS]; rp++) {
            if (rp->p_callerq == nullptr) continue;
            if (rp->p_callerq == rc) {
                rp->p_callerq = rc->p_sendlink;
                break;
            } else {
                np = rp->p_callerq;
                while ((xp = np->p_sendlink) != nullptr)
                    if (xp == rc) {
                        np->p_sendlink = xp->p_sendlink;
                        break;
                    } else {
                        np = xp;
                    }
            }
        }
    }
    rc->p_flags = static_cast<int>(P_SLOT_FREE);
    return (OK);
}

static int do_getsp(message *m_ptr) noexcept {
    struct proc *rp;
    int k; 
    k = proc1(*m_ptr);
    if (k < 0 || k >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);
    stack_ptr(m) = reinterpret_cast<char *>(static_cast<uintptr_t>(rp->p_sp));
    return (OK);
}

static int do_times(message *m_ptr) noexcept {
    struct proc *rp;
    int k;
    k = proc1(*m_ptr); 
    if (k < 0 || k >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(k);
    user_time(*m_ptr) = rp->user_time;
    system_time(*m_ptr) = rp->sys_time;
    child_utime(*m_ptr) = rp->child_utime;
    child_stime(*m_ptr) = rp->child_stime;
    return (OK);
}

static int do_abort(message *m_ptr) noexcept {
    (void)m_ptr;       
    kpanic("System Abort"); 
    return OK;         
}

static int do_sig(message *m_ptr) noexcept {
    struct proc *rp;
    uint64_t src_phys, dst_phys;            
    std::size_t vir_addr, sig_size, new_sp; 
    int proc_nr, sig;                                
    int (*sig_handler)();                   
    std::uint64_t tok;

    proc_nr = pr(*m_ptr);       
    sig = signum(*m_ptr);       
    sig_handler = func(*m_ptr); 
    tok = token(*m_ptr);
    if (proc_nr < LOW_USER || proc_nr >= NR_PROCS)
        return static_cast<int>(ErrorCode::E_BAD_PROC);
    rp = proc_addr(proc_nr);
    if (tok != rp->p_token)
        return static_cast<int>(ErrorCode::EACCES);
    vir_addr = reinterpret_cast<std::size_t>(sig_stuff); 
    new_sp = static_cast<std::size_t>(rp->p_sp); 

    build_sig(reinterpret_cast<sig_info *>(sig_stuff), rp, sig); 

    sig_size = SIG_PUSH_BYTES; 
    new_sp -= sig_size;
    src_phys = umap(proc_addr(SYSTASK), D, vir_addr, sig_size);
    dst_phys = umap(rp, S, new_sp, sig_size);
    if (dst_phys == 0)
        kpanic("do_sig can't signal; SP bad"); 
    phys_copy(reinterpret_cast<void *>(static_cast<uintptr_t>(dst_phys)),
              reinterpret_cast<const void *>(static_cast<uintptr_t>(src_phys)),
              sig_size); 

    rp->p_sp = static_cast<uint64_t>(new_sp); 
    rp->p_pcpsw.pc = static_cast<xinim::virt_addr_t>(reinterpret_cast<uintptr_t>(sig_handler));
    return (OK);
}

static int do_copy(message *m_ptr) noexcept {
    int src_proc, dst_proc, src_seg, dst_seg;
    std::size_t src_vir, dst_vir;       
    uint64_t src_phys, dst_phys, bytes; 

    src_proc = src_proc_nr(*m_ptr);
    dst_proc = dst_proc_nr(*m_ptr);
    src_seg = static_cast<int>(src_space(*m_ptr));
    dst_seg = static_cast<int>(dst_space(*m_ptr));
    src_vir = static_cast<std::size_t>(src_buffer(*m_ptr));
    dst_vir = static_cast<std::size_t>(dst_buffer(*m_ptr));
    bytes = static_cast<uint64_t>(copy_bytes(*m_ptr));

    if (src_proc == ABS)
        src_phys = static_cast<uint64_t>(static_cast<std::uintptr_t>(src_buffer(*m_ptr)));
    else
        src_phys = umap(proc_addr(src_proc), src_seg, src_vir, static_cast<std::size_t>(bytes));

    if (dst_proc == ABS)
        dst_phys = static_cast<uint64_t>(static_cast<std::uintptr_t>(dst_buffer(*m_ptr)));
    else
        dst_phys = umap(proc_addr(dst_proc), dst_seg, dst_vir, static_cast<std::size_t>(bytes));

    if (src_phys == 0 || dst_phys == 0)
        return static_cast<int>(ErrorCode::EFAULT);
    phys_copy(reinterpret_cast<void *>(static_cast<uintptr_t>(dst_phys)),
              reinterpret_cast<const void *>(static_cast<uintptr_t>(src_phys)),
              static_cast<std::size_t>(bytes));
    return (OK);
}

extern "C" void cause_sig(int proc_nr, int sig_nr) noexcept {
    struct proc *rp;
    rp = proc_addr(proc_nr);
    if (rp->p_pending == 0)
        sig_procs++; 
    rp->p_pending |= 1 << (sig_nr - 1);
    inform(MM_PROC_NR); 
}

extern "C" void inform(int proc_nr) noexcept {
    struct proc *rp, *mmp;
    mmp = proc_addr(proc_nr);
    if (((mmp->p_flags & static_cast<int>(RECEIVING)) == 0) || mmp->p_getfrom != ANY)
        return;
    for (rp = proc_addr(0); rp < proc_addr(NR_PROCS); rp++)
        if (rp->p_pending != 0) {
            m.m_type = KSIG; 
            m.m1_i1() = static_cast<int>(rp - proc - NR_TASKS);
            sig_map(m) = rp->p_pending;
            sig_procs--;
            if (mini_send(HARDWARE, proc_nr, &m) != OK)
                kpanic("can't inform MM"); 
            rp->p_pending = 0;                    
            return;
        }
}

extern "C" uint64_t umap(struct proc *rp, int seg, std::size_t vir_addr, std::size_t bytes) noexcept {
    if (bytes == 0) return 0;   
    (void)rp; (void)seg;
    return static_cast<uint64_t>(vir_addr);
}