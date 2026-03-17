#pragma once

#include <stdint.h>

namespace xinim::i486::console {

void initialize() noexcept;
void write_char(char c) noexcept;
void write_string(const char* text) noexcept;
void tty_write_char(char c) noexcept;
void tty_write_string(const char* text) noexcept;
void debug_write_char(char c) noexcept;
void debug_write_string(const char* text) noexcept;
char debug_read_char() noexcept;
char tty_read_char() noexcept;
void tty_poll_input() noexcept;
bool tty_has_input() noexcept;
bool tty_try_read_char(char* out) noexcept;
void write_bool(bool value) noexcept;
void write_dec32(uint32_t value) noexcept;
void write_hex32(uint32_t value) noexcept;
void write_hex64(uint64_t value) noexcept;
void newline() noexcept;
uint32_t consume_pending_tty_signal() noexcept;

} // namespace xinim::i486::console
