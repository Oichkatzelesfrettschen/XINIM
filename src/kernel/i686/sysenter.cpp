#include "sysenter.hpp"
#include "../i486/ring3_internal.hpp" // g_tss, kKernelCodeSelector

namespace xinim::i686::sysenter {

// MSR addresses (Intel SDM Vol. 2B).
static constexpr uint32_t kMsrSysenterCs  = 0x174U;
static constexpr uint32_t kMsrSysenterEsp = 0x175U;
static constexpr uint32_t kMsrSysenterEip = 0x176U;

static void wrmsr(uint32_t msr, uint64_t value) noexcept {
    __asm__ volatile("wrmsr"
                     :: "c"(msr),
                        "a"(static_cast<uint32_t>(value & 0xFFFFFFFFU)),
                        "d"(static_cast<uint32_t>(value >> 32U)));
}

void update_sysenter_esp(uint32_t kernel_stack_top) noexcept {
    wrmsr(kMsrSysenterEsp, static_cast<uint64_t>(kernel_stack_top));
}

void initialize_sysenter() noexcept {
    using namespace xinim::i486::ring3;

    // SYSENTER_CS: sets kernel CS to selector 0x08 (ring 0 code).
    // The CPU derives kernel SS as SYSENTER_CS + 8 (= 0x10, kernel data).
    // User CS is derived as SYSENTER_CS + 16 (= 0x18 + 3 = 0x1B, ring 3 code).
    // User SS is derived as SYSENTER_CS + 24 (= 0x23, ring 3 data).
    wrmsr(kMsrSysenterCs,  static_cast<uint64_t>(kKernelCodeSelector));

    // SYSENTER_ESP: kernel stack top (same as TSS.ESP0 after each context switch).
    // WHY: We point it at the bootstrap stack here; ring3.cpp updates g_tss.esp0
    // before each user return, which is the canonical kernel stack pointer.
    wrmsr(kMsrSysenterEsp,
          static_cast<uint64_t>(
              reinterpret_cast<uintptr_t>(g_tss.esp0)));

    // SYSENTER_EIP: our fast-path entry point.
    wrmsr(kMsrSysenterEip,
          static_cast<uint64_t>(
              reinterpret_cast<uintptr_t>(i686_sysenter_entry)));
}

} // namespace xinim::i686::sysenter
