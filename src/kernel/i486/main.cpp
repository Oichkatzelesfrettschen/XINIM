#include <stddef.h>
#include <stdint.h>

#include "console.hpp"
#include "bootfs.hpp"
#include "ring3.hpp"
#include "shell.hpp"
#include "../../vfs/bootfs_promote.hpp"
#include "xinim/boot/multiboot2_shim.hpp"

#ifndef XINIM_BOOT_LANE_NAME
#define XINIM_BOOT_LANE_NAME "i486"
#endif

namespace {

void write_key_value(const char* key, const char* value) noexcept {
    xinim::i486::console::write_string(key);
    xinim::i486::console::write_string(": ");
    xinim::i486::console::write_string(value);
    xinim::i486::console::newline();
}

void write_key_bool(const char* key, bool value) noexcept {
    xinim::i486::console::write_string(key);
    xinim::i486::console::write_string(": ");
    xinim::i486::console::write_bool(value);
    xinim::i486::console::newline();
}

void write_key_dec(const char* key, uint32_t value) noexcept {
    xinim::i486::console::write_string(key);
    xinim::i486::console::write_string(": ");
    xinim::i486::console::write_dec32(value);
    xinim::i486::console::newline();
}

void halt_forever() noexcept {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

} // namespace

extern "C" void xinim_i486_kmain(uint32_t magic, uint32_t info_addr) noexcept {
    xinim::i486::console::initialize();
    xinim::i486::console::write_string("XINIM " XINIM_BOOT_LANE_NAME " Booting");
    xinim::i486::console::newline();

    const xinim::boot::BootInfo info =
        xinim::boot::from_multiboot2(magic, static_cast<uintptr_t>(info_addr));

    if (info.protocol != xinim::boot::BootProtocol::Multiboot2) {
        write_key_value("boot protocol", "invalid");
        halt_forever();
    }

    write_key_value("boot protocol", "multiboot2");
    write_key_dec("memory ranges", static_cast<uint32_t>(info.memory_map_entries));
    write_key_bool("cpuid", info.cpu.has_cpuid);
    write_key_bool("fpu", info.cpu.has_fpu);
    write_key_bool("framebuffer", info.has_framebuffer());
    write_key_bool("acpi rsdp", info.acpi_rsdp != nullptr);

    if (info.cmdline != nullptr) {
        xinim::i486::console::write_string("cmdline: ");
        xinim::i486::console::write_string(info.cmdline);
        xinim::i486::console::newline();
    }

    if (info.memory_map != nullptr && info.memory_map_entries != 0U) {
        const size_t range_limit = info.memory_map_entries < 4U ? info.memory_map_entries : 4U;
        for (size_t index = 0U; index < range_limit; ++index) {
            xinim::i486::console::write_string("mmap[");
            xinim::i486::console::write_dec32(static_cast<uint32_t>(index));
            xinim::i486::console::write_string("] base=");
            xinim::i486::console::write_hex64(info.memory_map[index].base);
            xinim::i486::console::write_string(" len=");
            xinim::i486::console::write_hex64(info.memory_map[index].length);
            xinim::i486::console::write_string(" type=");
            xinim::i486::console::write_dec32(info.memory_map[index].type);
            xinim::i486::console::newline();
        }
    }

    if (info.has_framebuffer()) {
        xinim::i486::console::write_string("framebuffer addr=");
        xinim::i486::console::write_hex32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.framebuffer.address)));
        xinim::i486::console::write_string(" size=");
        xinim::i486::console::write_dec32(static_cast<uint32_t>(info.framebuffer.width));
        xinim::i486::console::write_char('x');
        xinim::i486::console::write_dec32(static_cast<uint32_t>(info.framebuffer.height));
        xinim::i486::console::newline();
    }

    xinim::i486::bootfs::initialize(info);
    const int promoted_entries = vfs_promote_from_bootfs();
    xinim::i486::console::write_string("bootfs promoted entries: ");
    xinim::i486::console::write_dec32(static_cast<uint32_t>(promoted_entries < 0 ? 0 : promoted_entries));
    xinim::i486::console::newline();

    xinim::i486::console::write_string("XINIM " XINIM_BOOT_LANE_NAME " bootstrap ready");
    xinim::i486::console::newline();

    if (!xinim::i486::ring3::launch_xash(info)) {
        xinim::i486::console::write_string("Ring 3 launch failed; starting rescue shell on COM2...");
        xinim::i486::console::newline();
        xinim::i486::shell::run(info);
    }

    halt_forever();
}
