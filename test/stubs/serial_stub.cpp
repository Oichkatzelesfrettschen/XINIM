/**
 * @file test/stubs/serial_stub.cpp
 * @brief No-op Serial16550 stub for host-side unit tests.
 *
 * WHY: elf_loader.cpp and similar kernel files call early_serial.write() for
 *      diagnostic output.  On the host build there is no I/O port hardware, so
 *      this stub satisfies the linker and silently drops all output.
 */

#include "../../src/kernel/early/serial_16550.hpp"

namespace xinim::early {

void Serial16550::init() {}
void Serial16550::write_char(char) {}
void Serial16550::write(const char*) {}
char Serial16550::read_char() { return '\0'; }
bool Serial16550::shell(const xinim::boot::BootInfo*, bool) { return false; }
void Serial16550::enable_rx_interrupt() {}
void Serial16550::isr_handler() {}
bool Serial16550::try_read_char(char&) { return false; }

void Serial16550::outb(std::uint16_t, std::uint8_t) const {}
std::uint8_t Serial16550::inb(std::uint16_t) const { return 0; }

} // namespace xinim::early

// Global early_serial instance required by elf_loader.cpp and other kernel files.
xinim::early::Serial16550 early_serial;

// ============================================================================
// VFS interface stubs (elf_loader.cpp: load_elf_binary / load_segment)
// ============================================================================

#include "../../src/kernel/vfs_interface.hpp"

namespace xinim::kernel {

void* vfs_lookup(const char*) { return nullptr; }

ssize_t vfs_read(void*, void*, size_t, uint64_t) { return -1; }

} // namespace xinim::kernel
