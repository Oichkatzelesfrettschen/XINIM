#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>
#include "early/serial_16550.hpp"
#include "acpi/acpi.hpp"
#include "../arch/x86_64/hal/apic.hpp"
#include "../arch/x86_64/hal/hpet.hpp"
#include "../arch/x86_64/hal/ioapic.hpp"
#include "time/monotonic.hpp"
#include "../arch/x86_64/hal/hpet.hpp"
#include "time/calibrate.hpp"
extern void interrupts_init(xinim::early::Serial16550& serial, xinim::hal::x86_64::Lapic& lapic);

extern "C" void _start();
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

enum TickMode { TICK_AUTO, TICK_APIC, TICK_HPET };
static TickMode parse_tick_mode(const char* cmdline) {
    if (!cmdline) return TICK_AUTO;
    const char* p = cmdline;
    const char* key = "tick=";
    while (*p) {
        const char* k = key; const char* s = p;
        while (*k && *s == *k) { ++k; ++s; }
        if (*k == '\0') {
            if (s[0]=='a'&&s[1]=='p'&&s[2]=='i'&&s[3]=='c') return TICK_APIC;
            if (s[0]=='h'&&s[1]=='p'&&s[2]=='e'&&s[3]=='t') return TICK_HPET;
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

extern "C" void _start() {
    early_serial = xinim::early::Serial16550(0x3F8);
    early_serial.init();
    kputs("\nXINIM: Limine boot reached.\n");
    kputs("Early serial OK.\n");

    // Gather boot info via Limine
    auto bi = xinim::boot::from_limine();
    // ACPI discovery
    auto acpi = xinim::acpi::probe(reinterpret_cast<uint64_t>(bi.acpi_rsdp), bi.hhdm_offset);
    // Initialize LAPIC and IDT, then set up periodic timer (HPET via IOAPIC if configured, else LAPIC)
    xinim::hal::x86_64::Lapic lapic;
    if (acpi.lapic_mmio) {
        auto lapic_virt = static_cast<uintptr_t>(bi.hhdm_offset + acpi.lapic_mmio);
        lapic.init(lapic_virt);
        interrupts_init(early_serial, lapic);
        // Calibrate APIC timer using HPET if present and install monotonic source
        uint32_t init_count = 20000000u; uint8_t divpow2 = 4;
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
            if (calib.initial_count) { init_count = calib.initial_count; divpow2 = calib.divider_pow2; }
            // Try HPET->IOAPIC routing if ioapic present and cmdline provides hpet_gsi
            int gsi = parse_hpet_gsi(bi.cmdline);
            if ((mode != TICK_APIC) && gsi >= 0 && acpi.ioapic_phys) {
                xinim::hal::x86_64::IoApic ioapic;
                auto ioapic_virt = static_cast<uintptr_t>(bi.hhdm_offset + acpi.ioapic_phys);
                ioapic.init(ioapic_virt, acpi.ioapic_gsi_base);
                // Redirect HPET GSI to vector 32, edge/high
                ioapic.redirect((uint32_t)gsi, 32, false, false);
                // Program HPET periodic timer 0 for 100Hz
                if (hpet.start_periodic(0, 10'000'000ULL, (uint32_t)gsi)) {
                    hpet_routed = true;
                }
            }
        }
        if (mode == TICK_HPET && hpet_routed) {
            // Prefer HPET: mask LAPIC timer to avoid duplicate ticks
            lapic.stop_timer();
        } else {
            // Fallback or forced APIC tick
            lapic.setup_timer(32, init_count, divpow2);
        }
        __asm__ volatile ("sti");
        for(;;) { __asm__ volatile ("hlt"); }
    } else {
        kputs("LAPIC not found.\n");
        for(;;) { __asm__ volatile ("hlt"); }
    }
}
