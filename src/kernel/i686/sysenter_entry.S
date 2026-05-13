.section .text

// SYSENTER fast-path entry point for i686.
// WHY: SYSENTER hardware convention repurposes ECX (user return EIP) and EDX
//      (user return ESP) for SYSEXIT mechanics, so these registers are NOT
//      available as syscall arguments.  This stub saves them below the
//      RegisterFrame and recovers them for SYSEXIT after the handler returns.
//
// User-space SYSENTER calling convention (syscall_i686.hpp):
//   EAX = syscall number
//   EBX = arg0            (same as INT 0x80)
//   ESI = arg1            (INT 0x80 uses ECX; remapped here)
//   EDI = arg2            (INT 0x80 uses EDX; remapped here)
//   EBP = arg3            (INT 0x80 uses ESI;  remapped here)
//   ECX = user return EIP (saved for SYSEXIT, NOT available as arg)
//   EDX = user return ESP (saved for SYSEXIT, NOT available as arg)
//
// Kernel RegisterFrame layout (struct RegisterFrame in ring3_internal.hpp):
//   eax  ebx  ecx  edx  esi  edi  ebp  ds  es  fs  gs
//
// This stub remaps so the kernel sees the same layout as INT 0x80:
//   frame.ecx <- ESI (arg1)   frame.edx <- EDI (arg2)
//   frame.esi <- EBP (arg3)   frame.edi <- 0   frame.ebp <- 0
//
// On SYSENTER entry the CPU has:
//   CS  = SYSENTER_CS (0x08, kernel code, CPL=0)
//   ESP = SYSENTER_ESP MSR (kernel stack top, equals g_tss.esp0)
//   IF  = 0  (hardware clears IF)
//   DS/ES/FS/GS = still user values (0x23)
//
// Return uses IRET rather than SYSEXIT.  SYSEXIT does not restore EFLAGS; if we
// leave IF cleared, the next userspace timeslice cannot be preempted and
// blocking TTY reads never wake from COM2 input.  Building an IRET frame with
// EFLAGS=0x202 keeps the fast entry path while restoring normal timer IRQs.

.extern i486_handle_syscall

.global i686_sysenter_entry
.type i686_sysenter_entry, @function
i686_sysenter_entry:
    // Save user return EIP/ESP below the RegisterFrame so SYSEXIT can find them.
    // Push EDX first so ECX sits on top (popl %ecx, then popl %edx on return).
    pushl   %edx        // [esp+4] after next push: user ESP for SYSEXIT EDX
    pushl   %ecx        // [esp+0] after next push: user EIP for SYSEXIT ECX

    // Build RegisterFrame (struct RegisterFrame, 11 uint32_t fields).
    // Push in reverse field order so the struct base (eax) lands at esp.
    // Remap ESI->ecx_slot, EDI->edx_slot, EBP->esi_slot so the kernel
    // dispatcher reads the same arg positions as the INT 0x80 path (args 0-3).
    // frame.edi and frame.ebp are zeroed; SYSENTER supports 4 args max.
    pushl   %gs
    pushl   %fs
    pushl   %es
    pushl   %ds
    pushl   $0          // frame.ebp slot: 0 (EBP was consumed for arg3)
    pushl   $0          // frame.edi slot: 0 (no arg4 in SYSENTER path)
    pushl   %ebp        // frame.esi slot: EBP = arg3
    pushl   %edi        // frame.edx slot: EDI = arg2
    pushl   %esi        // frame.ecx slot: ESI = arg1
    pushl   %ebx        // frame.ebx slot: EBX = arg0
    pushl   %eax        // frame.eax slot: EAX = syscall number

    // Load kernel data selectors (0x10 = kKernelDataSelector).
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // Re-enable interrupts.  SYSENTER clears IF; the handler runs with IRQs on.
    sti

    // Call i486_handle_syscall(RegisterFrame *frame).
    pushl   %esp
    call    i486_handle_syscall
    addl    $4, %esp

    // Disable interrupts while constructing the return frame.
    cli

    // Propagate the syscall return value into the frame's eax slot.
    movl    %eax, (%esp)

    // Discard the RegisterFrame: 11 fields * 4 bytes = 44 bytes.
    addl    $44, %esp

    // Recover user return EIP and ESP for SYSEXIT.
    popl    %ecx        // ECX = user return EIP (was saved at entry)
    popl    %edx        // EDX = user return ESP (was saved at entry)

    // Restore user data selectors (0x23 = kUserDataSelector).
    movw    $0x23, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // IRET: CPL 0 -> CPL 3 with IF restored.
    pushl   $0x23       // SS = kUserDataSelector
    pushl   %edx        // ESP = user return ESP
    pushl   $0x202      // EFLAGS = IF set
    pushl   $0x1B       // CS = kUserCodeSelector
    pushl   %ecx        // EIP = user return EIP
    iret
