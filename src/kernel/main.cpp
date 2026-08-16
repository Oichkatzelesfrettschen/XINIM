#include "../drivers/net/virtio_net.hpp"
#include "../mm/alloc.hpp"
#include "../vfs/bootfs_promote.hpp"
#include "acpi/acpi.hpp"
#include "arch/x86_64/cpu_features.hpp"
#include "arch/x86_64/fpu_init.hpp"
#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/realtime_clock.hpp"
#include "arch/x86_64/syscall_init.hpp"
#include "arch/x86_64/tss.hpp"
#include "bootfs.hpp"
#include "console.hpp"
#include "const.hpp"
#include "early/serial_16550.hpp"
#include "interrupts.hpp"
#include "panic.hpp"
#include "platform_traits.hpp"
#include "proc.hpp"
#include "server_spawn.hpp"
#include "time/calibrate.hpp"
#include "time/monotonic.hpp"
#include "timer.hpp"
#include "x86_64/staged_xash.hpp"

#include <cstring>
#include <xinim/arch/x86/mmio.hpp>
#include <xinim/boot/bootinfo.hpp>
#include <xinim/boot/limine_shim.hpp>
#include <xinim/drivers/ahci.hpp>
#include <xinim/drivers/e1000.hpp>
#include <xinim/pci/pci.hpp>

#ifdef XINIM_ARCH_X86_64
#include "hal/x86_64/hal/apic.hpp"
#include "hal/x86_64/hal/hpet.hpp"
#include "hal/x86_64/hal/ioapic.hpp"
#endif

extern xinim::early::Serial16550 early_serial;  // COM1: kernel log
extern xinim::early::Serial16550 kshell_serial; // COM2: kshell
static xinim::boot::BootInfo g_boot_info;

namespace xinim::boot {
    const BootInfo &get_info() {
        return g_boot_info;
    }
} // namespace xinim::boot

static void kputs(const char *s) {
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

static void kputs_hex_u64(uint64_t value) {
    static constexpr char kHexDigits[] = "0123456789ABCDEF";
    kputs("0x");

    bool emitted = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint8_t digit = static_cast<uint8_t>((value >> shift) & 0xFU);
        if (!emitted && digit == 0U && shift != 0) {
            continue;
        }
        emitted = true;
        early_serial.write_char(kHexDigits[digit]);
    }
}

static int seed_boot_modules_into_vfs(const xinim::boot::BootInfo &boot_info) {
    xinim::kernel::bootfs::initialize(boot_info);
    return vfs_promote_from_bootfs();
}

static void probe_x86_pci_feature_lanes() {
    bool found_e1000 = false;
    bool found_ahci = false;

    for (size_t index = 0; index < xinim::pci::PCI::get_device_count(); ++index) {
        const xinim::pci::PCIDevice *device = xinim::pci::PCI::get_device(index);
        if (device == nullptr || !device->is_valid()) {
            continue;
        }

        if (!found_e1000 && xinim::drivers::E1000Driver::matches_pci_device(*device)) {
            xinim::drivers::pci_binding::MappedBar bar{};
            if (xinim::drivers::pci_binding::bind_mmio_bar(*device, 0, bar)) {
                kputs("[pci] E1000 lane ready at BAR0 ");
                kputs_hex_u64(bar.physical);
                kputs(" size=");
                kputs_u64(bar.size);
                kputs("\n");
                found_e1000 = true;
            } else {
                kputs("[pci] E1000 detected but BAR0 MMIO binding failed\n");
            }
        }

        if (!found_ahci && xinim::drivers::AHCIDriver::matches_pci_device(*device)) {
            xinim::drivers::pci_binding::MappedBar bar{};
            if (xinim::drivers::pci_binding::bind_mmio_bar(*device, 5, bar)) {
                kputs("[pci] AHCI lane ready at BAR5 ");
                kputs_hex_u64(bar.physical);
                kputs(" size=");
                kputs_u64(bar.size);
                kputs("\n");
                found_ahci = true;
            } else {
                kputs("[pci] AHCI detected but BAR5 MMIO binding failed\n");
            }
        }
    }

    if (!found_e1000) {
        kputs("[pci] No supported E1000 lane detected\n");
    }
    if (!found_ahci) {
        kputs("[pci] No AHCI lane detected\n");
    }
}

