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
#include "x86_64/staged_xash.hpp"
#include "bootfs.hpp"
#include <xinim/pci/pci.hpp>
#include "../drivers/net/virtio_net.hpp"
#include "../vfs/bootfs_promote.hpp"
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

static void kputs_u64(uint64_t value) {
    char buffer[32];
    int pos = 0;
    if (value == 0) {
        kputs("0");
        return;
    }
    while (value > 0 && pos < static_cast<int>(sizeof(buffer))) {
        buffer[pos++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (pos > 0) {
        early_serial.write_char(buffer[--pos]);
    }
}

static int seed_boot_modules_into_vfs(const xinim::boot::BootInfo& boot_info) {
    xinim::kernel::bootfs::initialize(boot_info);
    return vfs_promote_from_bootfs();
}

#ifdef XINIM_ARCH_X86_64
// Static LAPIC instance accessible to both setup_x86_64_timers and _start
static xinim::hal::x86_64::Lapic g_lapic;

static uint64_t monotonic_from_hpet() {
    static xinim::hal::x86_64::Hpet hpet;
    return hpet.counter() * 100; // Stub
}

static uintptr_t map_mmio_hhdm(uint64_t phys_addr) {
    if (phys_addr == 0) {
        return 0;
    }
    return static_cast<uintptr_t>(phys_addr + g_boot_info.hhdm_offset);
}

static void setup_x86_64_timers(const xinim::acpi::Discovery& acpi) {
    static xinim::hal::x86_64::Hpet hpet;
    const uintptr_t lapic_mmio = map_mmio_hhdm(acpi.lapic_mmio);
    const uintptr_t hpet_mmio = map_mmio_hhdm(acpi.hpet_mmio);

    if (lapic_mmio == 0) {
        kputs("[boot] No LAPIC MMIO from ACPI, skipping timer init\n");
        return;
    }
    if (hpet_mmio == 0) {
        kputs("[boot] No HPET MMIO from ACPI, skipping timer init\n");
        return;
    }

    kputs("[boot] Timer init: LAPIC\n");
    g_lapic.init(lapic_mmio);
    xinim::kernel::set_timer_lapic(&g_lapic);

    kputs("[boot] Timer init: HPET\n");
    hpet.init(hpet_mmio);
    hpet.enable(true);
    xinim::time::monotonic_install(monotonic_from_hpet);

    uint32_t desired_hz = 100;
    kputs("[boot] Timer init: calibrate APIC with HPET\n");
    auto result = xinim::time::calibrate_apic_with_hpet(g_lapic, hpet, desired_hz);

    g_lapic.stop_timer();
    g_lapic.setup_timer(32, result.initial_count, result.divider_pow2, true);
    kputs("[boot] Timer init complete\n");
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
    kputs("[boot] Importing Limine handoff\n");
    g_boot_info = xinim::boot::from_limine();

    kputs("[boot] cmdline: ");
    kputs(g_boot_info.cmdline != nullptr ? g_boot_info.cmdline : "(none)");
    kputs("\n");

    const bool debug_shell_requested =
        g_boot_info.cmdline && std::strstr(g_boot_info.cmdline, "debug_shell");

    if (debug_shell_requested) {
        kputs("[boot] Entering COM2 debug/emergency shell before advanced bring-up\n");
        kputs("[boot] If you attached after boot, press Enter once to redraw the prompt\n");
        (void)kshell_serial.shell(&g_boot_info, true);
        kputs("[boot] Leaving COM2 debug/emergency shell\n");
    }

    const int seeded_modules = seed_boot_modules_into_vfs(g_boot_info);
    kputs("[boot] Seeded boot modules into ramfs: ");
    kputs_u64(static_cast<uint64_t>(seeded_modules));
    kputs("\n");
    xinim::kernel::x86_64::set_staged_xash_boot_info(&g_boot_info);

    if (g_boot_info.has_framebuffer()) {
        kputs("[boot] Framebuffer ");
        kputs_u64(g_boot_info.framebuffer.width);
        kputs("x");
        kputs_u64(g_boot_info.framebuffer.height);
        kputs(" @ ");
        kputs_u64(g_boot_info.framebuffer.bpp);
        kputs("bpp\n");
    } else {
        kputs("[boot] No framebuffer handoff\n");
    }

    kputs("[boot] Probing ACPI\n");
    xinim::acpi::Discovery acpi = xinim::acpi::probe(
        g_boot_info.acpi_rsdp,
        g_boot_info.hhdm_offset);
    kputs("[boot] ACPI probe complete\n");

    #ifdef XINIM_ARCH_X86_64
    kputs("[boot] Starting x86_64 timer bring-up\n");
    setup_x86_64_timers(acpi);
    kputs("[boot] x86_64 timer bring-up complete\n");
    #endif
#endif

    kputs("[boot] Initializing GDT\n");
    xinim::kernel::initialize_gdt();
    kputs("[boot] Initializing TSS\n");
    xinim::kernel::initialize_tss();
    kputs("[boot] Initializing IDT\n");
    xinim::arch::x86_64::idt::init();

#ifdef XINIM_ARCH_X86_64
    kputs("[boot] Launching staged x86_64 shell before full interrupt bring-up\n");
    xinim::kernel::x86_64::run_staged_xash_init();
#endif

    kputs("[boot] Initializing interrupt handlers\n");
    interrupts_init(early_serial, g_lapic);
    xinim::kernel::initialize_syscall();

    // PCI bus enumeration and device init
    kputs("[pci] Enumerating PCI bus...\n");
    xinim::pci::PCI::initialize();
    xinim::drivers::net::virtio_net_init();

    xinim::kernel::initialize_system_servers();
    xinim::kernel::spawn_init_process("/bin/xash");

    kputs("XINIM is now running!\n");

    xinim::kernel::schedule_forever();
}

int main() noexcept {
    _start();
    return 0;
}
