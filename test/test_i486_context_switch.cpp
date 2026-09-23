#include <stdint.h>

extern "C" void i486_switch_process_context(const void*, uint32_t*, uint32_t) noexcept;

namespace {
alignas(16) uint32_t peer_stack[256]{};
uint32_t caller_continuation = 0U;
uint32_t peer_continuation = 0U;
uint32_t peer_visits = 0U;

[[noreturn]] void finish(uint32_t status) noexcept {
    asm volatile("int $0x80" : : "a"(1U), "b"(status) : "memory");
    __builtin_unreachable();
}

extern "C" void peer_entry() noexcept {
    ++peer_visits;
    i486_switch_process_context(nullptr, &peer_continuation, caller_continuation);
    ++peer_visits;
    i486_switch_process_context(nullptr, &peer_continuation, caller_continuation);
    finish(3U);
}
}

// The actual assembly object also contains interrupt entries, which the hosted
// continuation test leaves unreachable. Unexpected use fails the test process.
extern "C" uint32_t i486_handle_syscall(void*) noexcept { finish(4U); }
extern "C" void i486_handle_timer_irq(void*) noexcept { finish(5U); }
extern "C" void i486_handle_fault(uint32_t, uint32_t, uint32_t) noexcept { finish(6U); }

extern "C" [[noreturn]] void _start() noexcept {
    // Saved EBX, ESI, EDI, EBP, then return address. Ret enters peer_entry
    // with ESP mod 16 == 12, the i386 C++ call-entry alignment.
    peer_stack[254] = reinterpret_cast<uint32_t>(&peer_entry);
    i486_switch_process_context(nullptr, &caller_continuation,
                                reinterpret_cast<uint32_t>(&peer_stack[250]));
    if (peer_visits != 1U || caller_continuation == 0U || peer_continuation == 0U) {
        finish(1U);
    }
    i486_switch_process_context(nullptr, &caller_continuation, peer_continuation);
    if (peer_visits != 2U) {
        finish(2U);
    }
    finish(0U);
}
