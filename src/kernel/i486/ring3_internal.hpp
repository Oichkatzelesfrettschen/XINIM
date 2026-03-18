#pragma once
// Shared type definitions for the decomposed i486 ring3 kernel modules.
// This header is internal to the ring3 subsystem -- do not include from
// outside src/kernel/i486/.

#include <stddef.h>
#include <stdint.h>

#include "bootfs.hpp"
#include "elf32_loader.hpp"
#include "../scheduler_policy.hpp"
#include "../recovery/recovery_dag.hpp"
#include "../recovery/service_node.hpp"
#include "xinim/sys/syscalls.h"

namespace xinim::i486::ring3 {

// -- Segment selectors and core constants ----------------------------------

constexpr uint16_t kKernelCodeSelector = 0x08U;
constexpr uint16_t kKernelDataSelector = 0x10U;
constexpr uint16_t kUserCodeSelector = 0x1BU;
constexpr uint16_t kUserDataSelector = 0x23U;
constexpr uint16_t kTssSelector = 0x28U;
constexpr uint8_t kTimerVector = 32U;
constexpr uint8_t kSyscallVector = 0x80U;
constexpr uint32_t kKernelStackSize = 8192U;
constexpr size_t kMaxProcesses = 8U;
constexpr uint32_t kUserEflags = 0x202U;
constexpr uint32_t kPageSize = 4096U;
constexpr uint32_t kMaxExecArgs = 16U;
constexpr uint32_t kMaxExecEnvs = 16U;
constexpr uint32_t kMaxExecStringBytes = 512U;
constexpr uint32_t kWaitNoHang = 1U;
constexpr uint32_t kWaitUntraced = 2U;
constexpr int32_t kWaitPidAny = -1;
constexpr uint32_t kHeapGuardBytes = 64U * 1024U;
constexpr uint32_t kAuxvTagNull = 0U;
constexpr uint32_t kAuxvTagPageSize = 6U;
constexpr int kMaxFds = 64;
constexpr uint32_t kMaxUserMappings = 16U;
constexpr uint32_t kMapPrivate = 0x02U;
constexpr uint32_t kMapFixed = 0x10U;
constexpr uint32_t kMapAnonymous = 0x20U;
constexpr uint32_t kErrnoPerm = static_cast<uint32_t>(-1);
constexpr uint32_t kErrnoNoSys = static_cast<uint32_t>(-38);
constexpr uint32_t kErrnoIntr = static_cast<uint32_t>(-4);
constexpr uint32_t kErrnoNoEnt = static_cast<uint32_t>(-2);
constexpr uint32_t kErrnoAcces = static_cast<uint32_t>(-13);
constexpr uint32_t kErrnoBadF = static_cast<uint32_t>(-9);
constexpr uint32_t kErrnoChild = static_cast<uint32_t>(-10);
constexpr uint32_t kErrnoNoMem = static_cast<uint32_t>(-12);
constexpr uint32_t kErrnoFault = static_cast<uint32_t>(-14);
constexpr uint32_t kErrnoInvalid = static_cast<uint32_t>(-22);
constexpr uint32_t kErrnoNoTTY = static_cast<uint32_t>(-25);

// -- PIC/PIT constants -----------------------------------------------------

constexpr uint16_t kPic1CommandPort = 0x20U;
constexpr uint16_t kPic1DataPort = 0x21U;
constexpr uint16_t kPic2CommandPort = 0xA0U;
constexpr uint16_t kPic2DataPort = 0xA1U;
constexpr uint16_t kPitChannel0Port = 0x40U;
constexpr uint16_t kPitModePort = 0x43U;
constexpr uint8_t kPicInitialize = 0x11U;
constexpr uint8_t kPic8086Mode = 0x01U;
constexpr uint8_t kPicEoi = 0x20U;
constexpr uint32_t kTimerHz = 100U;
constexpr uint32_t kPitInputHz = 1193182U;

// -- Supervised service constants ------------------------------------------

constexpr size_t kMaxSupervisedServices = 8U;
constexpr uint8_t kInitServiceMaxRestarts = 16U;
constexpr uint8_t kHoldServiceMaxRestarts = 4U;
constexpr uint32_t kInitServicePriority = xinim::kernel::sched_policy::PRIO_USER_NORM;
constexpr uint32_t kSupportServicePriority = xinim::kernel::sched_policy::PRIO_USER_LOW;

// -- Signal constants ------------------------------------------------------

constexpr uint32_t kSigHup = 1U;
constexpr uint32_t kSigInt = 2U;
constexpr uint32_t kSigQuit = 3U;
constexpr uint32_t kSigIll = 4U;
constexpr uint32_t kSigAbrt = 6U;
constexpr uint32_t kSigKill = 9U;
constexpr uint32_t kSigSegv = 11U;
constexpr uint32_t kSigPipe = 13U;
constexpr uint32_t kSigAlrm = 14U;
constexpr uint32_t kSigTerm = 15U;
constexpr uint32_t kSigChld = 17U;
constexpr uint32_t kSigCont = 18U;
constexpr uint32_t kSigStop = 19U;
constexpr uint32_t kSigTstp = 20U;
constexpr uint32_t kSigTtin = 21U;
constexpr uint32_t kSigVtalrm = 26U;
constexpr uint32_t kSigProf = 27U;
constexpr uint32_t kMaxSignals = 32U;

constexpr uint32_t kSigDfl = 0U;
constexpr uint32_t kSigIgn = 1U;

constexpr uint32_t kSaRestart = 0x10000000U;
constexpr uint32_t kSaNodefer = 0x40000000U;
constexpr uint32_t kSaResethand = 0x80000000U;
constexpr uint32_t kSaNocldwait = 0x00000002U;

// -- Struct definitions ----------------------------------------------------

struct TimeVal32 {
    uint32_t seconds;
    uint32_t microseconds;
};

struct TimeSpec32 {
    uint32_t seconds;
    uint32_t nanoseconds;
};

struct RLimit32 {
    uint32_t current;
    uint32_t maximum;
};

struct RUsage32 {
    TimeVal32 user_time;
    TimeVal32 system_time;
    int32_t max_rss;
    int32_t integral_shared_rss;
    int32_t integral_unshared_data;
    int32_t integral_unshared_stack;
    int32_t minor_faults;
    int32_t major_faults;
    int32_t swaps;
    int32_t block_inputs;
    int32_t block_outputs;
    int32_t messages_sent;
    int32_t messages_received;
    int32_t signals_received;
    int32_t voluntary_context_switches;
    int32_t involuntary_context_switches;
};

struct UserMapping {
    bool in_use;
    uint32_t address;
    uint32_t size;
};

struct [[gnu::packed]] GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct [[gnu::packed]] TssEntry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt_selector;
    uint16_t trap;
    uint16_t iomap_base;
};

