#include "shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "console.hpp"

namespace xinim::i486::shell {
namespace {

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}

void debug_newline() noexcept {
    console::debug_write_char('\n');
}

void debug_write_dec32(uint32_t value) noexcept {
    if (value == 0U) {
        console::debug_write_char('0');
        return;
    }

    char buffer[10];
    int index = 0;
    while (value != 0U) {
        buffer[index] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
        ++index;
    }

    while (index > 0) {
        --index;
        console::debug_write_char(buffer[index]);
    }
}

void debug_write_hex_nibble(uint8_t value) noexcept {
    if (value < 10U) {
        console::debug_write_char(static_cast<char>('0' + value));
        return;
    }

    console::debug_write_char(static_cast<char>('A' + (value - 10U)));
}

void debug_write_hex32(uint32_t value) noexcept {
    console::debug_write_string("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        debug_write_hex_nibble(static_cast<uint8_t>((value >> shift) & 0xFU));
    }
}

void debug_write_hex64(uint64_t value) noexcept {
    console::debug_write_string("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        debug_write_hex_nibble(static_cast<uint8_t>((value >> shift) & 0xFU));
    }
}

[[noreturn]] void halt_forever() noexcept {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

[[noreturn]] void reboot_machine() noexcept {
    auto outb = [](uint16_t port, uint8_t value) noexcept {
        asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    };
    auto inb = [](uint16_t port) noexcept -> uint8_t {
        uint8_t value = 0U;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    };

    while ((inb(0x64U) & 0x02U) != 0U) {
    }
    outb(0x64U, 0xFEU);
    halt_forever();
}

void print_help() noexcept {
    console::debug_write_string("Commands: help, info, cpu, mmap, fb, reboot, halt");
    debug_newline();
}

void print_info(const xinim::boot::BootInfo& info) noexcept {
    console::debug_write_string("XINIM i486 kernel shell");
    debug_newline();
    console::debug_write_string("protocol: multiboot2");
    debug_newline();
    console::debug_write_string("memory ranges: ");
    debug_write_dec32(static_cast<uint32_t>(info.memory_map_entries));
    debug_newline();
    console::debug_write_string("cmdline: ");
    console::debug_write_string(info.cmdline != nullptr ? info.cmdline : "(none)");
    debug_newline();
}

void print_cpu(const xinim::boot::BootInfo& info) noexcept {
    console::debug_write_string("cpuid: ");
    console::debug_write_string(info.cpu.has_cpuid ? "yes" : "no");
    debug_newline();
    console::debug_write_string("fpu: ");
    console::debug_write_string(info.cpu.has_fpu ? "yes" : "no");
    debug_newline();
}

void print_memory_map(const xinim::boot::BootInfo& info) noexcept {
    if (info.memory_map == nullptr || info.memory_map_entries == 0U) {
        console::debug_write_string("memory map unavailable");
        debug_newline();
        return;
    }

    const size_t limit = info.memory_map_entries < 8U ? info.memory_map_entries : 8U;
    for (size_t index = 0U; index < limit; ++index) {
        console::debug_write_string("mmap[");
        debug_write_dec32(static_cast<uint32_t>(index));
        console::debug_write_string("] base=");
        debug_write_hex64(info.memory_map[index].base);
        console::debug_write_string(" len=");
        debug_write_hex64(info.memory_map[index].length);
        console::debug_write_string(" type=");
        debug_write_dec32(info.memory_map[index].type);
        debug_newline();
    }
}

void print_framebuffer(const xinim::boot::BootInfo& info) noexcept {
    if (!info.has_framebuffer()) {
        console::debug_write_string("framebuffer: unavailable");
        debug_newline();
        return;
    }

    console::debug_write_string("framebuffer addr=");
    debug_write_hex32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.framebuffer.address)));
    console::debug_write_string(" size=");
    debug_write_dec32(static_cast<uint32_t>(info.framebuffer.width));
    console::debug_write_char('x');
    debug_write_dec32(static_cast<uint32_t>(info.framebuffer.height));
    console::debug_write_string(" pitch=");
    debug_write_dec32(static_cast<uint32_t>(info.framebuffer.pitch));
    console::debug_write_string(" bpp=");
    debug_write_dec32(info.framebuffer.bpp);
    debug_newline();
}

void print_prompt() noexcept {
    console::debug_write_string("xinim-i486> ");
}

} // namespace

void run(const xinim::boot::BootInfo& info) noexcept {
    console::debug_write_string("XINIM i486 shell ready");
    debug_newline();
    print_prompt();

    char buffer[128];
    size_t position = 0U;
    buffer[0] = '\0';

    for (;;) {
        const char input = console::debug_read_char();
        if (input == '\r' || input == '\n') {
            buffer[position] = '\0';
            debug_newline();

            if (position == 0U) {
                print_prompt();
                continue;
            }

            if (string_equals(buffer, "help")) {
                print_help();
            } else if (string_equals(buffer, "info")) {
                print_info(info);
            } else if (string_equals(buffer, "cpu")) {
                print_cpu(info);
            } else if (string_equals(buffer, "mmap")) {
                print_memory_map(info);
            } else if (string_equals(buffer, "fb")) {
                print_framebuffer(info);
            } else if (string_equals(buffer, "reboot")) {
                console::debug_write_string("Rebooting...");
                debug_newline();
                reboot_machine();
            } else if (string_equals(buffer, "halt")) {
                console::debug_write_string("Halting...");
                debug_newline();
                halt_forever();
            } else {
                console::debug_write_string("Unknown command: ");
                console::debug_write_string(buffer);
                debug_newline();
            }

            position = 0U;
            buffer[0] = '\0';
            print_prompt();
            continue;
        }

        if (input == '\b' || input == 127) {
            if (position != 0U) {
                --position;
                console::debug_write_string("\b \b");
            }
            continue;
        }

        if (position + 1U >= sizeof(buffer)) {
            continue;
        }

        buffer[position] = input;
        ++position;
        console::debug_write_char(input);
    }
}

} // namespace xinim::i486::shell
