#include "process_syscalls.hpp"

#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../server_spawn.hpp"
#include "../../signal.hpp"
#include "../../timer.hpp"
#include "../../uaccess.hpp"
#include "../../unified_scheduler.hpp"
#include "bootfs_syscalls.hpp"
#include "context_restore.hpp"
#include "elf64_user_image.hpp"
#include "exec_arguments.hpp"
#include "signal_syscalls.hpp"
#include "syscall_frame.hpp"
#include "syscall_init.hpp"
#include "tss.hpp"
#include "user_address_space.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xinim/abi/x86_segment_selectors.h>

extern "C" void *malloc(size_t size);
extern "C" void free(void *pointer);

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr uint64_t kKernelStackSize = 16U * 1024U;
        constexpr size_t kPathCapacity = 256U;
        constexpr int64_t kTryAgain = -11;
        constexpr int64_t kNoChild = -10;
        constexpr int64_t kBadAddress = -14;
        constexpr int kWaitNoHang = 1;
        constexpr int kWaitUntraced = 2;
        constexpr int kWaitContinued = 8;
        constexpr uint64_t kSyscallInstructionSize = 2U;

        enum class WaitEvent : uint8_t {
            None,
            Exited,
            Stopped,
            Continued,
        };

        [[nodiscard]] int exec_copy_bytes(void *, void *destination, uintptr_t source,
                                          size_t size) noexcept {
            return copy_from_user(destination, source, size);
        }

        [[nodiscard]] int exec_copy_string(void *, char *destination, uintptr_t source,
                                           size_t capacity) noexcept {
            return copy_string_from_user(destination, source, capacity);
        }

        [[nodiscard]] int64_t exec_copy_errno(ExecCopyError error) noexcept {
            switch (error) {
            case ExecCopyError::None:
                return 0;
            case ExecCopyError::BadAddress:
                return -EFAULT;
            case ExecCopyError::TooLarge:
                return -E2BIG;
            }
            return -EFAULT;
        }

        [[nodiscard]] int64_t elf_load_errno(Elf64LoadError error) noexcept {
            switch (error) {
            case Elf64LoadError::None:
                return 0;
            case Elf64LoadError::NotFound:
                return -ENOENT;
            case Elf64LoadError::InvalidImage:
                return -ENOEXEC;
            case Elf64LoadError::UnsupportedImage:
                return -EINVAL;
            case Elf64LoadError::OutOfMemory:
                return -ENOMEM;
            case Elf64LoadError::ArgumentsTooLarge:
                return -E2BIG;
            }
            return -ENOEXEC;
        }

        void capture_user_context(ProcessControlBlock &process, const SyscallFrame &frame,
                                  uint64_t return_value) noexcept {
            CpuContext &context = process.context;
            context.r15 = frame.r15;
            context.r14 = frame.r14;
            context.r13 = frame.r13;
            context.r12 = frame.r12;
            context.r11 = 0U;
            context.r10 = frame.r10;
            context.r9 = frame.r9;
            context.r8 = frame.r8;
            context.rbp = frame.rbp;
            context.rdi = frame.rdi;
            context.rsi = frame.rsi;
            context.rdx = frame.rdx;
            context.rcx = 0U;
            context.rbx = frame.rbx;
            context.rax = return_value;
            context.gs = XINIM_X86_USER_DS_SELECTOR;
            context.fs = XINIM_X86_USER_DS_SELECTOR;
            context.es = XINIM_X86_USER_DS_SELECTOR;
            context.ds = XINIM_X86_USER_DS_SELECTOR;
            context.rip = frame.rip;
            context.cs = frame.cs;
            context.rflags = frame.rflags;
            context.rsp = frame.rsp;
            context.ss = frame.ss;
            context.cr3 = process.address_space_root;
            const auto *saved_fpu =
                reinterpret_cast<const uint8_t *>(&frame) - XINIM_X86_64_FXSAVE_SIZE;
            std::memcpy(context.fxsave_area, saved_fpu, sizeof(context.fxsave_area));
        }

        [[noreturn]] void switch_to(ProcessControlBlock &process) noexcept {
            deliver_signals_to_context(process, process.context);
            set_kernel_stack(process.kernel_rsp);
            set_syscall_kernel_stack(process.kernel_rsp);
            load_context_ring3(&process.context);
        }

        [[noreturn]] void switch_to_next() noexcept {
            for (;;) {
                ProcessControlBlock *next = g_unified_scheduler.pick_next();
                if (next != nullptr) {
                    switch_to(*next);
                }
                asm volatile("sti; hlt; cli" ::: "memory");
            }
        }

        [[nodiscard]] bool matches_wait_request(const ProcessControlBlock &parent,
                                                const ProcessControlBlock &child,
                                                int requested_pid) noexcept {
            if (child.parent_pid != parent.pid || child.state == ProcessState::DEAD) {
                return false;
            }
            if (requested_pid > 0) {
                return child.pid == requested_pid;
            }
            if (requested_pid == -1) {
                return true;
            }
            if (requested_pid == 0) {
                return child.pgid == parent.pgid;
            }
            return child.pgid == -requested_pid;
        }

        [[nodiscard]] WaitEvent wait_event_for(const ProcessControlBlock &child,
                                               int options) noexcept {
            if (child.state == ProcessState::ZOMBIE) {
                return WaitEvent::Exited;
            }
            if (child.wait_stopped_pending && (options & kWaitUntraced) != 0) {
                return WaitEvent::Stopped;
            }
            if (child.wait_continued_pending && (options & kWaitContinued) != 0) {
                return WaitEvent::Continued;
            }
            return WaitEvent::None;
        }

        void reparent_children(ProcessControlBlock &process) noexcept {
            for (int pid = 1; pid < MAX_PROCESSES; ++pid) {
                ProcessControlBlock *child = find_process_by_pid(pid);
                if (child != nullptr && child->parent_pid == process.pid) {
                    child->parent_pid = 1;
                }
            }
        }

        [[nodiscard]] int encode_wait_status(const ProcessControlBlock &child,
                                             WaitEvent event) noexcept {
            switch (event) {
            case WaitEvent::Exited:
                return child.termination_signal != 0 ? child.termination_signal & 0x7f
                                                     : (child.exit_status & 0xff) << 8;
            case WaitEvent::Stopped:
                return ((child.stop_signal & 0xff) << 8) | 0x7f;
            case WaitEvent::Continued:
                return 0xffff;
            case WaitEvent::None:
                return 0;
            }
            return 0;
        }

        [[nodiscard]] bool deliver_wait_status(ProcessControlBlock &parent,
                                               const ProcessControlBlock &child,
                                               WaitEvent event) noexcept {
            if (parent.wait_status_address == 0U) {
                return true;
            }
            const int wait_status = encode_wait_status(child, event);
            const UserAddressSpace parent_space{parent.address_space_root};
            return copy_to_user_address_space(parent_space, parent.wait_status_address,
                                              &wait_status, sizeof(wait_status));
        }

        void consume_wait_event(ProcessControlBlock &child, WaitEvent event) noexcept {
            if (event == WaitEvent::Exited) {
                child.has_been_waited = true;
            } else if (event == WaitEvent::Stopped) {
                child.wait_stopped_pending = false;
            } else if (event == WaitEvent::Continued) {
                child.wait_continued_pending = false;
            }
        }

        void notify_parent_of_event(ProcessControlBlock &child, WaitEvent event) noexcept {
            ProcessControlBlock *parent = find_process_by_pid(child.parent_pid);
            if (parent == nullptr) {
                return;
            }
            if (parent->state == ProcessState::BLOCKED &&
                parent->blocked_on == BlockReason::WAIT_CHILD &&
                matches_wait_request(*parent, child, parent->wait_target_pid) &&
                wait_event_for(child, parent->wait_options) == event) {
                if (!deliver_wait_status(*parent, child, event)) {
                    parent->context.rax = static_cast<uint64_t>(kBadAddress);
                } else {
                    parent->context.rax = static_cast<uint64_t>(child.pid);
                    consume_wait_event(child, event);
                }
                g_unified_scheduler.unblock(parent->pid);
            }
            const bool suppress_stop_notification =
                (event == WaitEvent::Stopped || event == WaitEvent::Continued) &&
                parent->signal_state != nullptr &&
                (parent->signal_state->handlers[SIGCHLD].flags & SA_NOCLDSTOP) != 0U;
            if (!suppress_stop_notification) {
                static_cast<void>(send_signal(parent, SIGCHLD));
            }
        }

        void release_process(ProcessControlBlock &process) noexcept {
            ProcessControlBlock *parent = find_process_by_pid(process.parent_pid);
            if (parent != nullptr) {
                parent->children_ticks += process.total_ticks + process.children_ticks;
            }
            if (!g_unified_scheduler.remove_process(&process)) {
                return;
            }

            bootfs_release_process(process);
            destroy_signal_state(&process);

            UserAddressSpace address_space{process.address_space_root};
            destroy_user_address_space(address_space);
            process.address_space_root = 0U;
            process.context.cr3 = 0U;

            free(process.stack_base);
            process.stack_base = nullptr;
            process.stack_size = 0U;
            free(process.kernel_stack_base);
            process.kernel_stack_base = nullptr;
            process.kernel_stack_size = 0U;
            process.kernel_rsp = 0U;
            free(&process);
        }

    } // namespace

    void reap_waited_processes() noexcept {
        ProcessControlBlock *current = get_current_process();
        for (int pid = 1; pid < MAX_PROCESSES; ++pid) {
            ProcessControlBlock *process = find_process_by_pid(pid);
            if (process != nullptr && process != current && process->has_been_waited &&
                process->state == ProcessState::ZOMBIE) {
                release_process(*process);
            }
        }
    }

    int64_t process_fork() noexcept {
        ProcessControlBlock *parent = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (parent == nullptr || frame == nullptr || parent->address_space_root == 0U) {
            return kTryAgain;
        }

        const xinim::pid_t child_pid = allocate_process_id();
        if (child_pid < 0) {
            return kTryAgain;
        }
        ProcessControlBlock *child = create_process_control_block(child_pid);
        if (child == nullptr) {
            return kTryAgain;
        }
        const UserAddressSpace parent_space{parent->address_space_root};
        UserAddressSpace child_space{};
        if (!clone_user_address_space(parent_space, child_space)) {
            destroy_signal_state(child);
            free(child);
            return kTryAgain;
        }
        void *kernel_stack = malloc(kKernelStackSize);
        if (kernel_stack == nullptr) {
            destroy_user_address_space(child_space);
            destroy_signal_state(child);
            free(child);
            return kTryAgain;
        }
        std::memset(kernel_stack, 0, kKernelStackSize);

        child->name = child->name_storage.data();
        std::strncpy(child->name_storage.data(), parent->name, child->name_storage.size() - 1U);
        child->name_storage.back() = '\0';
        child->state = ProcessState::READY;
        child->priority = parent->priority;
        child->base_priority = parent->base_priority;
        child->parent_pid = parent->pid;
        child->has_execed = false;
        child->pgid = parent->pgid;
        child->sid = parent->sid;
        child->brk = parent->brk;
        child->minimum_break = parent->minimum_break;
        child->user_mappings = parent->user_mappings;
        child->descriptor_limit = parent->descriptor_limit;
        child->file_creation_mask = parent->file_creation_mask;
        child->nice_value = parent->nice_value;
        child->real_user_id = parent->real_user_id;
        child->effective_user_id = parent->effective_user_id;
        child->saved_user_id = parent->saved_user_id;
        child->real_group_id = parent->real_group_id;
        child->effective_group_id = parent->effective_group_id;
        child->saved_group_id = parent->saved_group_id;
        child->supplementary_groups = parent->supplementary_groups;
        child->supplementary_group_count = parent->supplementary_group_count;
        child->kernel_stack_base = kernel_stack;
        child->kernel_stack_size = kKernelStackSize;
        child->kernel_rsp = reinterpret_cast<uint64_t>(kernel_stack) + kKernelStackSize;
        child->address_space_root = child_space.root_physical;
        if (!copy_signal_state(parent, child)) {
            destroy_user_address_space(child_space);
            destroy_signal_state(child);
            free(kernel_stack);
            free(child);
            return kTryAgain;
        }
        bootfs_clone_process(*parent, *child);
        capture_user_context(*child, *frame, 0U);
        scheduler_add_process(child);
        return child_pid;
    }

    int64_t process_exec(uintptr_t pathname, uintptr_t argv, uintptr_t environment) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr) {
            return kTryAgain;
        }
        char kernel_path[kPathCapacity]{};
        const int path_copy_result =
            copy_string_from_user(kernel_path, pathname, sizeof(kernel_path));
        if (path_copy_result == -ENAMETOOLONG) {
            return -ENAMETOOLONG;
        }
        if (path_copy_result != 0) {
            return -EFAULT;
        }

        auto *copy_storage = static_cast<ExecArguments *>(malloc(sizeof(ExecArguments)));
        if (copy_storage == nullptr) {
            return -ENOMEM;
        }
        std::memset(copy_storage, 0, sizeof(*copy_storage));

        const ExecUserMemoryReader reader{
            nullptr,
            exec_copy_bytes,
            exec_copy_string,
        };
        ExecCopyError copy_result = ExecCopyError::None;
        if (argv != 0U) {
            copy_result = copy_exec_vector(*copy_storage, ExecVectorKind::Arguments, argv, reader);
        } else {
            copy_result = append_exec_string(*copy_storage, ExecVectorKind::Arguments, kernel_path);
        }
        if (copy_result != ExecCopyError::None) {
            free(copy_storage);
            return exec_copy_errno(copy_result);
        }

        constexpr char kDefaultPath[] = "PATH=/bin:/usr/bin";
        constexpr char kDefaultTerm[] = "TERM=xinim";
        if (environment != 0U) {
            copy_result =
                copy_exec_vector(*copy_storage, ExecVectorKind::Environment, environment, reader);
        } else {
            copy_result =
                append_exec_string(*copy_storage, ExecVectorKind::Environment, kDefaultPath);
            if (copy_result == ExecCopyError::None) {
                copy_result =
                    append_exec_string(*copy_storage, ExecVectorKind::Environment, kDefaultTerm);
            }
        }
        if (copy_result != ExecCopyError::None) {
            free(copy_storage);
            return exec_copy_errno(copy_result);
        }

        Elf64UserImage image{};
        const Elf64LoadError load_error = load_bootfs_elf64_user_image(
            kernel_path, image, copy_storage->arguments, copy_storage->environment);
        free(copy_storage);
        if (load_error != Elf64LoadError::None) {
            return elf_load_errno(load_error);
        }

        const UserAddressSpace old_space{process->address_space_root};
        process->address_space_root = image.address_space.root_physical;
        process->brk = image.program_break;
        process->minimum_break = image.program_break;
        process->user_mappings.reset();
        process->context.initialize(image.entry_point, image.stack_pointer, 3);
        process->context.cr3 = image.address_space.root_physical;
        std::strncpy(process->name_storage.data(), kernel_path, process->name_storage.size() - 1U);
        process->name_storage.back() = '\0';
        process->has_execed = true;
        bootfs_close_on_exec(*process);
        reset_signal_handlers(process);

        asm volatile("mov %0, %%cr3" : : "r"(image.address_space.root_physical) : "memory");
        UserAddressSpace releasable_old_space = old_space;
        destroy_user_address_space(releasable_old_space);
        switch_to(*process);
    }

    int64_t process_brk(uintptr_t address) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        if (address == 0U) {
            return static_cast<int64_t>(process->brk);
        }
        if (address < process->minimum_break || address >= kInitialUserStackBottom) {
            return static_cast<int64_t>(process->brk);
        }

        const auto align_up = [](uint64_t value) noexcept {
            return (value + kPageSize - 1U) & ~(kPageSize - 1U);
        };
        const uint64_t old_break = process->brk;
        const uint64_t old_page_end = align_up(old_break);
        const uint64_t new_page_end = align_up(address);
        UserAddressSpace address_space{process->address_space_root};

        if (new_page_end > old_page_end) {
            if (process->user_mappings.overlaps(old_page_end, new_page_end - old_page_end)) {
                return static_cast<int64_t>(old_break);
            }
            uint64_t page = old_page_end;
            for (; page < new_page_end; page += kPageSize) {
                if (!map_zeroed_user_page(address_space, page, UserPageFlags::Writable)) {
                    for (uint64_t rollback = old_page_end; rollback < page; rollback += kPageSize) {
                        static_cast<void>(unmap_user_page(address_space, rollback));
                    }
                    return static_cast<int64_t>(old_break);
                }
            }
        } else {
            for (uint64_t page = new_page_end; page < old_page_end; page += kPageSize) {
                if (!unmap_user_page(address_space, page)) {
                    return static_cast<int64_t>(old_break);
                }
            }
        }
        process->brk = address;
        return static_cast<int64_t>(address);
    }

    int64_t process_setpgid(int pid, int pgid) noexcept {
        ProcessControlBlock *caller = get_current_process();
        if (caller == nullptr) {
            return -ESRCH;
        }
        if (pid < 0 || pgid < 0) {
            return -EINVAL;
        }
        const xinim::pid_t target_pid = pid == 0 ? caller->pid : pid;
        const xinim::pid_t requested_pgid = pgid == 0 ? target_pid : pgid;
        ProcessControlBlock *target = find_process_by_pid(target_pid);
        if (target == nullptr || target->state == ProcessState::DEAD ||
            target->state == ProcessState::ZOMBIE) {
            return -ESRCH;
        }
        if (target != caller && target->parent_pid != caller->pid) {
            return -EPERM;
        }
        if (target->sid != caller->sid || target->sid == target->pid) {
            return -EPERM;
        }
        if (target != caller && target->has_execed) {
            return -EACCES;
        }
        if (requested_pgid != target->pid) {
            bool group_exists_in_session = false;
            for (int candidate_pid = 1; candidate_pid < MAX_PROCESSES; ++candidate_pid) {
                ProcessControlBlock *candidate = find_process_by_pid(candidate_pid);
                if (candidate != nullptr && candidate->pgid == requested_pgid &&
                    candidate->sid == caller->sid && candidate->state != ProcessState::DEAD &&
                    candidate->state != ProcessState::ZOMBIE) {
                    group_exists_in_session = true;
                    break;
                }
            }
            if (!group_exists_in_session) {
                return -EPERM;
            }
        }
        target->pgid = requested_pgid;
        return 0;
    }

    int64_t process_getpgid(int pid) noexcept {
        ProcessControlBlock *caller = get_current_process();
        if (caller == nullptr || pid < 0) {
            return pid < 0 ? -EINVAL : -ESRCH;
        }
        ProcessControlBlock *target =
            pid == 0 ? caller : find_process_by_pid(static_cast<xinim::pid_t>(pid));
        return target != nullptr && target->state != ProcessState::DEAD ? target->pgid : -ESRCH;
    }

    int64_t process_setsid() noexcept {
        ProcessControlBlock *caller = get_current_process();
        if (caller == nullptr) {
            return -ESRCH;
        }
        if (caller->pgid == caller->pid) {
            return -EPERM;
        }
        caller->sid = caller->pid;
        caller->pgid = caller->pid;
        return caller->sid;
    }

    int64_t process_getsid(int pid) noexcept {
        ProcessControlBlock *caller = get_current_process();
        if (caller == nullptr || pid < 0) {
            return pid < 0 ? -EINVAL : -ESRCH;
        }
        ProcessControlBlock *target =
            pid == 0 ? caller : find_process_by_pid(static_cast<xinim::pid_t>(pid));
        return target != nullptr && target->state != ProcessState::DEAD ? target->sid : -ESRCH;
    }

    int64_t process_wait(int child_pid, uintptr_t status_address, int options) noexcept {
        constexpr int kSupportedWaitOptions = kWaitNoHang | kWaitUntraced | kWaitContinued;
        if ((options & ~kSupportedWaitOptions) != 0) {
            return -22;
        }
        ProcessControlBlock *parent = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (parent == nullptr || frame == nullptr) {
            return kNoChild;
        }
        ProcessControlBlock *event_child = nullptr;
        WaitEvent event = WaitEvent::None;
        bool has_matching_child = false;
        for (int pid = 1; pid < MAX_PROCESSES; ++pid) {
            ProcessControlBlock *candidate = find_process_by_pid(pid);
            if (candidate == nullptr || !matches_wait_request(*parent, *candidate, child_pid)) {
                continue;
            }
            has_matching_child = true;
            const WaitEvent candidate_event = wait_event_for(*candidate, options);
            if (candidate_event != WaitEvent::None) {
                event_child = candidate;
                event = candidate_event;
                break;
            }
        }
        if (!has_matching_child) {
            return kNoChild;
        }
        parent->wait_status_address = status_address;
        parent->wait_target_pid = child_pid;
        parent->wait_options = options;
        if (event_child != nullptr) {
            if (!deliver_wait_status(*parent, *event_child, event)) {
                return kBadAddress;
            }
            const xinim::pid_t waited_pid = event_child->pid;
            consume_wait_event(*event_child, event);
            if (event == WaitEvent::Exited) {
                release_process(*event_child);
            }
            return waited_pid;
        }
        if ((options & kWaitNoHang) != 0) {
            return 0;
        }

        capture_user_context(*parent, *frame, 0U);
        if (!g_unified_scheduler.block(parent, BlockReason::WAIT_CHILD, child_pid)) {
            return kTryAgain;
        }
        switch_to_next();
    }

    void wake_io_waiters() noexcept {
        for (int pid = 1; pid < MAX_PROCESSES; ++pid) {
            ProcessControlBlock *process = find_process_by_pid(pid);
            if (process != nullptr && process->state == ProcessState::BLOCKED &&
                (process->blocked_on == BlockReason::IO ||
                 process->blocked_on == BlockReason::SELECT)) {
                g_unified_scheduler.unblock(process->pid);
            }
        }
    }

    void process_block_for_io() noexcept {
        ProcessControlBlock *process = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (process == nullptr || frame == nullptr || frame->rip < kSyscallInstructionSize) {
            switch_to_next();
        }

        capture_user_context(*process, *frame, frame->rax);
        process->context.rip -= kSyscallInstructionSize;
        if (!g_unified_scheduler.block(process, BlockReason::IO, -1)) {
            switch_to(*process);
        }
        switch_to_next();
    }

    void process_block_for_select(uint64_t deadline_tick, uintptr_t timeout_address) noexcept {
        ProcessControlBlock *process = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (process == nullptr || frame == nullptr || frame->rip < kSyscallInstructionSize) {
            switch_to_next();
        }

        capture_user_context(*process, *frame, frame->rax);
        process->context.rip -= kSyscallInstructionSize;
        process->select_deadline_tick = deadline_tick;
        process->select_timeout_address = timeout_address;
        process->select_active = true;
        process->wake_deadline_tick = deadline_tick;
        if (!g_unified_scheduler.block(process, BlockReason::SELECT, -1)) {
            process->select_deadline_tick = 0U;
            process->select_timeout_address = 0U;
            process->select_active = false;
            process->wake_deadline_tick = 0U;
            switch_to(*process);
        }
        switch_to_next();
    }

    void process_sleep_until(uint64_t deadline_tick, uintptr_t remaining_address) noexcept {
        ProcessControlBlock *process = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (process == nullptr || frame == nullptr) {
            switch_to_next();
        }
        capture_user_context(*process, *frame, 0U);
        process->wake_deadline_tick = deadline_tick;
        process->sleep_remaining_address = remaining_address;
        if (!g_unified_scheduler.block(process, BlockReason::TIMER, -1)) {
            process->wake_deadline_tick = 0U;
            process->sleep_remaining_address = 0U;
            switch_to(*process);
        }
        switch_to_next();
    }

    void process_block_for_signal() noexcept {
        ProcessControlBlock *process = get_current_process();
        const SyscallFrame *frame = active_syscall_frame();
        if (process == nullptr || frame == nullptr) {
            switch_to_next();
        }
        capture_user_context(*process, *frame, static_cast<uint64_t>(-EINTR));
        if (!g_unified_scheduler.block(process, BlockReason::WAIT_SIGNAL, -1)) {
            switch_to(*process);
        }
        switch_to_next();
    }

    void process_interrupt_sleep(ProcessControlBlock &process) noexcept {
        if (process.blocked_on != BlockReason::TIMER || process.wake_deadline_tick == 0U) {
            return;
        }
        if (process.sleep_remaining_address != 0U) {
            const uint64_t current_tick = g_unified_scheduler.tick_count();
            const uint64_t remaining_ticks = process.wake_deadline_tick > current_tick
                                                 ? process.wake_deadline_tick - current_tick
                                                 : 0U;
            struct RemainingTime {
                int64_t seconds;
                int64_t nanoseconds;
            };
            const RemainingTime remaining{
                static_cast<int64_t>(remaining_ticks / kSchedulerTicksPerSecond),
                static_cast<int64_t>((remaining_ticks % kSchedulerTicksPerSecond) *
                                     (1000000000ULL / kSchedulerTicksPerSecond)),
            };
            const UserAddressSpace address_space{process.address_space_root};
            static_cast<void>(copy_to_user_address_space(
                address_space, process.sleep_remaining_address, &remaining, sizeof(remaining)));
        }
        process.wake_deadline_tick = 0U;
        process.sleep_remaining_address = 0U;
    }

    void process_interrupt_select(ProcessControlBlock &process) noexcept {
        if (process.blocked_on != BlockReason::SELECT || !process.select_active) {
            return;
        }
        if (process.select_timeout_address != 0U) {
            const uint64_t current_tick = g_unified_scheduler.tick_count();
            const uint64_t remaining_ticks = process.select_deadline_tick > current_tick
                                                 ? process.select_deadline_tick - current_tick
                                                 : 0U;
            struct SelectTimeValue {
                int64_t seconds;
                int64_t microseconds;
            };
            const SelectTimeValue remaining{
                static_cast<int64_t>(remaining_ticks / kSchedulerTicksPerSecond),
                static_cast<int64_t>((remaining_ticks % kSchedulerTicksPerSecond) *
                                     (1000000ULL / kSchedulerTicksPerSecond)),
            };
            const UserAddressSpace address_space{process.address_space_root};
            static_cast<void>(copy_to_user_address_space(
                address_space, process.select_timeout_address, &remaining, sizeof(remaining)));
        }
        process.context.rip += kSyscallInstructionSize;
        process.select_deadline_tick = 0U;
        process.select_timeout_address = 0U;
        process.select_active = false;
        process.wake_deadline_tick = 0U;
    }

    void process_exit(int status) noexcept {
        ProcessControlBlock *child = get_current_process();
        if (child == nullptr) {
            for (;;) {
                asm volatile("cli; hlt");
            }
        }
        bootfs_release_process(*child);
        reparent_children(*child);
        child->exit_status = status;
        child->termination_signal = 0;
        child->has_exited = true;
        child->state = ProcessState::ZOMBIE;

        notify_parent_of_event(*child, WaitEvent::Exited);
        switch_to_next();
    }

    void process_exit_for_signal(int signum) noexcept {
        ProcessControlBlock *child = get_current_process();
        if (child == nullptr) {
            switch_to_next();
        }
        bootfs_release_process(*child);
        reparent_children(*child);
        child->exit_status = 0;
        child->termination_signal = signum;
        child->has_exited = true;
        child->state = ProcessState::ZOMBIE;
        notify_parent_of_event(*child, WaitEvent::Exited);
        switch_to_next();
    }

    void process_stop(int signum) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr) {
            switch_to_next();
        }
        process->stop_signal = signum;
        process->wait_stopped_pending = true;
        g_unified_scheduler.stop(process);
        notify_parent_of_event(*process, WaitEvent::Stopped);
        switch_to_next();
    }

    void process_continue(ProcessControlBlock &process) noexcept {
        if (process.state != ProcessState::STOPPED) {
            return;
        }
        process.wait_continued_pending = true;
        g_unified_scheduler.enqueue(&process);
        notify_parent_of_event(process, WaitEvent::Continued);
    }

} // namespace xinim::kernel::x86_64
