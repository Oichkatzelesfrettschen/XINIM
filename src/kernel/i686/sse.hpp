#pragma once
// SSE / FPU context management for i686 (Pentium III+).
// WHY: SYSENTER/SYSEXIT does not save or restore FPU/SSE state.  The kernel
//      must save the full 512-byte FXSAVE image on every kernel entry that
//      could touch floating-point state and restore it on the way out.
//      Without this, user processes corrupt each other's FPU/SSE registers.
//
// FXSAVE/FXRESTORE (Intel SDM Vol. 1 Ch. 10):
//   - 512-byte image, must be 16-byte aligned.
//   - Saves x87 FPU + MMX + XMM0-XMM7 + MXCSR.
//   - Requires CR4.OSFXSR (bit 9) and CR4.OSXMMEXCPT (bit 10) to be set.
//   - Available when CPUID leaf 1 EDX bit 24 (FXSR) is set.
//
// Usage pattern per context switch:
//   fpu_save_context(prev->fxsave_buf);
//   fpu_restore_context(next->fxsave_buf);

#include <stdint.h>

namespace xinim::i686::sse {

// Size and alignment of the FXSAVE image.
static constexpr uint32_t kFxsaveSize      = 512U;
static constexpr uint32_t kFxsaveAlignment = 16U;

// Enable OSFXSR and OSXMMEXCPT in CR4.
// Must be called after initialize_cpuid() confirms FXSR support.
void initialize_sse() noexcept;

// Save the full FPU/SSE state into buf.
// buf must be 16-byte aligned and kFxsaveSize bytes long.
void fpu_save_context(uint8_t* buf) noexcept;

// Restore the full FPU/SSE state from buf.
// buf must be 16-byte aligned and kFxsaveSize bytes long.
void fpu_restore_context(const uint8_t* buf) noexcept;

// Zero-initialise a fresh FXSAVE image (MXCSR = 0x1F80, FPU tag = 0xFF).
// Call when allocating a new process so it starts with a clean FPU state.
void fpu_init_context(uint8_t* buf) noexcept;

} // namespace xinim::i686::sse
