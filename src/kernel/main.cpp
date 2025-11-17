#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>
#include "early/serial_16550.hpp"
#include "acpi/acpi.hpp"
#include "console.hpp"
#include "platform_traits.hpp"
#include "time/monotonic.hpp"
#include "time/calibrate.hpp"
#include "server_spawn.hpp"  // Week 7: Server spawning infrastructure
#include "scheduler.hpp"      // Week 8 Phase 2: Preemptive scheduler
#include "timer.hpp"           // Week 8 Phase 2: Timer interrupts

#ifdef XINIM_ARCH_X86_64
#include "../arch/x86_64/hal/apic.hpp"
#include "../arch/x86_64/hal/hpet.hpp"
#include "../arch/x86_64/hal/ioapic.hpp"
#elif defined(XINIM_ARCH_ARM64)
#include "../arch/arm64/hal/gic.hpp"
#include "../arch/arm64/hal/timer.hpp"
#endif

#include "time/monotonic.hpp"
#include "time/calibrate.hpp"

extern void interrupts_init(xinim::early::Serial16550& serial
#ifdef XINIM_ARCH_X86_64
    , xinim::hal::x86_64::Lapic& lapic
#endif
);

// Platform-agnostic kernel configuration parsing
enum TickMode { TICK_AUTO, TICK_APIC, TICK_HPET, TICK_ARM_TIMER };

static TickMode parse_tick_mode(const char* cmdline) {
    if (!cmdline) return TICK_AUTO;
    const char* p = cmdline;
    const char* key = "tick=";
    while (*p) {
        const char* k = key;
        const char* s = p;
        while (*k && *s == *k) { ++k; ++s; }
        if (*k == '\0') {
            if (s[0]=='a'&&s[1]=='p'&&s[2]=='i'&&s[3]=='c') return TICK_APIC;
            if (s[0]=='h'&&s[1]=='p'&&s[2]=='e'&&s[3]=='t') return TICK_HPET;
            if (s[0]=='a'&&s[1]=='r'&&s[2]=='m') return TICK_ARM_TIMER;
            return TICK_AUTO;
        }
        ++p;
    }
    return TICK_AUTO;
}

static xinim::early::Serial16550 early_serial;

static void kputs(const char* s) {
    early_serial.write(s);
}

// x86_64 specific timer setup
#ifdef XINIM_ARCH_X86_64

static xinim::hal::x86_64::Hpet* g_hpet_ptr = nullptr;

static uint64_t monotonic_from_hpet() {
    if (!g_hpet_ptr) return 0;
    const uint64_t period_fs = g_hpet_ptr->period_fs();
    const uint64_t ticks = g_hpet_ptr->counter();
    return (uint64_t)(((__uint128_t)ticks * (__uint128_t)period_fs) / 1000000ULL);
}

static int parse_hpet_gsi(const char* cmdline) {
    if (!cmdline) return -1;
    const char* p = cmdline;
    const char* key = "hpet_gsi=";
    while (*p) {
        const char* k = key;
        const char* s = p;
        while (*k && *s == *k) { ++k; ++s; }
        if (*k == '\0') {
            int val = 0; bool any=false;
            while (*s >= '0' && *s <= '9') { val = val*10 + (*s - '0'); ++s; any=true; }
            return any ? val : -1;
        }
        ++p;
    }
    return -1;
}

static void setup_x86_64_timers(const xinim::boot::BootInfo& bi, const xinim::acpi::AcpiInfo& acpi) {
    // Week 8: Make lapic static so it persists for timer EOI
    static xinim::hal::x86_64::Lapic lapic;

    if (!acpi.lapic_mmio) {
        kputs("LAPIC not found.\n");
        for(;;) { __asm__ volatile ("hlt"); }
    }

    auto lapic_virt = static_cast<uintptr_t>(bi.hhdm_offset + acpi.lapic_mmio);
    lapic.init(lapic_virt);
    interrupts_init(early_serial, lapic);

    // Week 8: Set LAPIC reference for timer interrupt handler
    xinim::kernel::set_timer_lapic(&lapic);
    
    // Calibrate APIC timer using HPET if present
    uint32_t init_count = 20000000u;
    uint8_t divpow2 = 4;
    TickMode mode = parse_tick_mode(bi.cmdline);
    bool hpet_routed = false;
    
    if (acpi.hpet_mmio) {
        static xinim::hal::x86_64::Hpet hpet;
        auto hpet_virt = static_cast<uintptr_t>(bi.hhdm_offset + acpi.hpet_mmio);
        hpet.init(hpet_virt);
        hpet.enable(true);
        g_hpet_ptr = &hpet;
        xinim::time::monotonic_install(&monotonic_from_hpet);
        
        auto calib = xinim::time::calibrate_apic_with_hpet(lapic, hpet, 100); // 100 Hz
        if (calib.initial_count) {
            init_count = calib.initial_count;
            divpow2 = calib.divider_pow2;
        }
        
        // Try HPET->IOAPIC routing if configured
        int gsi = parse_hpet_gsi(bi.cmdline);
        if ((mode != TICK_APIC) && gsi >= 0 && acpi.ioapic_phys) {
            xinim::hal::x86_64::IoApic ioapic;
            auto ioapic_virt = static_cast<uintptr_t>(bi.hhdm_offset + acpi.ioapic_phys);
            ioapic.init(ioapic_virt, acpi.ioapic_gsi_base);
            ioapic.redirect((uint32_t)gsi, 32, false, false);
            
            if (hpet.start_periodic(0, 10'000'000ULL, (uint32_t)gsi)) {
                hpet_routed = true;
            }
        }
    }
    
    if (mode == TICK_HPET && hpet_routed) {
        lapic.stop_timer();
    } else {
        lapic.setup_timer(32, init_count, divpow2);
    }
    
    __asm__ volatile ("sti");
}

