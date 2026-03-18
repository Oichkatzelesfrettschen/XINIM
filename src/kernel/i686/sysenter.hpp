#pragma once
// SYSENTER/SYSEXIT fast system-call path for i686 (Pentium Pro / P2 / P3).
// WHY: INT 0x80 takes ~100+ cycles due to privilege-level checks, IDT lookup,
// stack switching, and task-state write-back.  SYSENTER/SYSEXIT reduces that
// to ~30-40 cycles by bypassing the full descriptor walk.
//
// Convention (Linux-compatible, also used by our xash and mksh):
//   EAX = syscall number
//   EBX, ECX, EDX, ESI, EDI, EBP = arguments (in that order)
//   On SYSENTER, ECX = user-space EIP to return to (set by call stub),
//                EDX = user-space ESP to return to (set by call stub).
// User-space call stub:
//   movl %esp, %edx    ; save user ESP in EDX
//   leal .Lreturn, %ecx ; save return EIP in ECX
//   sysenter
// .Lreturn:

#include <stdint.h>

namespace xinim::i686::sysenter {

// Program the three SYSENTER MSRs (0x174, 0x175, 0x176).
// Must be called after initialize_protection() sets up GDT and TSS.
void initialize_sysenter() noexcept;

// Update SYSENTER_ESP (MSR 0x175) to match the current process kernel stack.
// Must be called on every context switch so SYSENTER lands on the right stack.
void update_sysenter_esp(uint32_t kernel_stack_top) noexcept;

// Assembly fast-path entry point; defined in sysenter_entry.S.
// Not called directly -- installed as MSR 0x176 (SYSENTER_EIP).
extern "C" void i686_sysenter_entry() noexcept;

} // namespace xinim::i686::sysenter