struct [[gnu::packed]] GdtDescriptor {
    uint16_t size;
    uint32_t offset;
};

struct [[gnu::packed]] IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
};

struct [[gnu::packed]] IdtDescriptor {
    uint16_t size;
    uint32_t offset;
};

struct RegisterFrame {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
};

struct UserContext {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
};

struct SignalHandler32 {
    uint32_t handler;
    uint32_t flags;
    uint32_t mask;
    uint32_t restorer;
};

struct SignalState32 {
    SignalHandler32 handlers[kMaxSignals];
    uint32_t pending;
    uint32_t blocked;
    bool in_handler;
    uint32_t saved_mask;
};

struct [[gnu::packed]] SignalFrame32 {
    uint32_t sigreturn_trampoline[2];
    uint32_t signum;
    UserContext saved_context;
    uint32_t saved_mask;
};

enum class ProcessState : uint8_t {
    Empty = 0,
    Runnable = 1,
    Waiting = 2,
    Exited = 3,
    Stopped = 4,
};

enum class WaitReason : uint8_t {
    None = 0,
    ConsoleInput = 1,
    PipeIO = 2,
    SleepTick = 3,
    TcpConnect = 4,
};

constexpr uint32_t kMaxGroups = 16U;

struct Credentials {
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    uint32_t suid;  // saved set-user-ID
    uint32_t sgid;  // saved set-group-ID
    uint32_t groups[kMaxGroups];
    uint32_t ngroups;
};

struct Process {
    bool in_use;
    uint32_t pid;
    uint32_t ppid;
    ProcessState state;
    uint32_t exit_status;
    uint32_t file_creation_mask;
    Credentials cred;
    uint32_t minimum_break;
    uint32_t current_break;
    uint32_t saved_kernel_esp;
    uint32_t segment_base;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t quantum_ticks;
    uint16_t scheduler_domain;
    WaitReason wait_reason;
    int wait_tcp_conn;     // TCP connection index for TcpConnect wait
    uint64_t wake_tick;
    uint32_t ticks_remaining;
    uint32_t pgid;
    uint64_t alarm_tick;