#elif defined(XINIM_ARCH_ARM64)

static xinim::hal::arm64::Timer* g_timer_ptr = nullptr;

static uint64_t monotonic_from_arm_timer() {
    if (!g_timer_ptr) return 0;
    return g_timer_ptr->get_nanoseconds();
}

static void setup_arm64_timers(const xinim::boot::BootInfo& bi) {
    // Initialize GIC (Generic Interrupt Controller)
    static xinim::hal::arm64::Gic gic;
    gic.init(bi.gic_dist_base, bi.gic_cpu_base);
    
    // Initialize ARM generic timer
    static xinim::hal::arm64::Timer timer;
    timer.init();
    g_timer_ptr = &timer;
    
    // Install monotonic clock source
    xinim::time::monotonic_install(&monotonic_from_arm_timer);
    
    // Set up timer interrupt at 100Hz
    timer.setup_periodic(100);
    
    // Enable interrupts
    __asm__ volatile ("msr daifclr, #2");
}

#endif

/**
 * @brief Unified kernel entry function.
 *
 * This function serves as the main entry point for the XINIM kernel on both
 * x86_64 and ARM64 architectures. It initializes the boot console, probes
 * hardware via ACPI/device tree, sets up interrupt controllers and timers,
 * and then enters the kernel main loop.
 *
 * The implementation harmonizes both the advanced Limine boot path with
 * full hardware initialization and the simpler stub approach for testing.
 *
 * @pre Executed in early boot context with interrupts disabled.
 * @post Never returns in production; may return 0 for testing.
 */
extern "C" void _start() {
    // Initialize early serial console
    early_serial = xinim::early::Serial16550(0x3F8);
    early_serial.init();
    
    // Print startup banner
    kputs("\n==============================================\n");
    kputs("XINIM: Modern C++23 Post-Quantum Microkernel\n");
    kputs("==============================================\n");
    
#ifdef XINIM_BOOT_LIMINE
    kputs("Boot: Limine protocol detected\n");
    
    // Gather boot information
    auto bi = xinim::boot::from_limine();
    
    // Platform detection
#ifdef XINIM_ARCH_X86_64
    kputs("Architecture: x86_64\n");
    
    // ACPI discovery for x86_64
    auto acpi = xinim::acpi::probe(reinterpret_cast<uint64_t>(bi.acpi_rsdp), bi.hhdm_offset);
    kputs("ACPI: Tables parsed\n");
    
    // Set up x86_64 specific hardware
    setup_x86_64_timers(bi, acpi);
    kputs("Timers: Initialized (APIC/HPET)\n");
    
#elif defined(XINIM_ARCH_ARM64)
    kputs("Architecture: ARM64\n");
    
    // Set up ARM64 specific hardware
    setup_arm64_timers(bi);
    kputs("Timers: Initialized (ARM Generic Timer)\n");
#endif
    
    // Use advanced console if available
    Console::init();
    Console::printf("XINIM kernel fully initialized\n");
    
#else
    // Simple stub mode for testing
    kputs("Boot: Stub mode (testing)\n");
    Console::printf("XINIM kernel stub\n");
#endif

    // ========================================
    // Week 8 Phase 2: Initialize Scheduler
    // ========================================
    kputs("\n");
    kputs("========================================\n");
    kputs("Week 8: Initializing Scheduler\n");
    kputs("========================================\n");

    xinim::kernel::initialize_scheduler();

    // ========================================
    // Week 8: Spawn System Servers
    // ========================================
    kputs("\n");
    kputs("========================================\n");
    kputs("Week 8: Spawning System Servers\n");
    kputs("========================================\n");

    // Initialize and spawn all system servers (VFS, Process Mgr, Memory Mgr)
    if (xinim::kernel::initialize_system_servers() != 0) {
        kputs("[FATAL] Failed to spawn system servers\n");
        for(;;) {
#ifdef XINIM_ARCH_X86_64
            __asm__ volatile ("hlt");
#elif defined(XINIM_ARCH_ARM64)
            __asm__ volatile ("wfi");
#endif
        }
    }

    // Optionally spawn init process (PID 1)
    // Note: This will fail in Week 7 (no ELF loader yet)
    xinim::kernel::spawn_init_process("/sbin/init");

    kputs("\n");
    kputs("========================================\n");
    kputs("XINIM is now running!\n");
    kputs("========================================\n");
    kputs("\n");

    // ========================================
    // Week 8 Phase 2: Initialize Timer
    // ========================================
    kputs("Initializing timer interrupts...\n");
    initialize_timer(100);  // 100 Hz (10ms per tick)

    // ========================================
    // Enter Scheduler Loop (Never Returns)
    // ========================================
    kputs("Starting preemptive scheduler...\n");

    // Week 8: Call preemptive scheduler (never returns)
    // This enables interrupts and starts the first process
    xinim::kernel::schedule_forever();

    // Should never reach here
    kputs("[FATAL] Scheduler returned unexpectedly\n");
    for(;;) {
#ifdef XINIM_ARCH_X86_64
        __asm__ volatile ("hlt");
#elif defined(XINIM_ARCH_ARM64)
        __asm__ volatile ("wfi");
#endif
    }
}

// Alternative entry point for environments that expect main()
int main() noexcept {
    _start();
    return 0;
}
