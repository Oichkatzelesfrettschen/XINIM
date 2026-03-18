.section .text

// SYSENTER fast-path entry point for i686.
// WHY: SYSENTER hardware convention repurposes ECX (user return EIP) and EDX
//      (user return ESP) for SYSEXIT mechanics, so these registers are NOT
//      available as syscall arguments.  This stub saves them below the
//      RegisterFrame and recovers them for SYSEXIT after the handler returns.
//
// SYSENTER calling convention (set by user-space stub):
//   EAX = syscall number
//   EBX = arg1, ESI = arg2, EDI = arg3, EBP = arg4
//   ECX = user return EIP   (saved for SYSEXIT, NOT arg2)
//   EDX = user return ESP   (saved for SYSEXIT, NOT arg3)
//
// On SYSENTER entry the CPU has:
//   CS  = SYSENTER_CS (0x08, kernel code, CPL=0)
//   ESP = SYSENTER_ESP MSR (kernel stack top, equals g_tss.esp0)
//   IF  = 0  (hardware clears IF)
//   DS/ES/FS/GS = still user values (0x23)

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
    // WHY: ecx/edx slots are zeroed because those registers are unavailable
    //      as syscall args under the SYSENTER convention; the handler reads
    //      args from eax, ebx, esi, edi, ebp only.
    pushl   %gs
    pushl   %fs
    pushl   %es
    pushl   %ds
    pushl   %ebp
    pushl   %edi
    pushl   %esi
    pushl   $0          // edx frame slot: not a syscall arg
    pushl   $0          // ecx frame slot: not a syscall arg
    pushl   %ebx
    pushl   %eax

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

    // Disable interrupts before restoring user context to avoid a race where
    // an IRQ fires after we reload user segments but before SYSEXIT.
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

    // SYSEXIT: CPL 0 -> CPL 3.
    //   CS  = SYSENTER_CS + 16 = 0x18 | RPL3 = 0x1B  (kUserCodeSelector)
    //   SS  = SYSENTER_CS + 24 = 0x20 | RPL3 = 0x23  (kUserDataSelector)
    //   EIP = ECX, ESP = EDX
    sysexit