    // Interval timers (per POSIX setitimer/getitimer)
    // Each stores deadline tick and reload interval in scheduler ticks.
    struct IntervalTimer {
        uint64_t deadline;   // 0 = inactive
        uint64_t interval;   // 0 = one-shot
    };
    IntervalTimer itimer_real;    // ITIMER_REAL  -> SIGALRM
    IntervalTimer itimer_virtual; // ITIMER_VIRTUAL -> SIGVTALRM
    IntervalTimer itimer_prof;    // ITIMER_PROF -> SIGPROF

    int ctty_slot;
    char cwd[256];
    char exe_path[128]; // path passed to most recent successful execve
    SignalState32 signals;
    int fd_map[kMaxFds];
    int fd_flags[kMaxFds];
    UserMapping mappings[kMaxUserMappings];
    UserContext context;
    alignas(16) uint8_t kernel_stack[kKernelStackSize];
    alignas(16) uint8_t address_space[elf32::kUserAddressSpaceSize];
};

template <uint32_t Count>
struct ExecVector {
    uint32_t count;
    const char* values[Count];
    char storage[kMaxExecStringBytes];
    uint32_t used_bytes;
};

enum class ServiceLaunchMode : uint8_t {
    BootOnly = 0,
    OnDemand = 1,
};

struct SupervisedService {
    bool in_use;
    uint32_t service_id;
    char name[xinim::kernel::recovery::MAX_SERVICE_NAME];
    const bootfs::FileRecord* file;
    const char* path;
    const char* env;
    uint32_t pid;
    xinim::kernel::recovery::RestartPolicy restart_policy;
    xinim::kernel::recovery::ServiceState state;
    uint8_t restart_count;
    uint8_t max_restarts;
    bool respawn_on_clean_exit;
    bool critical;
    uint32_t owner_pid;
    int dag_index;
    ServiceLaunchMode launch_mode;
    uint32_t priority;
    uint32_t base_priority;
    uint32_t quantum_ticks;
    uint16_t scheduler_domain;
    bool run_announced;
};

struct SigAction32User {
    uint32_t sa_handler;
    uint32_t sa_flags;
    uint32_t sa_restorer;
    uint32_t sa_mask;
};

// -- Inline I/O helpers ----------------------------------------------------

inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb_port(uint16_t port) noexcept {
    uint8_t value = 0U;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// -- Assembly entry points -------------------------------------------------

extern "C" void i486_load_gdt(uint32_t descriptor) noexcept;
extern "C" void i486_load_idt(uint32_t descriptor) noexcept;
extern "C" void i486_load_tss(uint16_t selector) noexcept;
extern "C" void i486_resume_user_context(const UserContext* context) noexcept;
extern "C" void i486_switch_to_user_context(const UserContext* context,
                                             uint32_t* saved_kernel_esp) noexcept;
extern "C" void i486_resume_saved_kernel_stack(uint32_t saved_kernel_esp) noexcept;
extern "C" void i486_syscall_entry() noexcept;
extern "C" void i486_timer_irq_entry() noexcept;
extern "C" void i486_fault_ud_entry() noexcept;
extern "C" void i486_fault_gp_entry() noexcept;
extern "C" void i486_fault_pf_entry() noexcept;

// -- Global state (defined in ring3.cpp) -----------------------------------

extern GdtEntry g_gdt[7];
extern TssEntry g_tss;
extern IdtEntry g_idt[256];
extern uint8_t g_bootstrap_kernel_stack[kKernelStackSize];
extern Process g_processes[kMaxProcesses];
extern SupervisedService g_supervised_services[kMaxSupervisedServices];
extern xinim::kernel::recovery::RecoveryDag g_service_recovery_dag;
extern const xinim::boot::BootInfo* g_boot_info;
extern Process* g_current_process;
extern uint32_t g_next_pid;
extern uint32_t g_next_service_id;
extern uint64_t g_scheduler_ticks;
extern uint32_t g_boot_epoch_seconds;
extern uint64_t g_boot_epoch_ticks;

// -- Supervised service lookup (defined in ring3.cpp) ----------------------

[[nodiscard]] SupervisedService* find_supervised_service_by_process(
    const Process* process) noexcept;

} // namespace xinim::i486::ring3
