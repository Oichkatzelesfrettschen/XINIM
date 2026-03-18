#include "sse.hpp"

#include <string.h>

namespace xinim::i686::sse {

// ---------------------------------------------------------------------------
// CR4 manipulation -- set OSFXSR (bit 9) and OSXMMEXCPT (bit 10).
// ---------------------------------------------------------------------------

static uint32_t read_cr4() noexcept {
    uint32_t val = 0U;
    __asm__ volatile("movl %%cr4, %0" : "=r"(val));
    return val;
}

static void write_cr4(uint32_t val) noexcept {
    __asm__ volatile("movl %0, %%cr4" :: "r"(val));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initialize_sse() noexcept {
    // Set CR4.OSFXSR (bit 9): allow FXSAVE/FXRESTORE at all privilege levels
    // and indicate the OS manages FPU state.
    // Set CR4.OSXMMEXCPT (bit 10): allow unmasked SIMD floating-point
    // exceptions to generate #XF instead of #UD.
    const uint32_t cr4 = read_cr4();
    write_cr4(cr4 | (1U << 9U) | (1U << 10U));
}

void fpu_save_context(uint8_t* buf) noexcept {
    // FXSAVE writes 512 bytes to the 16-byte-aligned address in the operand.
    // WHY: The "m" constraint generates an effective address; the compiler
    //      handles the alignment guarantee because buf comes from
    //      alignas(16) uint8_t fxsave_buf[512] in Process.
    __asm__ volatile("fxsave %0" : "=m"(*buf));
}

void fpu_restore_context(const uint8_t* buf) noexcept {
    __asm__ volatile("fxrstor %0" :: "m"(*buf));
}

void fpu_init_context(uint8_t* buf) noexcept {
    // Zero the entire image, then set the two fields that must be non-zero
    // for a clean initial state:
    //   MXCSR (offset 24): 0x1F80 -- mask all SSE exceptions, round-to-nearest
    //   FTW   (offset 4):  0xFFFF -- x87 tag word: all registers empty
    memset(buf, 0, kFxsaveSize);

    // MXCSR at byte offset 24 (little-endian uint32_t).
    buf[24] = 0x80U;
    buf[25] = 0x1FU;
    buf[26] = 0x00U;
    buf[27] = 0x00U;

    // FTW (abridged tag word) at byte offset 4 (uint16_t).
    // Value 0xFF means all 8 x87 stack registers are empty.
    buf[4] = 0xFFU;
    buf[5] = 0x00U;
}

} // namespace xinim::i686::sse
