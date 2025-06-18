/**
 * @file main.cpp
 * @brief Kernel entry point and trap handlers for x86 (i386, i586, i686, x86_64).
 *
 * Uses C++23, RAII, and platform‐adaptive types for 32- and 64-bit x86.
 */

#include "console.hpp"             // Console::printf(), detect_color()
#include "glo.hpp"                 // extern proc_ptr, bill_ptr
#include "hardware.hpp"            // Hardware::enable_irqs(), reboot()
#include "idt.hpp"                 // IDT::init()
#include "paging.hpp"              // Paging::init()
#include "platform_traits.hpp"     // defines phys_clicks_t, CLICK_SHIFT, etc.
#include "process_table.hpp"       // ProcessTable::instance()
#include "quaternion_spinlock.hpp" // RAII QuaternionSpinlock and QuaternionLockGuard
#include "scheduler.hpp"           // Scheduler::pick(), restart()

#include <array>
#include <cstddef>
#include <cstdint>
#include <inttypes.h> // PRIxPTR

//-----------------------------------------------------------------------------
// Architecture selector
//-----------------------------------------------------------------------------
enum class Architecture : uint8_t {
#if defined(__x86_64__)
    X86_64,
#elif defined(__i686__)
    I686,
#elif defined(__i586__)
    I586,
#elif defined(__i486__)
    I486,
#elif defined(__i386__)
    I386,
#else
#error "Unsupported x86 architecture"
#endif
};
static constexpr Architecture arch =
#if defined(__x86_64__)
    Architecture::X86_64;
#elif defined(__i686__)
    Architecture::I686;
#elif defined(__i586__)
    Architecture::I586;
#elif defined(__i486__)
    Architecture::I486;
#elif defined(__i386__)
    Architecture::I386;
#endif

#ifdef __x86_64__
extern "C" void init_syscall_msrs() noexcept;
#endif

using Traits = PlatformTraits;
using phys_clicks_t = Traits::phys_clicks_t;
using virt_clicks_t = Traits::virt_clicks_t;
static constexpr std::size_t STACK_SAFETY = Traits::SAFETY;
static constexpr phys_clicks_t KERNEL_BASE = Traits::BASE >> Traits::CLICK_SHIFT;

//-----------------------------------------------------------------------------
// Kernel boot sequence
//-----------------------------------------------------------------------------
/**
 * @brief Kernel entry function that initializes subsystems and starts the
 *        scheduler.
 *
 * This function configures the CPU, paging, IDT and process table before
 * enabling interrupts and handing control to the scheduler. The function is
 * expected to never return under normal operation.
 *
 * @return Always returns 0 if control reaches the end.
 */
int main() noexcept {
    // Block interrupts via RAII quaternion spinlock
    hyper::QuaternionSpinlock irq_lock;
    const hyper::Quaternion ticket = hyper::Quaternion::id();
    {
        hyper::QuaternionLockGuard lk{irq_lock, ticket};
        cpu::set_current_cpu(0);
        Paging::init();
        IDT::init();
    }

    // Compute where MM starts in click units
    const phys_clicks_t base_click = KERNEL_BASE;
    const std::size_t kernel_text = Traits::kernel_text_clicks;
    const std::size_t kernel_data = Traits::kernel_data_clicks;
    const phys_clicks_t mm_base = base_click + (kernel_text + kernel_data);

    // Initialize process table
    auto &ptable = ProcessTable::instance();
    ptable.initialize_all(mm_base);

    // Video mode and CPU detection
    Console::set_color(Console::detect_color());
    if (Console::read_bios_cpu_type() == Traits::PC_AT) {
        glo::pc_at = true;
    }

#ifdef __x86_64__
    init_syscall_msrs();
#endif

    // Start scheduling and enable interrupts
    Scheduler::pick();
    Hardware::enable_irqs();
    Scheduler::restart(); // never returns on success

    return 0; // unreachable
}

//-----------------------------------------------------------------------------
// Trap and interrupt handlers
//-----------------------------------------------------------------------------

extern "C" void unexpected_int() noexcept {
    Console::printf("Unexpected interrupt (vector < 16)\n");
    Console::printf("pc = 0x%" PRIxPTR "\n", cpu::current_pc());
}

extern "C" void trap_handler() noexcept {
    Console::printf("\nUnexpected trap (vector >= 16)\n");
    Console::printf("pc = 0x%" PRIxPTR "\n", cpu::current_pc());
}

extern "C" void div_trap() noexcept {
    Console::printf("Divide overflow trap\n");
    Console::printf("pc = 0x%" PRIxPTR "\n", cpu::current_pc());
}

extern "C" void panic(const char *msg, int code) noexcept {
    if (msg && *msg) {
        Console::printf("Kernel panic: %s", msg);
        if (code != Traits::NO_NUM) {
            Console::printf(" %d", code);
        }
        Console::printf("\n");
    }
    Console::printf("Type space to reboot\n");
    Hardware::reboot();
}
