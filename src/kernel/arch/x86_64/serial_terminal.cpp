#include "serial_terminal.hpp"

#include "../../early/serial_16550.hpp"
#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../signal.hpp"
#include "../../uaccess.hpp"
#include "../../unified_scheduler.hpp"
#include "signal_syscalls.hpp"

#include <cstddef>
#include <cstdint>

extern xinim::early::Serial16550 kshell_serial;

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr size_t kCanonicalCapacity = 512U;
        constexpr size_t kTransferCapacity = 256U;
        constexpr int64_t kBadDescriptor = -9;
        constexpr int64_t kBadAddress = -14;
        constexpr int64_t kInterrupted = -4;
        constexpr int64_t kInvalidArgument = -22;
        constexpr int64_t kNotTerminal = -25;
        constexpr int64_t kNoSuchProcess = -3;
        constexpr uint64_t kTcgets = 0x5401U;
        constexpr uint64_t kTcsets = 0x5402U;
        constexpr uint64_t kTcsetsw = 0x5403U;
        constexpr uint64_t kTcsetsf = 0x5404U;
        constexpr uint64_t kTcflsh = 0x540bU;
        constexpr uint64_t kTiocgpgrp = 0x540fU;
        constexpr uint64_t kTiocspgrp = 0x5410U;
        constexpr uint64_t kTiocgwinsz = 0x5413U;
        constexpr uint64_t kTiocswinsz = 0x5414U;
        constexpr uintptr_t kFlushInput = 0U;
        constexpr uintptr_t kFlushOutput = 1U;
        constexpr uintptr_t kFlushInputAndOutput = 2U;
        constexpr uint32_t kInputMapCarriageReturn = 0000400U;
        constexpr uint32_t kOutputPostProcess = 0000001U;
        constexpr uint32_t kOutputMapNewline = 0000004U;
        constexpr uint32_t kLocalSignals = 0000001U;
        constexpr uint32_t kLocalCanonical = 0000002U;
        constexpr uint32_t kLocalEcho = 0000010U;
        constexpr uint32_t kLocalEchoErase = 0000020U;
        constexpr uint32_t kLocalEchoKill = 0000040U;
        constexpr uint32_t kLocalStopOutput = 0000400U;
        constexpr uint32_t kLocalExtended = 0100000U;
        constexpr size_t kControlCharacterCount = 19U;
        constexpr size_t kInterruptCharacter = 0U;
        constexpr size_t kQuitCharacter = 1U;
        constexpr size_t kEraseCharacter = 2U;
        constexpr size_t kKillCharacter = 3U;
        constexpr size_t kEndOfFileCharacter = 4U;
        constexpr size_t kSuspendCharacter = 10U;

        struct TerminalAttributes {
            uint32_t input_flags;
            uint32_t output_flags;
            uint32_t control_flags;
            uint32_t local_flags;
            uint8_t line_discipline;
            uint8_t control_characters[kControlCharacterCount];
        };

        struct TerminalWindowSize {
            uint16_t rows;
            uint16_t columns;
            uint16_t pixel_width;
            uint16_t pixel_height;
        };

        static_assert(sizeof(TerminalAttributes) == 36U);
        static_assert(sizeof(TerminalWindowSize) == 8U);

        char g_canonical_line[kCanonicalCapacity]{};
        size_t g_canonical_size = 0U;
        size_t g_canonical_offset = 0U;
        bool g_end_of_file = false;
        bool g_interrupted = false;
        xinim::pid_t g_foreground_pgid = 1;
        xinim::pid_t g_controlling_sid = 1;
        TerminalAttributes g_terminal_attributes{
            kInputMapCarriageReturn,
            kOutputPostProcess | kOutputMapNewline,
            0000017U | 0000060U | 0000200U,
            kLocalSignals | kLocalCanonical | kLocalEcho | kLocalEchoErase | kLocalEchoKill |
                kLocalExtended,
            0U,
            {3U, 28U, 127U, 21U, 4U, 0U, 1U, 0U, 17U, 19U, 26U, 0U, 18U, 15U, 23U, 22U, 0U, 0U, 0U},
        };
        TerminalWindowSize g_terminal_window_size{24U, 80U, 0U, 0U};

        void flush_terminal_input() noexcept {
            g_canonical_size = 0U;
            g_canonical_offset = 0U;
            g_end_of_file = false;
            g_interrupted = false;
            char discarded = '\0';
            while (kshell_serial.try_read_char(discarded)) {}
        }

        void write_terminal_character(char value) noexcept {
            if (value == '\n' &&
                (g_terminal_attributes.output_flags & (kOutputPostProcess | kOutputMapNewline)) ==
                    (kOutputPostProcess | kOutputMapNewline)) {
                kshell_serial.write_char('\r');
            }
            kshell_serial.write_char(value);
        }

        [[nodiscard]] bool terminal_group_exists(xinim::pid_t pgid, xinim::pid_t sid) noexcept {
            for (int pid = 1; pid < MAX_PROCESSES; ++pid) {
                const ProcessControlBlock *process = find_process_by_pid(pid);
                if (process != nullptr && process->pgid == pgid && process->sid == sid &&
                    process->state != ProcessState::DEAD &&
                    process->state != ProcessState::ZOMBIE) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool send_terminal_signal(char input) noexcept {
            if ((g_terminal_attributes.local_flags & kLocalSignals) == 0U) {
                return false;
            }
            int signum = 0;
            if (static_cast<uint8_t>(input) ==
                g_terminal_attributes.control_characters[kInterruptCharacter]) {
                signum = SIGINT;
            } else if (static_cast<uint8_t>(input) ==
                       g_terminal_attributes.control_characters[kQuitCharacter]) {
                signum = SIGQUIT;
            } else if (static_cast<uint8_t>(input) ==
                       g_terminal_attributes.control_characters[kSuspendCharacter]) {
                signum = SIGTSTP;
            }
            if (signum == 0) {
                return false;
            }
            static_cast<void>(process_kill(-g_foreground_pgid, signum));
            kshell_serial.write("^\n");
            g_interrupted = true;
            return true;
        }

        [[nodiscard]] bool caller_is_foreground() noexcept {
            const ProcessControlBlock *process = get_current_process();
            return process != nullptr && process->pgid == g_foreground_pgid;
        }

        void wait_for_character(char &value) noexcept {
            while (!kshell_serial.try_read_char(value)) {
                // LAPIC ticks guarantee progress even before q35 IRQ3 has an IOAPIC
                // route. CLI is restored before the caller inspects shared RX state.
                asm volatile("sti\n\thlt\n\tcli" ::: "memory");
            }
        }

        void fill_canonical_line() noexcept {
            g_canonical_size = 0U;
            g_canonical_offset = 0U;
            g_end_of_file = false;
            g_interrupted = false;

            for (;;) {
                char input = '\0';
                wait_for_character(input);
                if (send_terminal_signal(input)) {
                    return;
                }
                if (input == '\r' &&
                    (g_terminal_attributes.input_flags & kInputMapCarriageReturn) != 0U) {
                    input = '\n';
                }
                if (input == '\r' || input == '\n') {
                    if (g_canonical_size < kCanonicalCapacity) {
                        g_canonical_line[g_canonical_size++] = '\n';
                    }
                    if ((g_terminal_attributes.local_flags & (kLocalEcho | kLocalEchoKill)) != 0U) {
                        write_terminal_character('\n');
                    }
                    return;
                }
                if (static_cast<uint8_t>(input) ==
                    g_terminal_attributes.control_characters[kEndOfFileCharacter]) {
                    g_end_of_file = g_canonical_size == 0U;
                    return;
                }
                if (static_cast<uint8_t>(input) ==
                    g_terminal_attributes.control_characters[kEraseCharacter]) {
                    if (g_canonical_size != 0U) {
                        --g_canonical_size;
                        if ((g_terminal_attributes.local_flags & (kLocalEcho | kLocalEchoErase)) !=
                            0U) {
                            kshell_serial.write("\b \b");
                        }
                    }
                    continue;
                }
                if (static_cast<uint8_t>(input) ==
                    g_terminal_attributes.control_characters[kKillCharacter]) {
                    while (g_canonical_size != 0U) {
                        --g_canonical_size;
                        if ((g_terminal_attributes.local_flags & kLocalEcho) != 0U) {
                            kshell_serial.write("\b \b");
                        }
                    }
                    continue;
                }
                const uint8_t raw = static_cast<uint8_t>(input);
                if ((raw < 0x20U && input != '\t') || raw > 0x7eU ||
                    g_canonical_size == kCanonicalCapacity) {
                    continue;
                }
                g_canonical_line[g_canonical_size++] = input;
                if ((g_terminal_attributes.local_flags & kLocalEcho) != 0U) {
                    write_terminal_character(input);
                }
            }
        }

    } // namespace

    int64_t serial_terminal_read(uint64_t descriptor, uintptr_t user_buffer,
                                 size_t count) noexcept {
        if (descriptor != 0U) {
            return kBadDescriptor;
        }
        if (count == 0U) {
            return 0;
        }
        if (!is_user_address(user_buffer, count)) {
            return kBadAddress;
        }
        if (!caller_is_foreground()) {
            const ProcessControlBlock *process = get_current_process();
            if (process != nullptr) {
                static_cast<void>(process_kill(-process->pgid, SIGTTIN));
            }
            return kInterrupted;
        }
        if ((g_terminal_attributes.local_flags & kLocalCanonical) == 0U) {
            char input = '\0';
            for (;;) {
                wait_for_character(input);
                if (send_terminal_signal(input)) {
                    return kInterrupted;
                }
                if (input == '\r' &&
                    (g_terminal_attributes.input_flags & kInputMapCarriageReturn) != 0U) {
                    input = '\n';
                }
                break;
            }
            if ((g_terminal_attributes.local_flags & kLocalEcho) != 0U) {
                write_terminal_character(input);
            }
            return copy_to_user(user_buffer, &input, 1U) == 0 ? 1 : kBadAddress;
        }
        if (g_canonical_offset == g_canonical_size) {
            fill_canonical_line();
        }
        if (g_interrupted) {
            g_interrupted = false;
            return kInterrupted;
        }
        if (g_end_of_file && g_canonical_size == 0U) {
            g_end_of_file = false;
            return 0;
        }

        const size_t available = g_canonical_size - g_canonical_offset;
        const size_t transferred = count < available ? count : available;
        if (copy_to_user(user_buffer, g_canonical_line + g_canonical_offset, transferred) != 0) {
            return kBadAddress;
        }
        g_canonical_offset += transferred;
        return static_cast<int64_t>(transferred);
    }

    int64_t serial_terminal_write(uint64_t descriptor, uintptr_t user_buffer,
                                  size_t count) noexcept {
        if (descriptor != 1U && descriptor != 2U) {
            return kBadDescriptor;
        }
        if (count == 0U) {
            return 0;
        }
        if (!is_user_address(user_buffer, count)) {
            return kBadAddress;
        }
        const ProcessControlBlock *process = get_current_process();
        if (process != nullptr && process->pgid != g_foreground_pgid &&
            (g_terminal_attributes.local_flags & kLocalStopOutput) != 0U) {
            static_cast<void>(process_kill(-process->pgid, SIGTTOU));
            return kInterrupted;
        }

        char transfer[kTransferCapacity];
        size_t written = 0U;
        while (written < count) {
            const size_t remaining = count - written;
            const size_t chunk = remaining < kTransferCapacity ? remaining : kTransferCapacity;
            if (copy_from_user(transfer, user_buffer + written, chunk) != 0) {
                return kBadAddress;
            }
            for (size_t index = 0U; index < chunk; ++index) {
                write_terminal_character(transfer[index]);
            }
            written += chunk;
        }
        return static_cast<int64_t>(written);
    }

    int64_t serial_terminal_ioctl(uint64_t request, uintptr_t argument) noexcept {
        ProcessControlBlock *process = get_current_process();
        if (process == nullptr) {
            return kNoSuchProcess;
        }
        switch (request) {
        case kTcgets:
            return copy_to_user(argument, &g_terminal_attributes, sizeof(g_terminal_attributes)) ==
                           0
                       ? 0
                       : kBadAddress;
        case kTcsets:
        case kTcsetsw:
        case kTcsetsf: {
            TerminalAttributes requested{};
            if (copy_from_user(&requested, argument, sizeof(requested)) != 0) {
                return kBadAddress;
            }
            g_terminal_attributes = requested;
            if (request == kTcsetsf) {
                flush_terminal_input();
            }
            return 0;
        }
        case kTcflsh:
            if (argument != kFlushInput && argument != kFlushOutput &&
                argument != kFlushInputAndOutput) {
                return kInvalidArgument;
            }
            if (argument == kFlushInput || argument == kFlushInputAndOutput) {
                flush_terminal_input();
            }
            return 0;
        case kTiocgpgrp:
            if (process->sid != g_controlling_sid) {
                return kNotTerminal;
            }
            return copy_to_user(argument, &g_foreground_pgid, sizeof(g_foreground_pgid)) == 0
                       ? 0
                       : kBadAddress;
        case kTiocspgrp: {
            if (process->sid != g_controlling_sid) {
                return kNotTerminal;
            }
            if (process->pgid != g_foreground_pgid) {
                static_cast<void>(process_kill(-process->pgid, SIGTTOU));
                return kInterrupted;
            }
            xinim::pid_t requested_pgid = 0;
            if (copy_from_user(&requested_pgid, argument, sizeof(requested_pgid)) != 0) {
                return kBadAddress;
            }
            if (requested_pgid <= 0) {
                return kInvalidArgument;
            }
            if (!terminal_group_exists(requested_pgid, g_controlling_sid)) {
                return kNoSuchProcess;
            }
            g_foreground_pgid = requested_pgid;
            return 0;
        }
        case kTiocgwinsz: {
            return copy_to_user(argument, &g_terminal_window_size,
                                sizeof(g_terminal_window_size)) == 0
                       ? 0
                       : kBadAddress;
        }
        case kTiocswinsz: {
            TerminalWindowSize requested{};
            if (copy_from_user(&requested, argument, sizeof(requested)) != 0) {
                return kBadAddress;
            }
            const bool changed = requested.rows != g_terminal_window_size.rows ||
                                 requested.columns != g_terminal_window_size.columns ||
                                 requested.pixel_width != g_terminal_window_size.pixel_width ||
                                 requested.pixel_height != g_terminal_window_size.pixel_height;
            g_terminal_window_size = requested;
            if (changed) {
                static_cast<void>(process_kill(-g_foreground_pgid, SIGWINCH));
            }
            return 0;
        }
        default:
            return kNotTerminal;
        }
    }

    bool serial_terminal_has_input() noexcept {
        return g_canonical_offset < g_canonical_size || g_end_of_file || kshell_serial.has_input();
    }

} // namespace xinim::kernel::x86_64