#ifdef XINIM_ARCH_X86_64
// Static LAPIC instance accessible to both setup_x86_64_timers and _start
static xinim::hal::x86_64::Lapic g_lapic;
static xinim::hal::x86_64::Hpet g_hpet;
static xinim::hal::x86_64::IoApic g_ioapic;
static uint64_t g_hpet_period_femtoseconds = 0U;

static uint64_t monotonic_from_hpet() {
    const uint64_t counter = g_hpet.counter();
    constexpr uint64_t femtoseconds_per_nanosecond = 1000000U;
    return (counter / femtoseconds_per_nanosecond) * g_hpet_period_femtoseconds +
           ((counter % femtoseconds_per_nanosecond) * g_hpet_period_femtoseconds) /
               femtoseconds_per_nanosecond;
}

static void setup_x86_64_timers(const xinim::acpi::Discovery &acpi) {
    const uintptr_t lapic_mmio = xinim::arch::x86::mmio::map_physical(acpi.lapic_mmio);
    const uintptr_t hpet_mmio = xinim::arch::x86::mmio::map_physical(acpi.hpet_mmio);

    if (lapic_mmio == 0) {
        kputs("[boot] No LAPIC MMIO from ACPI, skipping timer init\n");
        return;
    }
    if (hpet_mmio == 0) {
        kputs("[boot] No HPET MMIO from ACPI, skipping timer init\n");
        return;
    }

    kputs("[boot] Timer init: LAPIC\n");
    kputs("[boot] LAPIC MMIO physical: ");
    kputs_hex_u64(acpi.lapic_mmio);
    kputs("\n");
    g_lapic.init(lapic_mmio);
    xinim::kernel::set_timer_lapic(&g_lapic);

    kputs("[boot] Timer init: HPET\n");
    kputs("[boot] HPET MMIO physical: ");
    kputs_hex_u64(acpi.hpet_mmio);
    kputs("\n");
    g_hpet.init(hpet_mmio);
    kputs("[boot] HPET period femtoseconds: ");
    kputs_u64(g_hpet.period_fs());
    kputs("\n");
    g_hpet_period_femtoseconds = g_hpet.period_fs();
    g_hpet.enable(true);
    kputs("[boot] HPET counter after enable: ");
    kputs_u64(g_hpet.counter());
    kputs("\n");
    xinim::time::monotonic_install(monotonic_from_hpet);

    uint32_t desired_hz = 100;
    kputs("[boot] Timer init: calibrate APIC with HPET\n");
    auto result = xinim::time::calibrate_apic_with_hpet(g_lapic, g_hpet, desired_hz);
    if (result.initial_count == 0U) {
        kpanic("APIC timer calibration against HPET did not converge");
    }

    g_lapic.stop_timer();
    g_lapic.setup_timer(32, result.initial_count, result.divider_pow2, true);
    kputs("[boot] Timer init complete\n");
}

