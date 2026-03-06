#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>
#include <cstring>
#include "early/serial_16550.hpp"
#include "acpi/acpi.hpp"
#include "console.hpp"
#include "platform_traits.hpp"
#include "time/monotonic.hpp"
#include "time/calibrate.hpp"
#include "interrupts.hpp"
#include "timer.hpp"
#include "proc.hpp"
#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/tss.hpp"
#include "arch/x86_64/syscall_init.hpp"
#include "arch/x86_64/fpu_init.hpp"
#include "arch/x86_64/cpu_features.hpp"
#include <xinim/pci/pci.hpp>
#include "../drivers/net/virtio_net.hpp"
#include "server_spawn.hpp"

#ifdef XINIM_ARCH_X86_64
#include "hal/x86_64/hal/apic.hpp"
#include "hal/x86_64/hal/hpet.hpp"
#include "hal/x86_64/hal/ioapic.hpp"
#endif

extern xinim::early::Serial16550 early_serial;   // COM1: kernel log
extern xinim::early::Serial16550 kshell_serial;  // COM2: kshell
static xinim::boot::BootInfo g_boot_info;

// Forward declare interrupts_init (defined in interrupts.cpp, no header decl)
void interrupts_init(xinim::early::Serial16550& serial,
                     xinim::hal::x86_64::Lapic& lapic);

namespace xinim::boot {
    const BootInfo& get_info() { return g_boot_info; }
}

static void kputs(const char* s) {
    early_serial.write(s);
}

#ifdef XINIM_ARCH_X86_64
// Static LAPIC instance accessible to both setup_x86_64_timers and _start
static xinim::hal::x86_64::Lapic g_lapic;

static uint64_t monotonic_from_hpet() {
    static xinim::hal::x86_64::Hpet hpet;
    return hpet.counter() * 100; // Stub
}

static void setup_x86_64_timers(const xinim::acpi::Discovery& acpi) {
    static xinim::hal::x86_64::Hpet hpet;

    g_lapic.init(acpi.lapic_mmio);
    xinim::kernel::set_timer_lapic(&g_lapic);

    hpet.init(acpi.hpet_mmio);
    xinim::time::monotonic_install(monotonic_from_hpet);

    uint32_t desired_hz = 100;
    auto result = xinim::time::calibrate_apic_with_hpet(g_lapic, hpet, desired_hz);

    g_lapic.stop_timer();
    g_lapic.setup_timer(32, result.initial_count, result.divider_pow2, true);
}
#endif

extern "C" void _start() {
    early_serial.init();
    kshell_serial.init();
    kputs("XINIM Kernel Booting...\n");

#ifdef XINIM_ARCH_X86_64
    xinim::arch::x86_64::fpu_init();
    xinim::arch::x86_64::g_cpu_features = xinim::arch::x86_64::cpu_detect_features();

    kputs("[cpu] Features:");
    if (xinim::arch::x86_64::g_cpu_features.aesni)  kputs(" AES-NI");
    if (xinim::arch::x86_64::g_cpu_features.sha_ni) kputs(" SHA-NI");
    if (xinim::arch::x86_64::g_cpu_features.avx2)   kputs(" AVX2");
    if (xinim::arch::x86_64::g_cpu_features.avx)    kputs(" AVX");
    if (xinim::arch::x86_64::g_cpu_features.sse42)  kputs(" SSE4.2");
    if (xinim::arch::x86_64::g_cpu_features.rdrand) kputs(" RDRAND");
    kputs("\n");
#endif

#ifdef XINIM_BOOT_LIMINE
    g_boot_info = xinim::boot::from_limine();

    xinim::acpi::Discovery acpi = xinim::acpi::probe(
        reinterpret_cast<uint64_t>(g_boot_info.acpi_rsdp),
        g_boot_info.hhdm_offset);

    #ifdef XINIM_ARCH_X86_64
    setup_x86_64_timers(acpi);
    #endif
#endif

    xinim::kernel::initialize_gdt();
    xinim::kernel::initialize_tss();
    xinim::arch::x86_64::idt::init();

    interrupts_init(early_serial, g_lapic);

    // PCI bus enumeration and device init
    kputs("[pci] Enumerating PCI bus...\n");
    xinim::pci::PCI::initialize();
    xinim::drivers::net::virtio_net_init();

    xinim::kernel::initialize_system_servers();

    kputs("XINIM is now running!\n");

    if (g_boot_info.cmdline && std::strstr(g_boot_info.cmdline, "debug_shell")) {
        kputs("Starting debug shell on COM2...\n");
        kshell_serial.shell();
    }

    xinim::kernel::schedule_forever();
}

int main() noexcept {
    _start();
    return 0;
}