static void setup_x86_64_serial_interrupts(const xinim::acpi::Discovery &acpi) {
    constexpr uint32_t kCom2IsaIrq = 3U;
    constexpr uint32_t kCom1IsaIrq = 4U;
    const uintptr_t ioapic_mmio = xinim::arch::x86::mmio::map_physical(acpi.ioapic_phys);
    if (ioapic_mmio == 0U) {
        kpanic("ACPI did not provide a usable I/O APIC mapping");
    }
    g_ioapic.init(ioapic_mmio, acpi.ioapic_gsi_base);
    const auto &com2_route = acpi.legacy_irq_routes[kCom2IsaIrq];
    const auto &com1_route = acpi.legacy_irq_routes[kCom1IsaIrq];
    if (!g_ioapic.redirect(com2_route.gsi, COM2_VECTOR, com2_route.level_triggered,
                           com2_route.active_low) ||
        !g_ioapic.redirect(com1_route.gsi, COM1_VECTOR, com1_route.level_triggered,
                           com1_route.active_low)) {
        kpanic("Serial IRQ routes do not fit the discovered I/O APIC");
    }
    kputs("[boot] I/O APIC serial IRQ3/IRQ4 routes installed\n");
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
    if (xinim::arch::x86_64::g_cpu_features.aesni)
        kputs(" AES-NI");
    if (xinim::arch::x86_64::g_cpu_features.sha_ni)
        kputs(" SHA-NI");
    if (xinim::arch::x86_64::g_cpu_features.avx2)
        kputs(" AVX2");
    if (xinim::arch::x86_64::g_cpu_features.avx)
        kputs(" AVX");
    if (xinim::arch::x86_64::g_cpu_features.sse42)
        kputs(" SSE4.2");
    if (xinim::arch::x86_64::g_cpu_features.rdrand)
        kputs(" RDRAND");
    kputs("\n");
#endif

#ifdef XINIM_BOOT_LIMINE
    kputs("[boot] Importing Limine handoff\n");
    g_boot_info = xinim::boot::from_limine();
    if (!mem_init_from_memory_map(g_boot_info.memory_map, g_boot_info.memory_map_entries)) {
        kpanic("Limine memory map cannot initialize the physical hole allocator");
    }

    kputs("[boot] cmdline: ");
    kputs(g_boot_info.cmdline != nullptr ? g_boot_info.cmdline : "(none)");
    kputs("\n");

    const bool debug_shell_requested =
        g_boot_info.cmdline && std::strstr(g_boot_info.cmdline, "debug_shell");
    const bool staged_shell_requested =
        g_boot_info.cmdline && std::strstr(g_boot_info.cmdline, "staged_shell");

    if (debug_shell_requested) {
        kputs("[boot] Entering COM2 debug/emergency shell before advanced bring-up\n");
        kputs("[boot] If you attached after boot, press Enter once to redraw the prompt\n");
        (void) kshell_serial.shell(&g_boot_info, true);
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
    xinim::acpi::Discovery acpi =
        xinim::acpi::probe(g_boot_info.acpi_rsdp, g_boot_info.hhdm_offset);
    kputs("[boot] ACPI probe complete\n");

#ifdef XINIM_ARCH_X86_64
    kputs("[boot] Starting x86_64 timer bring-up\n");
    setup_x86_64_timers(acpi);
    if (!xinim::kernel::x86_64::initialize_realtime_clock()) {
        kpanic("CMOS RTC did not provide a valid calendar snapshot");
    }
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
    if (staged_shell_requested) {
        kputs("[boot] Entering staged x86_64 diagnostic shell before interrupt bring-up\n");
        xinim::kernel::x86_64::run_staged_xash_init();
    }
#endif

    kputs("[boot] Initializing interrupt handlers\n");
    interrupts_init(early_serial, g_lapic);
#ifdef XINIM_ARCH_X86_64
    setup_x86_64_serial_interrupts(acpi);
#endif
    xinim::kernel::initialize_syscall();

    // PCI bus enumeration and device init
    kputs("[pci] Enumerating PCI bus...\n");
    xinim::pci::PCI::initialize();
    probe_x86_pci_feature_lanes();
    xinim::drivers::net::virtio_net_init();

    kputs("[boot] Deferring kernel-function server placeholders until real server ELFs exist\n");
    const char *const init_arguments[] = {"/bin/sh", "-i", nullptr};
    const char *const init_environment[] = {
        "PATH=/bin", "TERM=xinim",   "HOME=/",   "SHELL=/bin/sh", "ENV=/etc/mkshrc",
        "USER=root", "LOGNAME=root", "LC_ALL=C", nullptr,
    };
    if (xinim::kernel::spawn_init_process("/bin/sh", init_arguments, init_environment) != 0) {
        kpanic("PID 1 ELF64 load failed");
    }

    kputs("XINIM is now running!\n");

    xinim::kernel::schedule_forever();
}

int main() noexcept {
    _start();
    return 0;
}
